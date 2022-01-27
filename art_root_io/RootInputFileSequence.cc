#include "art_root_io/RootInputFileSequence.h"
// vim: set sw=2:

#include "art/Framework/Core/FileBlock.h"
#include "art/Framework/IO/Catalog/FileCatalog.h"
#include "art/Framework/IO/Catalog/InputFileCatalog.h"
#include "art/Framework/IO/detail/logFileAction.h"
#include "art/Framework/Principal/EventPrincipal.h"
#include "art/Framework/Principal/RunPrincipal.h"
#include "art/Framework/Principal/SubRunPrincipal.h"
#include "art/Utilities/Globals.h"
#include "art_root_io/RootInputFile.h"
#include "art_root_io/setup.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "TFile.h"

#include <ctime>
#include <map>
#include <string>
#include <utility>

using namespace cet;
using namespace std;

namespace art {

  RootInputFileSequence::RootInputFileSequence(
    fhicl::TableFragment<RootInputFileSequence::Config> const& config,
    InputFileCatalog& catalog,
    ProcessingLimits const& limits,
    UpdateOutputCallbacks& outputCallbacks,
    ProcessConfiguration const& processConfig)
    : catalog_{catalog}
    , fileIndexes_(catalog_.size())
    , eventsToSkip_{config().skipEvents()}
    , compactSubRunRanges_{config().compactSubRunRanges()}
    , noEventSort_{config().noEventSort()}
    , skipBadFiles_{config().skipBadFiles()}
    , treeCacheSize_{config().cacheSize()}
    , treeMaxVirtualSize_{config().treeMaxVirtualSize()}
    , saveMemoryObjectThreshold_{config().saveMemoryObjectThreshold()}
    , delayedReadEventProducts_{config().delayedReadEventProducts()}
    , delayedReadSubRunProducts_{config().delayedReadSubRunProducts()}
    , delayedReadRunProducts_{config().delayedReadRunProducts()}
    , groupSelectorRules_{config().inputCommands(),
                          "inputCommands",
                          "InputSource"}
    , dropDescendants_{config().dropDescendantsOfDroppedBranches()}
    , readParameterSets_{config().readParameterSets()}
    , processingLimits_{limits}
    , processConfiguration_{processConfig}
    , outputCallbacks_{outputCallbacks}
  {
    root::setup();

    auto const& primaryFileNames = catalog_.fileSources();

    map<string const, vector<string> const> secondaryFilesMap;

    std::vector<Config::SecondaryFile> secondaryFiles;
    if (config().secondaryFileNames(secondaryFiles)) {
      // Until we can find a way to atomically update the
      // 'selectedProducts' list for output modules, secondary input
      // files can be used only in single-threaded, single-schedule
      // execution.
      auto const& globals = *Globals::instance();
      if (globals.nthreads() != 1 && globals.nschedules() != 1) {
        throw Exception{
          errors::Configuration,
          "An error occurred while creating the RootInput source.\n"}
          << "This art process is using " << globals.nthreads()
          << " thread(s) and " << globals.nschedules() << " schedule(s).\n"
          << "Secondary file names can be used only when 1 thread and 1 "
             "schedule are specified.\n"
          << "This is done by specifying '-j=1' at the command line.\n";
      }

      for (auto const& val : secondaryFiles) {
        auto const a = val.a();
        auto const b = val.b();
        if (a.empty()) {
          throw Exception(errors::Configuration)
            << "Empty filename found as value of an \"a\" parameter!\n";
        }
        for (auto const& name : b) {
          if (name.empty()) {
            throw Exception(errors::Configuration)
              << "Empty secondary filename found as value of an \"b\" "
                 "parameter!\n";
          }
        }
        secondaryFilesMap.emplace(a, b);
      }
    }

    vector<pair<vector<string>::const_iterator, vector<string>::const_iterator>>
      stk;
    for (auto const& primaryFileName : primaryFileNames) {
      vector<string> secondaries;
      auto SFMI = secondaryFilesMap.find(primaryFileName);
      if (SFMI == secondaryFilesMap.end()) {
        // This primary has no secondaries.
        secondaryFileNames_.push_back(std::move(secondaries));
        continue;
      }
      if (!SFMI->second.size()) {
        // Has an empty secondary list.
        secondaryFileNames_.push_back(std::move(secondaries));
        continue;
      }
      stk.emplace_back(SFMI->second.cbegin(), SFMI->second.cend());
      while (stk.size()) {
        auto val = stk.back();
        stk.pop_back();
        if (val.first == val.second) {
          // Reached end of this filename list.
          continue;
        }
        auto const& fn = *val.first;
        ++val.first;
        secondaries.push_back(fn);
        auto SI = secondaryFilesMap.find(fn);
        if (SI == secondaryFilesMap.end()) {
          // Has no secondary list.
          if (val.first == val.second) {
            // Reached end of this filename list.
            continue;
          }
          stk.emplace_back(val.first, val.second);
          continue;
        }
        if (!SI->second.size()) {
          // Has an empty secondary list.
          if (val.first == val.second) {
            // Reached end of this filename list.
            continue;
          }
          stk.emplace_back(val.first, val.second);
          continue;
        }
        stk.emplace_back(val.first, val.second);
        stk.emplace_back(SI->second.cbegin(), SI->second.cend());
      }
      secondaryFileNames_.push_back(std::move(secondaries));
    }
    RunNumber_t firstRun{};
    bool const haveFirstRun{config().hasFirstRun(firstRun)};
    SubRunNumber_t firstSubRun{};
    bool const haveFirstSubRun{config().hasFirstSubRun(firstSubRun)};
    EventNumber_t firstEvent{};
    bool const haveFirstEvent{config().hasFirstEvent(firstEvent)};

    RunID const firstRunID{haveFirstRun ? RunID{firstRun} : RunID::firstRun()};
    SubRunID const firstSubRunID{haveFirstSubRun ?
                                   SubRunID{firstRunID.run(), firstSubRun} :
                                   SubRunID::firstSubRun(firstRunID)};

    origEventID_ = haveFirstEvent ? EventID{firstSubRunID, firstEvent} :
                                    EventID::firstEvent(firstSubRunID);

    if (noEventSort_ && haveFirstEvent) {
      throw Exception(errors::Configuration)
        << "Illegal configuration options passed to RootInput\n"
        << "You cannot request \"noEventSort\" and also set \"firstEvent\".\n";
    }
    duplicateChecker_ = std::make_shared<DuplicateChecker>(config().dc);
    if (pendingClose_) {
      throw Exception(errors::LogicError)
        << "RootInputFileSequence looking for next file with a pending close!";
    }

    while (!rootFile_ && catalog_.getNextFile()) {
      rootFile_ = initFile(skipBadFiles_);
    }

    if (!rootFile_) {
      // We could not open any input files, stop.
      return;
    }
    RunNumber_t setRun;
    if (config().setRunNumber(setRun)) {
      try {
        forcedRunOffset_ = rootFile_->setForcedRunOffset(setRun);
      }
      catch (Exception& e) {
        if (e.categoryCode() == errors::InvalidNumber) {
          throw Exception(errors::Configuration)
            << "setRunNumber " << setRun
            << " does not correspond to a valid run number in ["
            << RunID::firstRun().run() << ", " << RunID::maxRun().run()
            << "]\n";
        } else {
          throw; // Rethrow.
        }
      }
      if (forcedRunOffset_ < 0) {
        throw Exception(errors::Configuration)
          << "The value of the 'setRunNumber' parameter must not be\n"
          << "less than the first run number in the first input file.\n"
          << "'setRunNumber' was " << setRun << ", while the first run was "
          << setRun - forcedRunOffset_ << ".\n";
      }
    }
    if (!readParameterSets_) {
      mf::LogWarning("PROVENANCE")
        << "Source parameter readParameterSets was set to false: parameter set "
           "provenance\n"
        << "will NOT be available in this or subsequent jobs using output from "
           "this job.\n"
        << "Check your experiment's policy on this issue  to avoid future "
           "problems\n"
        << "with analysis reproducibility.\n";
    }
    if (compactSubRunRanges_) {
      mf::LogWarning("PROVENANCE")
        << "Source parameter compactEventRanges was set to true: enabling "
           "compact event ranges\n"
        << "creates a history that can cause file concatenation problems if a "
           "given SubRun spans\n"
        << "multiple input files.  Use with care.\n";
    }
  }

