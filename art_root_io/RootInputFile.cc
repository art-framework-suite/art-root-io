#include "art_root_io/RootInputFile.h"
// vim: set sw=2 expandtab :

#include "art/Framework/Core/GroupSelector.h"
#include "art/Framework/Core/ProcessingLimits.h"
#include "art/Framework/Core/UpdateOutputCallbacks.h"
#include "art/Framework/Principal/ClosedRangeSetHandler.h"
#include "art/Framework/Principal/EventPrincipal.h"
#include "art/Framework/Principal/OpenRangeSetHandler.h"
#include "art/Framework/Principal/RunPrincipal.h"
#include "art/Framework/Principal/SubRunPrincipal.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Services/Registry/ServiceRegistry.h"
#include "art/Framework/Services/System/DatabaseConnection.h"
#include "art/Framework/Services/System/FileCatalogMetadata.h"
#include "art/Persistency/Provenance/ProcessHistoryRegistry.h"
#include "art_root_io/DuplicateChecker.h"
#include "art_root_io/FastCloningEnabled.h"
#include "art_root_io/GetFileFormatEra.h"
#include "art_root_io/Inputfwd.h"
#include "art_root_io/RootDB/TKeyVFSOpenPolicy.h"
#include "art_root_io/RootDelayedReader.h"
#include "art_root_io/RootFileBlock.h"
#include "art_root_io/checkDictionaries.h"
#include "art_root_io/detail/getObjectRequireDict.h"
#include "art_root_io/detail/readFileIndex.h"
#include "art_root_io/detail/readMetadata.h"
#include "art_root_io/detail/resolveRangeSet.h"
#include "canvas/Persistency/Provenance/BranchChildren.h"
#include "canvas/Persistency/Provenance/BranchDescription.h"
#include "canvas/Persistency/Provenance/BranchType.h"
#include "canvas/Persistency/Provenance/Compatibility/History.h"
#include "canvas/Persistency/Provenance/ParameterSetBlob.h"
#include "canvas/Persistency/Provenance/ParameterSetMap.h"
#include "canvas/Persistency/Provenance/ParentageRegistry.h"
#include "canvas/Persistency/Provenance/ProductID.h"
#include "canvas/Persistency/Provenance/ProductList.h"
#include "canvas/Persistency/Provenance/RunID.h"
#include "canvas/Persistency/Provenance/rootNames.h"
#include "canvas/Utilities/Exception.h"
#include "canvas_root_io/Streamers/ProductIDStreamer.h"
#include "canvas_root_io/Utilities/DictionaryChecker.h"
#include "fhiclcpp/ParameterSet.h"
#include "fhiclcpp/ParameterSetRegistry.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "range/v3/view.hpp"

#include "TBranch.h"
#include "TFile.h"
#include "TLeaf.h"
#include "TTree.h"

#include <cassert>
#include <string>
#include <utility>

#include "sqlite3.h"

namespace {

  sqlite3*
  db_or_nullptr(std::unique_ptr<cet::sqlite::Connection> const& db)
  {
    return db.get() == nullptr ? nullptr : static_cast<sqlite3*>(*db);
  }

  bool
  have_table(sqlite3* db, std::string const& table, std::string const& filename)
  {
    bool result = false;
    sqlite3_stmt* stmt = nullptr;
    std::string const ddl{
      "select 1 from sqlite_master where type='table' and name='" + table +
      "';"};
    auto rc = sqlite3_prepare_v2(db, ddl.c_str(), -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
      switch (rc = sqlite3_step(stmt)) {
      case SQLITE_ROW:
        result = true; // Found the table.
        [[fallthrough]];
      case SQLITE_DONE:
        rc = SQLITE_OK; // No such table.
        break;
      default:
        break;
      }
    }
    rc = sqlite3_finalize(stmt);
    if (rc != SQLITE_OK) {
      throw art::Exception{art::errors::FileReadError}
        << "Unexpected status (" << rc
        << ") when interrogating SQLite3 DB in file " << filename << ":\n"
        << sqlite3_errmsg(db) << '\n';
    }
    return result;
  }

} // unnamed namespace

namespace art {

  RootInputFile::RootInputTree::~RootInputTree() = default;

  RootInputFile::RootInputTree::RootInputTree(cet::exempt_ptr<TFile> filePtr,
                                              BranchType const branchType,
                                              bool const missingOK /*=false*/)
  {
    if (filePtr) {
      tree_ =
        filePtr->Get<TTree>(BranchTypeToProductTreeName(branchType).c_str());
      metaTree_ =
        filePtr->Get<TTree>(BranchTypeToMetaDataTreeName(branchType).c_str());
    }
    if (tree_) {
      auxBranch_ =
        tree_->GetBranch(BranchTypeToAuxiliaryBranchName(branchType).c_str());
      nEntries_ = tree_->GetEntries();
    }
    if (metaTree_) {
      productProvenanceBranch_ =
        metaTree_->GetBranch(productProvenanceBranchName(branchType).c_str());
    }
    if (!missingOK && !isValid()) {
      throw Exception(errors::FileReadError)
        << "RootInputTree for branch type " << BranchTypeToString(branchType)
        << " could not be initialized correctly from input file.\n";
    }
  }

  TBranch*
  RootInputFile::RootInputTree::auxBranch() const
  {
    return auxBranch_;
  }

  RootInputFile::RootInputTree::EntryNumber
  RootInputFile::RootInputTree::nEntries() const
  {
    return nEntries_;
  }

