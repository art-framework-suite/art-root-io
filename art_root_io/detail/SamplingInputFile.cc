#include "art_root_io/detail/SamplingInputFile.h"

#include "art/Framework/Core/GroupSelector.h"
#include "art/Framework/Core/UpdateOutputCallbacks.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Services/System/DatabaseConnection.h"
#include "art/Persistency/Provenance/ModuleDescription.h"
#include "art/Persistency/Provenance/ProcessHistoryRegistry.h"
#include "art_root_io/RootDB/TKeyVFSOpenPolicy.h"
#include "art_root_io/RootDB/have_table.h"
#include "art_root_io/checkDictionaries.h"
#include "art_root_io/detail/SamplingDelayedReader.h"
#include "art_root_io/detail/dropBranch.h"
#include "art_root_io/detail/readFileIndex.h"
#include "art_root_io/detail/readMetadata.h"
#include "art_root_io/rootErrMsgs.h"
#include "canvas/Persistency/Common/Wrapper.h"
#include "canvas/Persistency/Provenance/BranchChildren.h"
#include "canvas/Persistency/Provenance/BranchDescription.h"
#include "canvas/Persistency/Provenance/BranchKey.h"
#include "canvas/Persistency/Provenance/SampledInfo.h"
#include "canvas/Persistency/Provenance/rootNames.h"
#include "canvas/Utilities/uniform_type_name.h"
#include "canvas_root_io/Streamers/ProductIDStreamer.h"
#include "fhiclcpp/ParameterSetRegistry.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "range/v3/view.hpp"

#include <set>
#include <string>

using EntriesForID_t = art::detail::SamplingInputFile::EntriesForID_t;
using ProductsForKey_t = art::detail::SamplingInputFile::ProductsForKey_t;
using namespace ranges;
using namespace std::string_literals;

namespace {
  TTree*
  get_tree(TFile& file, std::string const& treeName)
  {
    auto result = file.Get<TTree>(treeName.c_str());
    if (result == nullptr) {
      throw art::Exception{art::errors::FileReadError,
                           "An error occurred while trying to read "s +
                             file.GetName() + "\n"}
        << art::couldNotFindTree(treeName);
    }
    return result;
  }

  TTree*
  get_tree(TFile& file,
           std::string const& treeName,
           unsigned int const treeCacheSize,
           int64_t const treeMaxVirtualSize)
  {
    auto result = get_tree(file, treeName);
    result->SetCacheSize(static_cast<Long64_t>(treeCacheSize));
    if (treeMaxVirtualSize >= 0) {
      result->SetMaxVirtualSize(static_cast<Long64_t>(treeMaxVirtualSize));
    }
    return result;
  }
}