  EventID
  RootInputFileSequence::seekToEvent(EventID const& eID, bool exact)
  {
    assert(rootFile_);
    // Attempt to find event in currently open input file.
    if (rootFile_->setEntry_Event(eID, true)) {
      // found in the current file
      return rootFile_->eventIDForFileIndexPosition();
    }

    // fail if not searchable
    if (!catalog_.isSearchable()) {
      return EventID();
    }

    // Look for event in files previously opened without reopening unnecessary
    // files.
    for (auto itBegin = fileIndexes_.cbegin(),
              itEnd = fileIndexes_.cend(),
              it = itBegin;
         it != itEnd;
         ++it) {
      if (*it && (*it)->contains(eID, exact)) {
        // We found it. Close the currently open file, and open the correct one.
        catalog_.rewindTo(std::distance(itBegin, it));
        rootFile_ = initFile();
        // Now get the event from the correct file.
        bool const found [[maybe_unused]] =
          rootFile_->setEntry_Event(eID, exact);
        assert(found);
        break;
      }
    }

    // Look for event in files not yet opened.
    while (catalog_.getNextFile()) {
      rootFile_ = initFile();
      if (rootFile_->setEntry_Event(eID, exact)) {
        return rootFile_->eventIDForFileIndexPosition();
      }
    }

    return EventID();
  }