  TTree*
  RootInputFile::RootInputTree::tree() const
  {
    return tree_;
  }

  TTree*
  RootInputFile::RootInputTree::metaTree() const
  {
    return metaTree_;
  }

  RootInputFile::RootInputTree::BranchMap const&
  RootInputFile::RootInputTree::branches() const
  {
    return branches_;
  }

  TBranch*
  RootInputFile::RootInputTree::productProvenanceBranch() const
  {
    return productProvenanceBranch_;
  }

  bool
  RootInputFile::RootInputTree::isValid() const
  {
    if ((metaTree_ == nullptr) || (metaTree_->GetNbranches() == 0)) {
      return tree_ && auxBranch_ && (tree_->GetNbranches() == 1);
    }
    return tree_ && auxBranch_ && metaTree_ && productProvenanceBranch_;
  }

  void
  RootInputFile::RootInputTree::addBranch(BranchDescription const& bd)
  {
    assert(isValid());
    TBranch* branch = tree_->GetBranch(bd.branchName().c_str());
    assert(bd.present() == (branch != nullptr));
    assert(bd.dropped() == (branch == nullptr));
    branches_.emplace(bd.productID(), input::BranchInfo{bd, branch});
  }

  void
  RootInputFile::RootInputTree::dropBranch(std::string const& branchName)
  {
    TBranch* branch = tree_->GetBranch(branchName.c_str());
    if (branch == nullptr) {
      return;
    }
    TObjArray* leaves = tree_->GetListOfLeaves();
    if (leaves == nullptr) {
      return;
    }
    int entries = leaves->GetEntries();
    for (int i = 0; i < entries; ++i) {
      TLeaf* leaf = reinterpret_cast<TLeaf*>((*leaves)[i]);
      if (leaf == nullptr) {
        continue;
      }
      TBranch* br = leaf->GetBranch();
      if (br == nullptr) {
        continue;
      }
      if (br->GetMother() == branch) {
        leaves->Remove(leaf);
      }
    }
    leaves->Compress();
    tree_->GetListOfBranches()->Remove(branch);
    tree_->GetListOfBranches()->Compress();
    delete branch;
    branch = nullptr;
  }

  namespace {
    input::EntryNumber
    next_event(FileIndex::const_iterator it,
               FileIndex::const_iterator const end)
    {
      do {
        ++it;
      } while (it != end && it->getEntryType() != FileIndex::kEvent);
      return it != end ? it->entry : FileIndex::Element::invalid;
    }
  }

  RootInputFile::~RootInputFile() = default;