namespace art::detail {
  SamplingInputFile::SamplingInputFile(
    std::string const& dataset,
    std::string const& filename,
    double const weight,
    double const probability,
    EventID const& firstEvent,
    GroupSelectorRules const& groupSelectorRules,
    bool const dropDescendants,
    unsigned int const treeCacheSize,
    int64_t const treeMaxVirtualSize,
    int64_t const saveMemoryObjectThreshold,
    BranchDescription const& sampledEventInfoDesc,
    bool const compactRangeSets,
    std::map<BranchKey, BranchDescription>& oldKeyToSampledProductDescription,
    ModuleDescription const& md,
    bool const readIncomingParameterSets,
    UpdateOutputCallbacks& outputCallbacks)
    : dataset_{dataset}
    , file_{std::make_unique<TFile>(filename.c_str())}
    , weight_{weight}
    , probability_{probability}
    , firstEvent_{firstEvent}
    , saveMemoryObjectThreshold_{saveMemoryObjectThreshold}
    , sampledEventInfoDesc_{sampledEventInfoDesc}
    , compactRangeSets_{compactRangeSets}
  {
    auto metaDataTree = get_tree(*file_, rootNames::metaDataTreeName());

    // Read the ProcessHistory
    {
      auto pHistMap = detail::readMetadata<ProcessHistoryMap>(metaDataTree);
      ProcessHistoryRegistry::put(pHistMap);
    }

    // Read file format version
    fileFormatVersion_ = detail::readMetadata<FileFormatVersion>(metaDataTree);

    // Also need to check RootFileDB if we have one.
    if (fileFormatVersion_.value_ >= 5) {
      sqliteDB_ = ServiceHandle<DatabaseConnection>
      {
        } -> get<TKeyVFSOpenPolicy>("RootFileDB", file_.get());
      if (readIncomingParameterSets &&
          have_table(sqliteDB_->get(), "ParameterSets", dataset_)) {
        fhicl::ParameterSetRegistry::importFrom(sqliteDB_->get());
      }
    }

    // Read file index
    auto findexPtr = &fileIndex_;
    detail::readFileIndex(file_.get(), metaDataTree, findexPtr);
    fiIter_ = fileIndex_.begin();
    fiEnd_ = fileIndex_.end();

    // To support files that contain BranchIDLists
    BranchIDLists branchIDLists{};
    if (detail::readMetadata(metaDataTree, branchIDLists)) {
      branchIDLists_ =
        std::make_unique<BranchIDLists>(std::move(branchIDLists));
      configureProductIDStreamer(branchIDLists_.get());
    }

    // Event-level trees
    if (fileFormatVersion_.value_ < 15) {
      eventHistoryTree_ = get_tree(*file_, rootNames::eventHistoryTreeName());
      auto eventHistoryBranch = eventHistoryTree_->GetBranch(
        rootNames::eventHistoryBranchName().c_str());
      eventHistoryBranch->SetAddress(nullptr);
    }
    eventTree_ = get_tree(*file_,
                          BranchTypeToProductTreeName(InEvent),
                          treeCacheSize,
                          treeMaxVirtualSize);
    auxBranch_ =
      eventTree_->GetBranch(BranchTypeToAuxiliaryBranchName(InEvent).c_str());
    auxBranch_->SetAddress(nullptr);

    eventMetaTree_ = get_tree(*file_, BranchTypeToMetaDataTreeName(InEvent));
    productProvenanceBranch_ =
      eventMetaTree_->GetBranch(productProvenanceBranchName(InEvent).c_str());
    productProvenanceBranch_->SetAddress(nullptr);

    // Higher-level trees
    subRunTree_ = get_tree(*file_,
                           BranchTypeToProductTreeName(InSubRun),
                           treeCacheSize,
                           treeMaxVirtualSize);
    runTree_ = get_tree(*file_,
                        BranchTypeToProductTreeName(InRun),
                        treeCacheSize,
                        treeMaxVirtualSize);

    // Read the BranchChildren, necessary for dropping descendent products
    auto const branchChildren =
      detail::readMetadata<BranchChildren>(metaDataTree);

    // Read the ProductList and fill the cached product-list holder
    // skipping over entries with no branches.
    {
      auto const productListHolder =
        detail::readMetadata<ProductRegistry>(metaDataTree);

      auto descriptionsByID = productListHolder.productList_ | views::values |
                              views::transform([](auto const& pd) {
                                return std::make_pair(pd.productID(), pd);
                              }) |
                              to<ProductDescriptionsByID>();
      ;
      dropOnInput_(
        groupSelectorRules, branchChildren, dropDescendants, descriptionsByID);

      for (auto const& pd : descriptionsByID | views::values) {
        auto const bt = pd.branchType();
        auto branch =
          treeForBranchType_(bt)->GetBranch(pd.branchName().c_str());
        if (branch == nullptr) {
          // This situation can happen for dropped products registered in
          // files that were created with art 2.09 and 2.10.  To ensure
          // that we do not fill the product tables for these dropped
          // products, we skip over them.
          continue;
        }
        branch->SetAddress(nullptr);
        branches_.emplace(pd.productID(), input::BranchInfo{pd, branch});

        // A default-constructed BranchDescription object is initialized
        // with the PresentFromSource validity flag.  If we get this far,
        // then the branch is present in the file, and we need not adjust
        // the validity of the BranchDescription.
        productListHolder_.productList_.emplace(BranchKey{pd}, pd);
      }
    }

    // Insert sampled products for (sub)runs.
    auto& productList = productListHolder_.productList_;
    for (auto const& [key, pd] : productList) {
      auto const bt = pd.branchType();
      assert(bt != NumBranchTypes);
      if (bt == InEvent || bt == InResults)
        continue;

      std::string const wrapped_product{"art::Sampled<" +
                                        pd.producedClassName() + ">"};
      ProcessConfiguration const pc{"SampledFrom" + pd.processName(),
                                    md.parameterSetID(),
                                    md.releaseVersion()};
      BranchDescription sampledDesc{
        bt,
        pd.moduleLabel(),
        pc.processName(),
        uniform_type_name(wrapped_product),
        pd.productInstanceName(),
        pc.parameterSetID(),
        pc.id(),
        BranchDescription::Transients::PresentFromSource,
        false,
        false};
      oldKeyToSampledProductDescription.emplace(key, std::move(sampledDesc));
    }

    // Register newly created data product
    checkDictionaries(sampledEventInfoDesc_);
    productList.emplace(BranchKey{sampledEventInfoDesc_},
                        sampledEventInfoDesc_);

    presentEventProducts_ =
      ProductTable{make_product_descriptions(productList), InEvent};
    auto tables = ProductTables::invalid();
    tables.get(InEvent) = presentEventProducts_;
    outputCallbacks.invoke(tables);
  }