  EventID
  RootInputFileSequence::seekToEvent(int offset, bool)
  {
    assert(rootFile_);
    skip(offset);
    return rootFile_->eventIDForFileIndexPosition();
  }

  void
  RootInputFileSequence::endJob()
  {
    closeFile_();
  }

  std::unique_ptr<FileBlock>
  RootInputFileSequence::readFile_()
  {
    if (rootFile_) {
      return rootFile_->createFileBlock();
    }

    while (catalog_.getNextFile()) {
      rootFile_ = initFile(skipBadFiles_);
      if (rootFile_) {
        return rootFile_->createFileBlock();
      }
    }

    // We could not open any input files.
    return nullptr;
  }

  void
  RootInputFileSequence::closeFile_()
  {
    if (pendingClose_) {
      catalog_.finish(); // We were expecting this
      pendingClose_ = false;
    }
    if (!rootFile_) {
      return;
    }
    // Account for events skipped in the file.
    eventsToSkip_ = rootFile_->eventsToSkip();
    rootFile_->close();
    for (auto const& sf : secondaryFilesForPrimary_) {
      if (!sf) {
        continue;
      }
      sf->close();
    }
    detail::logFileAction("Closed input file ", rootFile_->fileName());
    rootFile_.reset();
    if (duplicateChecker_.get() != nullptr) {
      duplicateChecker_->inputFileClosed();
    }
  }

  void
  RootInputFileSequence::finish()
  {
    pendingClose_ = true;
  }