  RootInputFile::RootInputFile(
    std::string const& fileName,
    ProcessConfiguration const& processConfiguration,
    std::unique_ptr<TFile>&& filePtr,
    EventID const& origEventID,
    unsigned int const eventsToSkip,
    bool const compactSubRunRanges,
    unsigned int const treeCacheSize,
    int64_t const treeMaxVirtualSize,
    int64_t const saveMemoryObjectThreshold,
    bool const delayedReadEventProducts,
    bool const delayedReadSubRunProducts,
    bool const delayedReadRunProducts,
    ProcessingLimits const& limits,
    bool const noEventSort,
    GroupSelectorRules const& groupSelectorRules,
    bool const dropDescendants,
    bool const readIncomingParameterSets,
    UpdateOutputCallbacks& outputCallbacks,
    secondary_reader_t openSecondaryFile,
    std::shared_ptr<DuplicateChecker> duplicateChecker)
    : fileName_{fileName}
    , processConfiguration_{processConfiguration}
    , filePtr_{std::move(filePtr)}
    , origEventID_{origEventID}
    , eventsToSkip_{eventsToSkip}
    , compactSubRunRanges_{compactSubRunRanges}
    , treePointers_{{makeInputTree(InEvent, false),
                     makeInputTree(InSubRun, false),
                     makeInputTree(InRun, false),
                     makeInputTree(InResults, true)}}
    , delayedReadEventProducts_{delayedReadEventProducts}
    , delayedReadSubRunProducts_{delayedReadSubRunProducts}
    , delayedReadRunProducts_{delayedReadRunProducts}
    , processingLimits_{limits}
    , noEventSort_{noEventSort}
    , readFromSecondaryFile_{openSecondaryFile}
    , duplicateChecker_{duplicateChecker}
    , saveMemoryObjectThreshold_{saveMemoryObjectThreshold}
  {
    if (treeMaxVirtualSize >= 0) {
      eventTree().tree()->SetMaxVirtualSize(
        static_cast<Long64_t>(treeMaxVirtualSize));
    }
    if (treeMaxVirtualSize >= 0) {
      subRunTree().tree()->SetMaxVirtualSize(
        static_cast<Long64_t>(treeMaxVirtualSize));
    }
    if (treeMaxVirtualSize >= 0) {
      runTree().tree()->SetMaxVirtualSize(
        static_cast<Long64_t>(treeMaxVirtualSize));
    }
    eventTree().tree()->SetCacheSize(static_cast<Long64_t>(treeCacheSize));
    subRunTree().tree()->SetCacheSize(static_cast<Long64_t>(treeCacheSize));
    runTree().tree()->SetCacheSize(static_cast<Long64_t>(treeCacheSize));
    if (resultsTree().isValid()) {
      if (treeMaxVirtualSize >= 0) {
        resultsTree().tree()->SetMaxVirtualSize(
          static_cast<Long64_t>(treeMaxVirtualSize));
      }
      resultsTree().tree()->SetCacheSize(static_cast<Long64_t>(treeCacheSize));
    }
    // Retrieve the metadata tree.
    auto metaDataTree =
      filePtr_->Get<TTree>(rootNames::metaDataTreeName().c_str());
    if (!metaDataTree) {
      throw Exception{errors::FileReadError}
        << couldNotFindTree(rootNames::metaDataTreeName());
    }
    metaDataTree->SetCacheSize(static_cast<Long64_t>(treeCacheSize));
    using namespace art::rootNames;
    fileFormatVersion_ = detail::readMetadata<FileFormatVersion>(metaDataTree);
    // Read file index
    auto findexPtr = &fileIndex_;

    detail::readFileIndex(filePtr_.get(), metaDataTree, findexPtr);
    // To support files that contain BranchIDLists
    BranchIDLists branchIDLists;
    if (detail::readMetadata(metaDataTree, branchIDLists)) {
      branchIDLists_ =
        std::make_unique<BranchIDLists>(std::move(branchIDLists));
      configureProductIDStreamer(branchIDLists_.get());
    }
    // Read the ParameterSets if there are any on a branch.
    {
      ParameterSetMap psetMap;
      if (readIncomingParameterSets &&
          detail::readMetadata(metaDataTree, psetMap)) {
        // Merge into the hashed registries.
        for (auto const& blob : psetMap | ranges::views::values) {
          auto const pset = fhicl::ParameterSet::make(blob.pset_);
          // Note ParameterSet::id() has the side effect of making
          // sure the parameter set *has* an ID.
          pset.id();
          fhicl::ParameterSetRegistry::put(pset);
        }
      }
    }
    // Read the ProcessHistory
    {
      auto pHistMap = detail::readMetadata<ProcessHistoryMap>(metaDataTree);
      ProcessHistoryRegistry::put(pHistMap);
    }
    // Check the, "Era" of the input file (new since art v0.5.0). If it
    // does not match what we expect we cannot read the file. Required
    // since we reset the file versioning since forking off from
    // CMS. Files written by art prior to v0.5.0 will *also* not be
    // readable because they do not have this datum and because the run,
    // subrun and event-number handling has changed significantly.
    std::string const& expected_era = getFileFormatEra();
    if (fileFormatVersion_.era_ != expected_era) {
      throw Exception{errors::FileReadError}
        << "Can only read files written during the \"" << expected_era
        << "\" era: "
        << "Era of "
        << "\"" << fileName_ << "\" was "
        << (fileFormatVersion_.era_.empty() ?
              "not set" :
              ("set to \"" + fileFormatVersion_.era_ + "\" "))
        << ".\n";
    }
    // Also need to check RootFileDB if we have one.
    if (fileFormatVersion_.value_ >= 5) {
      sqliteDB_ = ServiceHandle<DatabaseConnection>()->get<TKeyVFSOpenPolicy>(
        "RootFileDB", filePtr_.get());
      if (readIncomingParameterSets &&
          have_table(*sqliteDB_, "ParameterSets", fileName_)) {
        fhicl::ParameterSetRegistry::importFrom(*sqliteDB_);
      }
      if (ServiceRegistry::isAvailable<FileCatalogMetadata>() &&
          have_table(*sqliteDB_, "FileCatalog_metadata", fileName_)) {
        sqlite3_stmt* stmt{nullptr};
        sqlite3_prepare_v2(*sqliteDB_,
                           "SELECT Name, Value from FileCatalog_metadata;",
                           -1,
                           &stmt,
                           nullptr);
        std::vector<std::pair<std::string, std::string>> md;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
          std::string const name{
            reinterpret_cast<char const*>(sqlite3_column_text(stmt, 0))};
          std::string const value{
            reinterpret_cast<char const*>(sqlite3_column_text(stmt, 1))};
          md.emplace_back(name, value);
        }
        int const finalize_status = sqlite3_finalize(stmt);
        if (finalize_status != SQLITE_OK) {
          throw Exception{errors::SQLExecutionError}
            << "Unexpected status (" << finalize_status
            << ") from DB status cleanup:\n"
            << sqlite3_errmsg(*sqliteDB_) << '\n';
        }
        ServiceHandle<FileCatalogMetadata>()->setMetadataFromInput(md);
      }
    }
    validateFile();
    // Read the parentage tree.  Old format files are handled
    // internally in readParentageTree().
    readParentageTree(treeCacheSize);
    initializeDuplicateChecker();
    if (noEventSort_) {
      fileIndex_.sortBy_Run_SubRun_EventEntry();
    }
    fiIter_ = fileIndex_.begin();
    fiBegin_ = fileIndex_.begin();
    fiEnd_ = fileIndex_.end();
    readEventHistoryTree(treeCacheSize);

    // Read the ProductList
    // -- The 'false' value signifies that we do not check for a
    //    dictionary here.  The reason is that the BranchDescription
    //    class has an enumeration data member, and current checking
    //    for enum dictionaries is problematic.
    auto productList =
      detail::readMetadata<ProductRegistry>(metaDataTree, false).productList_;

    auto const branchChildren =
      detail::readMetadata<BranchChildren>(metaDataTree);

    // Create product table for present products
    presentProducts_ = ProductTables{make_product_descriptions(productList)};
    dropOnInput(
      groupSelectorRules, branchChildren, dropDescendants, presentProducts_);