  bool
  SamplingInputFile::updateEventEntry_(FileIndex::const_iterator& it,
                                       input::EntryNumber& entry) const
  {
    for (; it != fiEnd_; ++it) {
      if (it->getEntryType() != FileIndex::kEvent ||
          it->eventID < firstEvent_) {
        continue;
      }

      entry = it->entry;
      return true;
    }

    return false;
  }

  EventID
  SamplingInputFile::nextEvent() const
  {
    auto local_it = fiIter_;
    input::EntryNumber entry;
    return updateEventEntry_(local_it, entry) ? local_it->eventID :
                                                EventID::invalidEvent();
  }

  bool
  SamplingInputFile::readyForNextEvent()
  {
    bool const another_one = updateEventEntry_(fiIter_, currentEventEntry_);
    if (another_one) {
      ++fiIter_;
    }
    return another_one;
  }

  namespace {
    constexpr FileIndex::EntryType
    to_entry_type(BranchType const bt)
    {
      switch (bt) {
      case InRun:
        return FileIndex::kRun;
      case InSubRun:
        return FileIndex::kSubRun;
      case InEvent:
        return FileIndex::kEvent;
      default:
        return FileIndex::kEnd;
      }
    }
  }

  EntriesForID_t
  SamplingInputFile::treeEntries(BranchType const bt)
  {
    EntriesForID_t entries;
    for (auto const& element : fileIndex_) {
      if (element.getEntryType() != to_entry_type(bt)) {
        continue;
      }
      entries[element.eventID].push_back(element.entry);
    }
    return entries;
  }

  ProductsForKey_t
  SamplingInputFile::productsFor(EntriesForID_t const& entries,
                                 BranchType const bt)
  {
    ProductsForKey_t result;
    for (auto const& [id, tree_entries] : entries) {
      SamplingDelayedReader const reader{fileFormatVersion_,
                                         sqliteDB_->get(),
                                         tree_entries,
                                         branches_,
                                         nullptr,
                                         saveMemoryObjectThreshold_,
                                         branchIDLists_.get(),
                                         bt,
                                         id,
                                         compactRangeSets_};
      for (auto const& [key, bd] : productListHolder_.productList_) {
        if (bd.branchType() != bt)
          continue;
        auto rs = RangeSet::invalid();
        auto product = reader.getProduct(bd.productID(), bd.wrappedName(), rs);
        result[key].emplace(id.subRunID(), std::move(product));
      }
    }
    return result;
  }

  std::unique_ptr<EventPrincipal>
  SamplingInputFile::readEvent(EventID const& eventID,
                               ProcessConfigurations const& sampled_pcs,
                               ProcessConfiguration const& current_pc)
  {
    auto const on_disk_aux = auxiliaryForEntry_(currentEventEntry_);

    ProcessHistory ph;
    bool found [[maybe_unused]]{false};
    if (fileFormatVersion_.value_ < 15) {
      auto history = historyForEntry_(currentEventEntry_);
      found = ProcessHistoryRegistry::get(history.processHistoryID(), ph);
    } else {
      found = ProcessHistoryRegistry::get(on_disk_aux.processHistoryID(), ph);
    }
    assert(found);

    for (auto const& sampled_pc : sampled_pcs) {
      ph.push_back(sampled_pc);
    }
    ph.push_back(current_pc);
    auto const id = ph.id();
    ProcessHistoryRegistry::emplace(id, ph);

    // We do *not* keep the on-disk EventID for the primary event; we
    // instead create it as an event product.
    EventAuxiliary const aux{eventID,
                             on_disk_aux.time(),
                             on_disk_aux.isRealData(),
                             on_disk_aux.experimentType(),
                             id};
    auto const on_disk_id = on_disk_aux.id();
    auto ep = std::make_unique<EventPrincipal>(
      aux,
      current_pc,
      &presentEventProducts_,
      std::make_unique<SamplingDelayedReader>(
        fileFormatVersion_,
        sqliteDB_->get(),
        input::EntryNumbers{currentEventEntry_},
        branches_,
        productProvenanceBranch_,
        -1 /* saveMemoryObjectThreshold */,
        branchIDLists_.get(),
        InEvent,
        on_disk_id,
        false));

    // Place sampled EventID onto event
    auto sampledEventID = std::make_unique<SampledEventInfo>(
      SampledEventInfo{on_disk_id, dataset_, weight_, probability_});
    auto wp =
      std::make_unique<Wrapper<SampledEventInfo>>(std::move(sampledEventID));
    auto const& pd = sampledEventInfoDesc_;
    ep->put(pd,
            std::make_unique<ProductProvenance const>(pd.productID(),
                                                      productstatus::present()),
            std::move(wp),
            std::make_unique<RangeSet>(RangeSet::invalid()));
    return ep;
  }