  std::shared_ptr<RootInputFile>
  RootInputFileSequence::initFile(bool const skipBadFiles)
  {
    // close the currently open file, any, and delete the RootInputFile object.
    closeFile_();
    std::unique_ptr<TFile> filePtr;
    try {
      detail::logFileAction("Initiating request to open input file ",
                            catalog_.currentFile().fileName());
      filePtr.reset(TFile::Open(catalog_.currentFile().fileName().c_str()));
    }
    catch (cet::exception& e) {
      if (!skipBadFiles) {
        throw Exception(errors::FileOpenError)
          << e.explain_self()
          << "\nRootInputFileSequence::initFile(): Input file "
          << catalog_.currentFile().fileName()
          << " was not found or could not be opened.\n";
      }
    }
    if (!filePtr || filePtr->IsZombie()) {
      if (!skipBadFiles) {
        throw Exception(errors::FileOpenError)
          << "RootInputFileSequence::initFile(): Input file "
          << catalog_.currentFile().fileName()
          << " was not found or could not be opened.\n";
      }
      mf::LogWarning("")
        << "Input file: " << catalog_.currentFile().fileName()
        << " was not found or could not be opened, and will be skipped.\n";
      return nullptr;
    }
    detail::logFileAction("Opened input file ",
                          catalog_.currentFile().fileName());

    // Group the following together into one class?
    auto const n_secondary_files =
      secondaryFileNames_.empty() ?
        0ul :
        secondaryFileNames_.at(catalog_.currentIndex()).size();
    secondaryFilesForPrimary_ =
      vector<unique_ptr<RootInputFile>>(n_secondary_files);
    auto secondary_opener =
      n_secondary_files == 0ul ?
        secondary_reader_t{} :
        [this](int& idx, BranchType const bt, EventID const& eid) {
          return this->nextSecondaryPrincipal(idx, bt, eid);
        };

    auto result = make_shared<RootInputFile>(catalog_.currentFile().fileName(),
                                             processConfiguration_,
                                             std::move(filePtr),
                                             origEventID_,
                                             eventsToSkip_,
                                             compactSubRunRanges_,
                                             treeCacheSize_,
                                             treeMaxVirtualSize_,
                                             saveMemoryObjectThreshold_,
                                             delayedReadEventProducts_,
                                             delayedReadSubRunProducts_,
                                             delayedReadRunProducts_,
                                             processingLimits_,
                                             forcedRunOffset_,
                                             noEventSort_,
                                             groupSelectorRules_,
                                             dropDescendants_,
                                             readParameterSets_,
                                             outputCallbacks_,
                                             secondary_opener,
                                             duplicateChecker_);

    assert(catalog_.currentIndex() != InputFileCatalog::indexEnd);
    if (catalog_.currentIndex() + 1 > fileIndexes_.size()) {
      fileIndexes_.resize(catalog_.currentIndex() + 1);
    }
    fileIndexes_[catalog_.currentIndex()] = result->fileIndexSharedPtr();
    return result;
  }

  RootInputFile&
  RootInputFileSequence::secondaryFile(int const idx)
  {
    auto& file = secondaryFilesForPrimary_[idx];
    if (file) {
      return *file;
    }

    auto const& name = secondaryFileNames_.at(catalog_.currentIndex())[idx];
    std::unique_ptr<TFile> filePtr;
    try {
      detail::logFileAction("Attempting to open secondary input file ", name);
      filePtr.reset(TFile::Open(name.c_str()));
    }
    catch (cet::exception& e) {
      throw Exception(errors::FileOpenError)
        << e.explain_self()
        << "\nRootInputFileSequence::openSecondaryFile(): Input file " << name
        << " was not found or could not be opened.\n";
    }
    if (!filePtr || filePtr->IsZombie()) {
      throw Exception(errors::FileOpenError)
        << "RootInputFileSequence::openSecondaryFile(): Input file " << name
        << " was not found or could not be opened.\n";
    }
    detail::logFileAction("Opened secondary input file ", name);

    file = std::make_unique<RootInputFile>(name,
                                           processConfiguration_,
                                           std::move(filePtr),
                                           origEventID_,
                                           eventsToSkip_,
                                           compactSubRunRanges_,
                                           treeCacheSize_,
                                           treeMaxVirtualSize_,
                                           saveMemoryObjectThreshold_,
                                           delayedReadEventProducts_,
                                           delayedReadSubRunProducts_,
                                           delayedReadRunProducts_,
                                           processingLimits_,
                                           forcedRunOffset_,
                                           noEventSort_,
                                           groupSelectorRules_,
                                           dropDescendants_,
                                           readParameterSets_,
                                           outputCallbacks_);
    return *file;
  }

  bool
  RootInputFileSequence::atEnd(int const idx)
  {
    // Check if secondary input files are available
    assert(idx >= 0);
    auto const uidx = static_cast<size_t>(idx);
    return uidx == secondaryFilesForPrimary_.size() or
           secondaryFilesForPrimary_.empty();
  }