    // Adjust validity of BranchDescription objects: if the branch
    // does not exist in the input file, but its BranchDescription is
    // still persisted to disk (and read here), the product must be
    // marked as "dropped".  See notes in RootOutputFile.cc
    // (particularly for branchesWithStoredHistory_ insertion) to see
    // how this can happen.  We then add the branch to the tree using
    // the updated validity of the product.
    auto set_validity_then_add_branch = [this](BranchType const bt) {
      for (auto& pd :
           presentProducts_.get(bt).descriptions | ranges::views::values) {
        if (treePointers_[bt]->tree()->GetBranch(pd.branchName().c_str()) ==
            nullptr) {
          pd.setValidity(BranchDescription::Transients::Dropped);
        } else {
          // Only check dictionaries for products that are actually readable.
          checkDictionaries(pd);
        }
        treePointers_[bt]->addBranch(pd);
      }
    };
    for_each_branch_type(set_validity_then_add_branch);

    // Invoke output callbacks with adjusted BranchDescription
    // validity values.
    outputCallbacks.invoke(presentProducts_);

    // Determine if this file is fast clonable.
    fastClonable_ = setIfFastClonable();

    // Check if dictionaries exist for the auxiliary objects
    root::DictionaryChecker checker{};
    checker.checkDictionaries<EventAuxiliary>();
    checker.checkDictionaries<SubRunAuxiliary>();
    checker.checkDictionaries<RunAuxiliary>();
    checker.checkDictionaries<ResultsAuxiliary>();
    checker.reportMissingDictionaries();