  void
  SamplingInputFile::dropOnInput_(GroupSelectorRules const& rules,
                                  BranchChildren const& children,
                                  bool const dropDescendants,
                                  ProductDescriptionsByID& descriptions)
  {
    // FIXME: The functionality below is a near duplicate to that
    //        provided in RootInput.

    GroupSelector const groupSelector{rules, descriptions};
    // Do drop on input. On the first pass, just fill in a set of
    // branches to be dropped.

    // FIXME: ProductID does not include BranchType, so this algorithm
    //        may be problematic.
    std::set<ProductID> branchesToDrop;
    for (auto const& pd : descriptions | views::values) {
      // We explicitly do not support results products for the Sampling
      // input source.
      if (pd.branchType() == InResults || !groupSelector.selected(pd)) {
        if (dropDescendants) {
          children.appendToDescendants(pd.productID(), branchesToDrop);
        } else {
          branchesToDrop.insert(pd.productID());
        }
      }
    }
    // On this pass, actually drop the branches.
    auto branchesToDropEnd = branchesToDrop.cend();
    for (auto I = descriptions.begin(), E = descriptions.end(); I != E;) {
      auto const& pd = I->second;
      bool drop = branchesToDrop.find(pd.productID()) != branchesToDropEnd;
      if (!drop) {
        ++I;
        checkDictionaries(pd);
        continue;
      }
      if (groupSelector.selected(pd)) {
        mf::LogWarning("SamplingInputFile")
          << "Branch '" << pd.branchName()
          << "' is being dropped from the input\n"
          << "of file '" << file_->GetName()
          << "' because it is dependent on a branch\n"
          << "that was explicitly dropped.\n";
      }
      dropBranch(treeForBranchType_(pd.branchType()), pd.branchName());
      auto icopy = I++;
      descriptions.erase(icopy);
    }
  }

  TTree*
  SamplingInputFile::treeForBranchType_(BranchType const bt) const
  {
    switch (bt) {
    case InEvent:
      return eventTree_;
    case InSubRun:
      return subRunTree_;
    case InRun:
      return runTree_;
    default: {
      throw Exception{errors::LogicError}
        << "Cannot call treeForBranchType_ for a branch type of " << bt;
    }
    }
  }

  EventAuxiliary
  SamplingInputFile::auxiliaryForEntry_(input::EntryNumber const entry)
  {
    auto aux = std::make_unique<EventAuxiliary>();
    auto pAux = aux.get();
    auxBranch_->SetAddress(&pAux);
    eventTree_->LoadTree(entry);
    input::getEntry(auxBranch_, entry);
    return *aux;
  }

  History
  SamplingInputFile::historyForEntry_(input::EntryNumber const entry)
  {
    // We could consider doing delayed reading, but because we have to
    // store this History object in a different tree than the event
    // data tree, this is too hard to do in this first version.
    History history;
    auto pHistory = &history;
    auto eventHistoryBranch =
      eventHistoryTree_->GetBranch(rootNames::eventHistoryBranchName().c_str());
    if (!eventHistoryBranch) {
      throw Exception{errors::DataCorruption}
        << "Failed to find history branch in event history tree.\n";
    }
    eventHistoryBranch->SetAddress(&pHistory);
    input::getEntry(eventHistoryTree_, entry);
    return history;
  }
}