  // Note: Return code of -2 means stop, -1 means event-not-found,
  //       otherwise 0 for success.
  std::unique_ptr<Principal>
  RootInputFileSequence::readFromSecondaryFile(int const idx,
                                               BranchType const bt,
                                               EventID const& eventID)
  {
    // Check if secondary input files are available
    assert(idx >= 0);
    auto const uidx = static_cast<size_t>(idx);
    assert(uidx <= secondaryFilesForPrimary_.size());
    if (uidx == secondaryFilesForPrimary_.size() or
        secondaryFilesForPrimary_.empty()) {
      return nullptr;
    }

    switch (bt) {
    case InEvent: {
      return secondaryFile(idx).readEventWithID(eventID);
    }
    case InSubRun: {
      return secondaryFile(idx).readSubRunWithID(eventID.subRunID());
    }
    case InRun: {
      return secondaryFile(idx).readRunWithID(eventID.runID());
    }
    default: {
      assert(false &&
             "RootDelayedReader encountered an unsupported BranchType!");
    }
    }

    return nullptr;
  }

  std::unique_ptr<Principal>
  RootInputFileSequence::nextSecondaryPrincipal(int& idx,
                                                BranchType const bt,
                                                EventID const& eventID)
  {
    std::unique_ptr<Principal> p;
    while (not p) {
      if (atEnd(idx)) {
        return nullptr;
      }
      p = readFromSecondaryFile(idx, bt, eventID);
      ++idx;
    }

    return p;
  }

  std::shared_ptr<RootInputFile>
  RootInputFileSequence::nextFile()
  {
    if (!catalog_.getNextFile()) {
      // no more files
      return nullptr;
    }
    return initFile(skipBadFiles_);
  }

  std::shared_ptr<RootInputFile>
  RootInputFileSequence::previousFile()
  {
    // no going back for non-persistent files
    if (!catalog_.isSearchable()) {
      return nullptr;
    }
    // no file in the catalog
    if (catalog_.currentIndex() == InputFileCatalog::indexEnd) {
      return nullptr;
    }
    // first file in the catalog, move to the last file in the list
    if (catalog_.currentIndex() == 0) {
      return nullptr;
    }

    catalog_.rewindTo(catalog_.currentIndex() - 1);
    auto result = initFile();
    if (result) {
      result->setToLastEntry();
    }
    return result;
  }

  void
  RootInputFileSequence::readIt(EventID const& id, bool exact)
  {
    // Attempt to find event in currently open input file.
    if (rootFile_->setEntry_Event(id, exact)) {
      rootFileForLastReadEvent_ = rootFile_;
      return;
    }

    if (!catalog_.isSearchable()) {
      return;
    }

    // Look for event in cached files
    for (auto IB = fileIndexes_.cbegin(), IE = fileIndexes_.cend(), I = IB;
         I != IE;
         ++I) {
      if (*I && (*I)->contains(id, exact)) {
        // We found it. Close the currently open file, and open the correct one.
        catalog_.rewindTo(std::distance(IB, I));
        rootFile_ = initFile();
        bool const found [[maybe_unused]] =
          rootFile_->setEntry_Event(id, exact);
        assert(found);
        rootFileForLastReadEvent_ = rootFile_;
        return;
      }
    }

    // Look for event in files not yet opened.
    while (catalog_.getNextFile()) {
      rootFile_ = initFile();
      if (rootFile_->setEntry_Event(id, exact)) {
        rootFileForLastReadEvent_ = rootFile_;
        return;
      }
    }

    // Not found
  }

  unique_ptr<EventPrincipal>
  RootInputFileSequence::readEvent_()
  {
    // Create and setup the EventPrincipal.
    //
    //   1. create an EventPrincipal with a unique EventID
    //   2. For each entry in the provenance, put in one Group,
    //      holding the Provenance for the corresponding EDProduct.
    //   3. set up the caches in the EventPrincipal to know about this
    //      Group.
    //
    // We do *not* create the EDProduct instance (the equivalent of reading
    // the branch containing this EDProduct. That will be done by the
    // Delayed Reader when it is asked to do so.
    //
    rootFileForLastReadEvent_ = rootFile_;
    return rootFile_->readEvent();
  }

  std::unique_ptr<RangeSetHandler>
  RootInputFileSequence::runRangeSetHandler()
  {
    return rootFile_->runRangeSetHandler();
  }