    // FIXME: This probably is unnecessary!
    configureProductIDStreamer();
  }

  std::unique_ptr<RootInputFile::RootInputTree>
  RootInputFile::makeInputTree(BranchType const bt, bool const missingOK) const
  {
    assert(filePtr_);
    return std::make_unique<RootInputTree>(filePtr_.get(), bt, missingOK);
  }

  template <typename ID>
  bool
  RootInputFile::setEntry(ID const& id, bool const exact /*= true*/)
  {
    fiIter_ = fileIndex_.findPosition(id, exact);
    return fiIter_ != fiEnd_;
  }

  template bool RootInputFile::setEntry<RunID>(RunID const& id, bool);
  template bool RootInputFile::setEntry<SubRunID>(SubRunID const& id, bool);
  template bool RootInputFile::setEntry<EventID>(EventID const& id, bool);

  RootInputFile::RootInputTree const&
  RootInputFile::eventTree() const
  {
    return *treePointers_[InEvent];
  }

  RootInputFile::RootInputTree const&
  RootInputFile::subRunTree() const
  {
    return *treePointers_[InSubRun];
  }

  RootInputFile::RootInputTree const&
  RootInputFile::runTree() const
  {
    return *treePointers_[InRun];
  }

  RootInputFile::RootInputTree const&
  RootInputFile::resultsTree() const
  {
    return *treePointers_[InResults];
  }

  RootInputFile::RootInputTree&
  RootInputFile::eventTree()
  {
    return *treePointers_[InEvent];
  }

  RootInputFile::RootInputTree&
  RootInputFile::subRunTree()
  {
    return *treePointers_[InSubRun];
  }

  RootInputFile::RootInputTree&
  RootInputFile::runTree()
  {
    return *treePointers_[InRun];
  }

  RootInputFile::RootInputTree&
  RootInputFile::resultsTree()
  {
    return *treePointers_[InResults];
  }

  template <typename Aux>
  Aux
  RootInputFile::getAuxiliary(EntryNumber const entry) const
  {
    Aux result{};
    auto auxbr = treePointers_[Aux::branch_type]->auxBranch();
    auto pAux = &result;
    auxbr->SetAddress(&pAux);
    input::getEntry(auxbr, entry);
    return result;
  }

  detail::RangeSetInfo
  RootInputFile::resolveInfo(BranchType const bt,
                             unsigned const range_set_id) const
  {
    assert(sqliteDB_);
    return detail::resolveRangeSetInfo(
      *sqliteDB_, fileName_, bt, range_set_id, compactSubRunRanges_);
  }

  template <typename Aux>
  std::pair<Aux, std::unique_ptr<RangeSetHandler>>
  RootInputFile::getAuxiliary(EntryNumbers const& entries) const
  {
    auto auxResult = getAuxiliary<Aux>(entries[0]);
    if (fileFormatVersion_.value_ < 9) {
      return {std::move(auxResult),
              std::make_unique<OpenRangeSetHandler>(auxResult.run())};
    }

    auto rangeSetInfo = resolveInfo(Aux::branch_type, auxResult.rangeSetID());
    for (auto i = entries.cbegin() + 1, e = entries.cend(); i != e; ++i) {
      auto tmpAux = getAuxiliary<Aux>(*i);
      auxResult.mergeAuxiliary(tmpAux);
      rangeSetInfo.update(resolveInfo(Aux::branch_type, tmpAux.rangeSetID()),
                          compactSubRunRanges_);
    }
    auxResult.setRangeSetID(-1u); // Range set of new auxiliary is invalid
    return {
      std::move(auxResult),
      std::make_unique<ClosedRangeSetHandler>(resolveRangeSet(rangeSetInfo))};
  }

  std::string const&
  RootInputFile::fileName() const
  {
    return fileName_;
  }

  void
  RootInputFile::setToLastEntry()
  {
    fiIter_ = fiEnd_;
  }

  void
  RootInputFile::nextEntry()
  {
    ++fiIter_;
  }

  void
  RootInputFile::previousEntry()
  {
    --fiIter_;
  }

  void
  RootInputFile::advanceEntry(size_t n)
  {
    while (n-- != 0) {
      nextEntry();
    }
  }

  unsigned int
  RootInputFile::eventsToSkip() const
  {
    return eventsToSkip_;
  }

  std::shared_ptr<FileIndex>
  RootInputFile::fileIndexSharedPtr() const
  {
    return fileIndexSharedPtr_;
  }

  void
  RootInputFile::readParentageTree(unsigned int const treeCacheSize)
  {
    //
    //  Auxiliary routine for the constructor.
    //
    auto parentageTree =
      filePtr_->Get<TTree>(rootNames::parentageTreeName().c_str());
    if (!parentageTree) {
      throw Exception{errors::FileReadError}
        << couldNotFindTree(rootNames::parentageTreeName());
    }
    parentageTree->SetCacheSize(static_cast<Long64_t>(treeCacheSize));
    auto idBuffer = root::getObjectRequireDict<ParentageID>();
    auto pidBuffer = &idBuffer;
    parentageTree->SetBranchAddress(rootNames::parentageIDBranchName().c_str(),
                                    &pidBuffer);

    auto parentageBuffer = root::getObjectRequireDict<Parentage>();
    auto pParentageBuffer = &parentageBuffer;
    parentageTree->SetBranchAddress(rootNames::parentageBranchName().c_str(),
                                    &pParentageBuffer);

    // Fill the registry
    for (EntryNumber i = 0, numEntries = parentageTree->GetEntries();
         i < numEntries;
         ++i) {
      input::getEntry(parentageTree, i);
      if (idBuffer != parentageBuffer.id()) {
        throw Exception{errors::DataCorruption}
          << "Corruption of Parentage tree detected.\n";
      }
      ParentageRegistry::emplace(parentageBuffer.id(), parentageBuffer);
    }

    parentageTree->SetBranchAddress(rootNames::parentageIDBranchName().c_str(),
                                    nullptr);
    parentageTree->SetBranchAddress(rootNames::parentageBranchName().c_str(),
                                    nullptr);
  }

  EventID
  RootInputFile::eventIDForFileIndexPosition() const
  {
    if (fiIter_ == fiEnd_) {
      return EventID{};
    }
    return fiIter_->eventID;
  }

  FastCloningEnabled

  RootInputFile::setIfFastClonable() const
  {
    FastCloningEnabled enabled;
    if (readFromSecondaryFile_) {
      enabled.disable("Reading from secondary file.");
    }
    if (!fileIndex_.allEventsInEntryOrder()) {
      enabled.disable("Events are not in entry order.");
    }
    if (eventsToSkip_ != 0) {
      enabled.disable("The events-to-skip option has been specified.");
    }
    if ((processingLimits_.remainingEvents() >= 0) &&
        (eventTree().nEntries() > processingLimits_.remainingEvents())) {
      enabled.disable("There are fewer events to process than are present in "
                      "the event tree.");
    }
    if ((processingLimits_.remainingSubRuns() >= 0) &&
        (subRunTree().nEntries() > processingLimits_.remainingSubRuns())) {
      enabled.disable("There are fewer subruns to process than are present in "
                      "the subrun tree.");
    }
    if (processingLimits_.processingMode() !=
        InputSource::RunsSubRunsAndEvents) {
      enabled.disable("Processing mode does not process all events.");
    }
    // Find entry for first event in file.
    auto it = fiBegin_;
    while ((it != fiEnd_) && (it->getEntryType() != FileIndex::kEvent)) {
      ++it;
    }
    if (it == fiEnd_) {
      enabled.disable("No event found in file index of input file.");
    }
    if (it->eventID < origEventID_) {
      enabled.disable(
        "Starting event does not include first event in input file.");
    }
    return enabled;
  }

  std::unique_ptr<FileBlock>
  RootInputFile::createFileBlock()
  {
    return std::make_unique<RootFileBlock>(
      fileFormatVersion_,
      fileName_,
      readResults(),
      cet::make_exempt_ptr(eventTree().tree()),
      fastClonable_);
  }

  FileIndex::EntryType
  RootInputFile::getEntryType() const
  {
    if (fiIter_ == fiEnd_) {
      return FileIndex::kEnd;
    }
    return fiIter_->getEntryType();
  }

  FileIndex::EntryType
  RootInputFile::getNextEntryTypeWanted()
  {
    auto entryType = getEntryType();
    if (entryType == FileIndex::kEnd) {
      return FileIndex::kEnd;
    }
    RunID currentRun(fiIter_->eventID.runID());
    if (!currentRun.isValid()) {
      return FileIndex::kEnd;
    }
    if (entryType == FileIndex::kRun) {
      // Skip any runs before the first run specified
      if (currentRun < origEventID_.runID()) {
        fiIter_ = fileIndex_.findPosition(origEventID_.runID(), false);
        return getNextEntryTypeWanted();
      }
      return FileIndex::kRun;
    }
    if (processingLimits_.processingMode() == InputSource::Runs) {
      fiIter_ = fileIndex_.findPosition(
        currentRun.isValid() ? currentRun.next() : currentRun, false);
      return getNextEntryTypeWanted();
    }
    SubRunID const& currentSubRun = fiIter_->eventID.subRunID();
    if (entryType == FileIndex::kSubRun) {
      // Skip any subRuns before the first subRun specified
      if ((currentRun == origEventID_.runID()) &&
          (currentSubRun < origEventID_.subRunID())) {
        fiIter_ = fileIndex_.findSubRunOrRunPosition(origEventID_.subRunID());
        return getNextEntryTypeWanted();
      }
      return FileIndex::kSubRun;
    }
    if (processingLimits_.processingMode() == InputSource::RunsAndSubRuns) {
      fiIter_ = fileIndex_.findSubRunOrRunPosition(currentSubRun.next());
      return getNextEntryTypeWanted();
    }
    assert(entryType == FileIndex::kEvent);
    // Skip any events before the first event specified
    if (fiIter_->eventID < origEventID_) {
      fiIter_ = fileIndex_.findPosition(origEventID_);
      return getNextEntryTypeWanted();
    }
    if (duplicateChecker_.get() && duplicateChecker_->isDuplicateAndCheckActive(
                                     fiIter_->eventID, fileName_)) {
      nextEntry();
      return getNextEntryTypeWanted();
    }
    if (eventsToSkip_ == 0) {
      return FileIndex::kEvent;
    }
    // We have specified a count of events to skip, keep skipping
    // events in this subRun block until we reach the end of the
    // subRun block or the full count of the number of events to skip.
    while ((eventsToSkip_ != 0) && (fiIter_ != fiEnd_) &&
           (getEntryType() == FileIndex::kEvent)) {
      nextEntry();
      --eventsToSkip_;
      while ((eventsToSkip_ != 0) && (fiIter_ != fiEnd_) &&
             (fiIter_->getEntryType() == FileIndex::kEvent) &&
             duplicateChecker_.get() &&
             duplicateChecker_->isDuplicateAndCheckActive(fiIter_->eventID,
                                                          fileName_)) {
        nextEntry();
      }
    }
    return getNextEntryTypeWanted();
  }

  void
  RootInputFile::validateFile()
  {
    if (!fileFormatVersion_.isValid()) {
      fileFormatVersion_.value_ = 0;
    }
    if (!eventTree().isValid()) {
      throw Exception{errors::DataCorruption}
        << "'Events' tree is corrupted or not present\n"
        << "in the input file.\n";
    }
    if (fileIndex_.empty()) {
      throw Exception{errors::FileReadError}
        << "FileIndex information is missing for the input file.\n";
    }
  }

  void
  RootInputFile::close()
  {
    filePtr_->Close();
  }

  void
  RootInputFile::fillHistory(EntryNumber const entry, History& history)
  {
    // We could consider doing delayed reading, but because we have to
    // store this History object in a different tree than the event
    // data tree, this is too hard to do in this first version.
    assert(eventHistoryTree_);
    auto pHistory = &history;
    auto eventHistoryBranch =
      eventHistoryTree_->GetBranch(rootNames::eventHistoryBranchName().c_str());
    if (!eventHistoryBranch) {
      throw Exception{errors::DataCorruption}
        << "Failed to find history branch in event history tree.\n";
    }
    eventHistoryBranch->SetAddress(&pHistory);
    input::getEntry(eventHistoryTree_, entry);
  }

  int
  RootInputFile::skipEvents(int offset)
  {
    while ((offset > 0) && (fiIter_ != fiEnd_)) {
      if (fiIter_->getEntryType() == FileIndex::kEvent) {
        --offset;
      }
      nextEntry();
    }
    while ((offset < 0) && (fiIter_ != fiBegin_)) {
      previousEntry();
      if (fiIter_->getEntryType() == FileIndex::kEvent) {
        ++offset;
      }
    }
    while ((fiIter_ != fiEnd_) &&
           (fiIter_->getEntryType() != FileIndex::kEvent)) {
      nextEntry();
    }
    return offset;
  }

  // readEvent() is responsible for creating, and setting up, the
  // EventPrincipal.
  //
  //   1. create an EventPrincipal with a unique EventID
  //   2. For each entry in the provenance, put in one Group,
  //      holding the Provenance for the corresponding EDProduct.
  //   3. set up the caches in the EventPrincipal to know about this
  //      Group.
  //
  // We do *not* create the EDProduct instance (the equivalent of
  // reading the branch containing this EDProduct). That will be done
  // by the Delayed Reader, when it is asked to do so.
  std::unique_ptr<EventPrincipal>
  RootInputFile::readEvent()
  {
    assert(fiIter_ != fiEnd_);
    assert(fiIter_->getEntryType() == FileIndex::kEvent);
    assert(fiIter_->eventID.runID().isValid());

    auto ep = readEventWithID(fiIter_->eventID);
    assert(ep);
    assert(ep->run() == fiIter_->eventID.run());
    assert(ep->eventID().subRunID() == fiIter_->eventID.subRunID());
    nextEntry();
    return ep;
  }

  std::unique_ptr<EventPrincipal>
  RootInputFile::readEventWithID(EventID const& eventID)
  {
    if (fiIter_ == fiEnd_ and not setEntry(eventID)) {
      return nullptr;
    }

    if (eventID != fiIter_->eventID and not setEntry(eventID)) {
      return nullptr;
    }

    auto const [entryNumbers, lastInSubRun] = getEntryNumbers(InEvent);
    assert(size(entryNumbers) == 1ull);

    auto const entry = entryNumbers[0];
    auto orig_event_aux = getAuxiliary<EventAuxiliary>(entry);
    auto event_aux = overrideAuxiliary(std::move(orig_event_aux), entry);
    if (fiIter_->eventID != event_aux.eventID()) {
      throw Exception{errors::LogicError}
        << "There is a mismatch in the file's index and the event "
           "auxiliary.\n\n"
        << "  File index ID ........ " << fiIter_->eventID << "\n\n    vs."
        << "\n\n  Event auxiliary ID ... " << event_aux.eventID()
        << "\n\nThis file and any of its decendents are likely corrupt.\n"
        << "Contact artists@fnal.gov for more information.\n";
    }

    auto ep = std::make_unique<EventPrincipal>(
      event_aux,
      processConfiguration_,
      &presentProducts_.get(InEvent),
      std::make_unique<RootDelayedReader>(fileFormatVersion_,
                                          nullptr,
                                          entryNumbers,
                                          &eventTree().branches(),
                                          eventTree().productProvenanceBranch(),
                                          saveMemoryObjectThreshold_,
                                          readFromSecondaryFile_,
                                          branchIDLists_.get(),
                                          InEvent,
                                          event_aux.eventID(),
                                          compactSubRunRanges_),
      lastInSubRun);
    if (!delayedReadEventProducts_) {
      ep->readImmediate();
    }
    return ep;
  }

  std::unique_ptr<RangeSetHandler>
  RootInputFile::runRangeSetHandler()
  {
    return std::move(runRangeSetHandler_);
  }

  std::unique_ptr<RunPrincipal>
  RootInputFile::readRun()
  {
    return readRunWithID(fiIter_->eventID.runID(), true);
  }

  std::unique_ptr<RunPrincipal>
  RootInputFile::readRunWithID(RunID const id, bool const thenAdvanceToNextRun)
  {
    if (fiIter_ == fiEnd_ and not setEntry(id)) {
      return nullptr;
    }

    if (fiIter_->eventID.runID() != id and not setEntry(id)) {
      return nullptr;
    }

    assert(fiIter_ != fiEnd_);
    assert(fiIter_->getEntryType() == FileIndex::kRun);
    assert(fiIter_->eventID.runID().isValid());

    auto const entryNumbers = getEntryNumbers(InRun).first;
    auto&& [orig_auxiliary, rs] = getAuxiliary<RunAuxiliary>(entryNumbers);
    runRangeSetHandler_ = std::move(rs);
    assert(orig_auxiliary.id() == fiIter_->eventID.runID());

    auto run_aux = overrideAuxiliary(std::move(orig_auxiliary));
    auto rp = std::make_unique<RunPrincipal>(
      run_aux,
      processConfiguration_,
      &presentProducts_.get(InRun),
      std::make_unique<RootDelayedReader>(fileFormatVersion_,
                                          db_or_nullptr(sqliteDB_),
                                          entryNumbers,
                                          &runTree().branches(),
                                          runTree().productProvenanceBranch(),
                                          saveMemoryObjectThreshold_,
                                          readFromSecondaryFile_,
                                          nullptr,
                                          InRun,
                                          fiIter_->eventID,
                                          compactSubRunRanges_));
    if (!delayedReadRunProducts_) {
      rp->readImmediate();
    }
    if (thenAdvanceToNextRun) {
      advanceEntry(entryNumbers.size());
    }
    return rp;
  }

  std::unique_ptr<RangeSetHandler>
  RootInputFile::subRunRangeSetHandler()
  {
    return std::move(subRunRangeSetHandler_);
  }

  std::unique_ptr<SubRunPrincipal>
  RootInputFile::readSubRun()
  {
    return readSubRunWithID(fiIter_->eventID.subRunID(), true);
  }

  std::unique_ptr<SubRunPrincipal>
  RootInputFile::readSubRunWithID(SubRunID const id,
                                  bool const thenAdvanceToNextSubRun)
  {
    if (fiIter_ == fiEnd_ and not setEntry(id)) {
      return nullptr;
    }

    if (fiIter_->eventID.subRunID() != id and not setEntry(id)) {
      return nullptr;
    }

    assert(fiIter_ != fiEnd_);
    assert(fiIter_->getEntryType() == FileIndex::kSubRun);

    auto const entryNumbers = getEntryNumbers(InSubRun).first;
    auto&& [orig_auxiliary, rs] = getAuxiliary<SubRunAuxiliary>(entryNumbers);
    subRunRangeSetHandler_ = std::move(rs);
    assert(orig_auxiliary.id() == fiIter_->eventID.subRunID());

    auto subrun_aux = overrideAuxiliary(std::move(orig_auxiliary));
    auto srp = std::make_unique<SubRunPrincipal>(
      subrun_aux,
      processConfiguration_,
      &presentProducts_.get(InSubRun),
      std::make_unique<RootDelayedReader>(
        fileFormatVersion_,
        db_or_nullptr(sqliteDB_),
        entryNumbers,
        &subRunTree().branches(),
        subRunTree().productProvenanceBranch(),
        saveMemoryObjectThreshold_,
        readFromSecondaryFile_,
        nullptr,
        InSubRun,
        fiIter_->eventID,
        compactSubRunRanges_));
    if (!delayedReadSubRunProducts_) {
      srp->readImmediate();
    }
    if (thenAdvanceToNextSubRun) {
      advanceEntry(entryNumbers.size());
    }
    return srp;
  }

  EventAuxiliary
  RootInputFile::overrideAuxiliary(EventAuxiliary event_aux,
                                   EntryNumber const entry)
  {
    // Older file versions have process history IDs stored in the History tree.
    if (fileFormatVersion_.value_ < 15) {
      auto history = std::make_unique<History>();
      fillHistory(entry, *history);
      event_aux.setProcessHistoryID(history->processHistoryID());
    }
    return event_aux;
  }

  template <typename Aux>
  Aux
  RootInputFile::overrideAuxiliary(Aux auxiliary) const
  {
    if (auxiliary.beginTime() == Timestamp::invalidTimestamp()) {
      // The auxiliary did not contain a valid timestamp.  Take it
      // from the next event, if there is one.
      if (auto const entry = next_event(fiIter_, fiEnd_);
          entry != FileIndex::Element::invalid) {
        auto const next_event_aux = getAuxiliary<EventAuxiliary>(entry);
        return auxiliary.duplicateWith(next_event_aux.time());
      }
    }
    return auxiliary;
  }

  void
  RootInputFile::readEventHistoryTree(unsigned int treeCacheSize)
  {
    if (fileFormatVersion_.value_ >= 15) {
      // The process history ID is stored directly in the
      // EventAuxiliary object for file formats 15 and above.
      return;
    }

    // Read in the event history tree, if we have one...
    eventHistoryTree_ =
      filePtr_->Get<TTree>(rootNames::eventHistoryTreeName().c_str());
    if (!eventHistoryTree_) {
      throw Exception{errors::DataCorruption}
        << "Failed to find the event history tree.\n";
    }
    eventHistoryTree_->SetCacheSize(static_cast<Long64_t>(treeCacheSize));
  }

  void
  RootInputFile::initializeDuplicateChecker()
  {
    if (duplicateChecker_.get() == nullptr) {
      return;
    }
    if (eventTree().nEntries()) {
      // FIXME: We don't initialize the duplicate checker if there are
      // no events!
      auto event_aux = getAuxiliary<EventAuxiliary>(0);
      duplicateChecker_->init(event_aux.isRealData(), fileIndex_);
    }
  }

  std::pair<RootInputFile::EntryNumbers, bool>
  RootInputFile::getEntryNumbers(BranchType const bt)
  {
    EntryNumbers enumbers;
    if (fiIter_ == fiEnd_) {
      return {enumbers, true};
    }
    auto const eid = fiIter_->eventID;
    auto iter = fiIter_;
    for (; (iter != fiEnd_) && (iter->eventID == eid); ++iter) {
      enumbers.push_back(iter->entry);
    }
    if ((bt == InEvent) && (enumbers.size() > 1ul)) {
      throw Exception{errors::FileReadError} << "File " << fileName_
                                             << " has multiple entries for\n"
                                             << eid << '\n';
    }
    bool const lastInSubRun{(iter == fiEnd_) ||
                            (iter->eventID.subRun() != eid.subRun())};
    return {enumbers, lastInSubRun};
  }

  void
  RootInputFile::dropOnInput(GroupSelectorRules const& rules,
                             BranchChildren const& children,
                             bool const dropDescendants,
                             ProductTables& tables)
  {
    auto dropOnInputForBranchType =
      [this, &rules, &children, dropDescendants, &tables](BranchType const bt) {
        auto& prodList = tables.get(bt).descriptions;

        // This is the selector for drop on input.
        GroupSelector const groupSelector{rules, prodList};
        // Do drop on input. On the first pass, just fill in a set of
        // branches to be dropped.
        std::set<ProductID> branchesToDrop;
        for (auto const& pd : prodList | ranges::views::values) {
          if (!groupSelector.selected(pd)) {
            if (dropDescendants) {
              children.appendToDescendants(pd.productID(), branchesToDrop);
            } else {
              branchesToDrop.insert(pd.productID());
            }
          }
        }

        // On this pass, actually drop the branches.
        auto branchesToDropEnd = branchesToDrop.cend();
        for (auto I = prodList.begin(), E = prodList.end(); I != E;) {
          auto const& bd = I->second;
          bool drop = branchesToDrop.find(bd.productID()) != branchesToDropEnd;
          if (!drop) {
            ++I;
            continue;
          }
          if (groupSelector.selected(bd)) {
            mf::LogWarning("RootInputFile")
              << "Branch '" << bd.branchName()
              << "' is being dropped from the input\n"
              << "of file '" << fileName_
              << "' because it is dependent on a branch\n"
              << "that was explicitly dropped.\n";
          }
          treePointers_[bt]->dropBranch(bd.branchName());
          auto icopy = I++;
          prodList.erase(icopy);
        }
      };
    for_each_branch_type(dropOnInputForBranchType);
  }

  std::unique_ptr<ResultsPrincipal>
  RootInputFile::readResults()
  {
    if (!resultsTree().isValid()) {
      return std::make_unique<ResultsPrincipal>(
        ResultsAuxiliary{}, processConfiguration_, nullptr);
    }

    EntryNumbers const entryNumbers{0};
    return std::make_unique<ResultsPrincipal>(
      getAuxiliary<ResultsAuxiliary>(entryNumbers.front()),
      processConfiguration_,
      &presentProducts_.get(InResults),
      std::make_unique<RootDelayedReader>(
        fileFormatVersion_,
        nullptr,
        entryNumbers,
        &resultsTree().branches(),
        resultsTree().productProvenanceBranch(),
        saveMemoryObjectThreshold_,
        readFromSecondaryFile_,
        nullptr,
        InResults,
        EventID{},
        compactSubRunRanges_));
  }

} // namespace art