  std::unique_ptr<RangeSetHandler>
  RootInputFileSequence::subRunRangeSetHandler()
  {
    return rootFile_->subRunRangeSetHandler();
  }

  void
  RootInputFileSequence::readIt(SubRunID const& id)
  {
    // Attempt to find subRun in currently open input file.
    if (rootFile_->setEntry_SubRun(id)) {
      return;
    }
    if (!catalog_.isSearchable()) {
      return;
    }
    // Look for event in cached files
    for (auto itBegin = fileIndexes_.begin(),
              itEnd = fileIndexes_.end(),
              it = itBegin;
         it != itEnd;
         ++it) {
      if (*it && (*it)->contains(id, true)) {
        // We found it. Close the currently open file, and open the correct one.
        catalog_.rewindTo(std::distance(itBegin, it));
        rootFile_ = initFile();
        bool const found [[maybe_unused]] = rootFile_->setEntry_SubRun(id);
        assert(found);
        return;
      }
    }
    // Look for subRun in files not yet opened.
    while (catalog_.getNextFile()) {
      rootFile_ = initFile();
      if (rootFile_->setEntry_SubRun(id)) {
        return;
      }
    }

    // not found
  }

  std::unique_ptr<SubRunPrincipal>
  RootInputFileSequence::readSubRun_()
  {
    return rootFile_->readSubRun();
  }

  void
  RootInputFileSequence::readIt(RunID const& id)
  {
    // Attempt to find run in current file.
    if (rootFile_->setEntry_Run(id)) {
      // Got it, done.
      return;
    }
    if (!catalog_.isSearchable()) {
      // Cannot random access files, give up.
      return;
    }
    // Look for the run in the opened files.
    for (auto B = fileIndexes_.cbegin(), E = fileIndexes_.cend(), I = B; I != E;
         ++I) {
      if (*I && (*I)->contains(id, true)) {
        // We found it, open the file.
        catalog_.rewindTo(std::distance(B, I));
        rootFile_ = initFile();
        bool const found [[maybe_unused]] = rootFile_->setEntry_Run(id);
        assert(found);
        return;
      }
    }
    // Look for run in files not yet opened.
    while (catalog_.getNextFile()) {
      rootFile_ = initFile();
      if (rootFile_->setEntry_Run(id)) {
        return;
      }
    }

    // Not found.
  }

  std::unique_ptr<RunPrincipal>
  RootInputFileSequence::readRun_()
  {
    return rootFile_->readRun();
  }

  input::ItemType
  RootInputFileSequence::getNextItemType()
  {
    if (firstFile_) {
      // This is a bit of a wart.  Ick.
      firstFile_ = false;
      return input::IsFile;
    }
    if (rootFile_) {
      switch (rootFile_->getNextEntryTypeWanted()) {
      case FileIndex::kEvent:
        return input::IsEvent;
      case FileIndex::kSubRun:
        return input::IsSubRun;
      case FileIndex::kRun:
        return input::IsRun;
      case FileIndex::kEnd: {
      }
      }
    }
    // We are either at the end of a root file or the current file is
    // not a root file
    if (!catalog_.hasNextFile()) {
      // FIXME: upon the advent of a catalog system which can do
      // something intelligent with the difference between whole-file
      // success, partial-file success, partial-file failure and
      // whole-file failure (such as file-open failure), we will need
      // to communicate that difference here. The file disposition
      // options as they are now (and the mapping to any concrete
      // implementation we are are aware of currently) are not
      // sufficient to the task, so we deliberately do not distinguish
      // here between partial-file and whole-file success in
      // particular.
      finish();
      return input::IsStop;
    }
    return input::IsFile;
  }

  // Advance "offset" events.  Offset can be positive or negative (or zero).
  void
  RootInputFileSequence::skip(int offset)
  {
    while (offset != 0) {
      if (!rootFile_)
        return;

      offset = rootFile_->skipEvents(offset);
      if (offset > 0) {
        rootFile_ = nextFile();
      } else if (offset < 0) {
        rootFile_ = previousFile();
      }
    }
    rootFile_->skipEvents(0);
  }

} // namespace art
