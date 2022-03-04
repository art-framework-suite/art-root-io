#ifndef art_root_io_RootInputFile_h
#define art_root_io_RootInputFile_h
// vim: set sw=2 expandtab :

#include "art/Framework/Core/InputSource.h"
#include "art/Framework/Core/fwd.h"
#include "art/Framework/Principal/EventPrincipal.h"
#include "art/Framework/Principal/RangeSetHandler.h"
#include "art/Framework/Principal/ResultsPrincipal.h"
#include "art/Framework/Principal/RunPrincipal.h"
#include "art/Framework/Principal/SubRunPrincipal.h"
#include "art_root_io/Inputfwd.h"
#include "art_root_io/RootDelayedReader.h"
#include "canvas/Persistency/Provenance/Compatibility/fwd.h"
#include "canvas/Persistency/Provenance/EventAuxiliary.h"
#include "canvas/Persistency/Provenance/FileFormatVersion.h"
#include "canvas/Persistency/Provenance/FileIndex.h"
#include "canvas/Persistency/Provenance/ProductTables.h"
#include "canvas/Persistency/Provenance/ResultsAuxiliary.h"
#include "canvas/Persistency/Provenance/RunAuxiliary.h"
#include "canvas/Persistency/Provenance/SubRunAuxiliary.h"
#include "canvas/Persistency/Provenance/fwd.h"
#include "cetlib/exempt_ptr.h"
#include "cetlib/sqlite/Connection.h"

#include <array>
#include <memory>
#include <string>

class TFile;
class TTree;
class TBranch;

namespace art {
  class BranchChildren;
  class DuplicateChecker;
  class GroupSelectorRules;
  namespace detail {
    struct RangeSetInfo;
  }

  class RootInputFile {
    class RootInputTree {
    public:
      using BranchMap = input::BranchMap;
      using EntryNumber = input::EntryNumber;
      using EntryNumbers = input::EntryNumbers;

      ~RootInputTree();
      RootInputTree(cet::exempt_ptr<TFile>, BranchType, bool missingOK = false);
      RootInputTree(RootInputTree const&) = delete;
      RootInputTree(RootInputTree&&) = delete;
      RootInputTree& operator=(RootInputTree const&) = delete;
      RootInputTree&& operator=(RootInputTree&&) = delete;

      bool isValid() const;
      EntryNumber nEntries() const;
      TTree* tree() const;
      TTree* metaTree() const;
      TBranch* auxBranch() const;
      TBranch* productProvenanceBranch() const;
      BranchMap const& branches() const;
      void addBranch(BranchDescription const&);
      void dropBranch(std::string const& branchName);

    private:
      TTree* tree_{nullptr};
      TTree* metaTree_{nullptr};
      TBranch* auxBranch_{nullptr};
      TBranch* productProvenanceBranch_{nullptr};
      EntryNumber nEntries_{0};
      BranchMap branches_{};
    };

    using RootInputTreePtrArray =
      std::array<std::unique_ptr<RootInputTree>, NumBranchTypes>;
    using EntryNumber = RootInputTree::EntryNumber;
    using EntryNumbers = RootInputTree::EntryNumbers;

  public:
    ~RootInputFile();
    RootInputFile(RootInputFile const&) = delete;
    RootInputFile(RootInputFile&&) = delete;
    RootInputFile& operator=(RootInputFile const&) = delete;
    RootInputFile& operator=(RootInputFile&&) = delete;
    RootInputFile(std::string const& fileName,
                  ProcessConfiguration const& processConfiguration,
                  std::unique_ptr<TFile>&& filePtr,
                  EventID const& origEventID,
                  unsigned int eventsToSkip,
                  bool compactSubRunRanges,
                  unsigned int treeCacheSize,
                  int64_t treeMaxVirtualSize,
                  int64_t saveMemoryObjectThreashold,
                  bool delayedReadEventProducts,
                  bool delayedReadSubRunProducts,
                  bool delayedReadRunProducts,
                  ProcessingLimits const& limits,
                  int forcedRunOffset,
                  bool noEventSort,
                  GroupSelectorRules const& groupSelectorRules,
                  bool dropDescendantsOfDroppedProducts,
                  bool readIncomingParameterSets,
                  UpdateOutputCallbacks& outputCallbacks,
                  secondary_reader_t openSecondaryFile = {},
                  std::shared_ptr<DuplicateChecker> duplicateChecker = nullptr);

    void reportOpened();
    void close();

    // Assumes sequential access
    std::unique_ptr<ResultsPrincipal> readResults();
    std::unique_ptr<RunPrincipal> readRun();
    std::unique_ptr<SubRunPrincipal> readSubRun();
    std::unique_ptr<EventPrincipal> readEvent();

    // Random access
    std::unique_ptr<RunPrincipal> readRunWithID(
      RunID id,
      bool thenAdvanceToNextRun = false);
    std::unique_ptr<SubRunPrincipal> readSubRunWithID(
      SubRunID id,
      bool thenAdvanceToNextSubRun = false);
    std::unique_ptr<EventPrincipal> readEventWithID(EventID const& id);

    std::string const& fileName() const;
    bool fastClonable() const;
    std::unique_ptr<FileBlock> createFileBlock();

    template <typename ID>
    bool setEntry(ID const& id, bool exact = true);

    void setToLastEntry();
    void nextEntry();
    void previousEntry();
    void advanceEntry(std::size_t n);
    unsigned eventsToSkip() const;
    int skipEvents(int offset);
    int setForcedRunOffset(RunNumber_t const& forcedRunNumber);
    FileIndex::EntryType getEntryType() const;
    FileIndex::EntryType getNextEntryTypeWanted();
    std::shared_ptr<FileIndex> fileIndexSharedPtr() const;
    EventID eventIDForFileIndexPosition() const;
    std::unique_ptr<RangeSetHandler> runRangeSetHandler();
    std::unique_ptr<RangeSetHandler> subRunRangeSetHandler();

  private:
    RootInputTree const& eventTree() const;
    RootInputTree const& subRunTree() const;
    RootInputTree const& runTree() const;
    RootInputTree const& resultsTree() const;
    std::unique_ptr<RootInputTree> makeInputTree(BranchType bt,
                                                 bool missingOK) const;
    RootInputTree& eventTree();
    RootInputTree& subRunTree();
    RootInputTree& runTree();
    RootInputTree& resultsTree();
    bool setIfFastClonable() const;
    void validateFile();
    void fillHistory(EntryNumber entry, History&);

    template <typename Aux>
    Aux getAuxiliary(EntryNumber entry) const;

    template <typename Aux>
    std::pair<Aux, std::unique_ptr<RangeSetHandler>> getAuxiliary(
      EntryNumbers const& entries) const;

    detail::RangeSetInfo resolveInfo(BranchType bt, unsigned rangeSetID) const;

    RunID overrideRunNumber(RunID id) const;
    SubRunID overrideRunNumber(SubRunID const& id) const;
    EventID overrideRunNumber(EventID const& id, bool isRealData) const;

    template <typename Aux>
    Aux overrideAuxiliary(Aux auxiliary) const;
    EventAuxiliary overrideAuxiliary(EventAuxiliary aux, EntryNumber entry);

    void dropOnInput(GroupSelectorRules const& rules,
                     BranchChildren const& branchChildren,
                     bool dropDescendants,
                     ProductTables& tables);
    void readParentageTree(unsigned int treeCacheSize);
    void readEventHistoryTree(unsigned int treeCacheSize);
    void initializeDuplicateChecker();
    std::pair<EntryNumbers, bool> getEntryNumbers(BranchType);

    std::string const fileName_;
    ProcessConfiguration const& processConfiguration_;
    std::unique_ptr<TFile> filePtr_;
    // Start with invalid connection.
    std::unique_ptr<cet::sqlite::Connection> sqliteDB_{nullptr};
    EventID origEventID_;
    EventNumber_t eventsToSkip_;
    bool const compactSubRunRanges_;
    RootInputTreePtrArray treePointers_;
    bool delayedReadEventProducts_;
    bool delayedReadSubRunProducts_;
    bool delayedReadRunProducts_;
    ProcessingLimits const& processingLimits_;
    int forcedRunOffset_;
    bool noEventSort_;
    secondary_reader_t readFromSecondaryFile_;
    std::shared_ptr<DuplicateChecker> duplicateChecker_;
    cet::exempt_ptr<RootInputFile> primaryFile_;
    FileFormatVersion fileFormatVersion_{};
    std::shared_ptr<FileIndex> fileIndexSharedPtr_{new FileIndex};
    FileIndex& fileIndex_{*fileIndexSharedPtr_};
    FileIndex::const_iterator fiBegin_{fileIndex_.begin()};
    FileIndex::const_iterator fiEnd_{fileIndex_.end()};
    FileIndex::const_iterator fiIter_{fiBegin_};
    bool fastClonable_{false};
    ProductTables presentProducts_{ProductTables::invalid()};
    std::unique_ptr<BranchIDLists> branchIDLists_{};
    TTree* eventHistoryTree_{nullptr};
    // We need to add the secondary principals to the primary
    // principal when they are delay read, so we need to keep around a
    // pointer to the primary.  Note that these are always used in a
    // situation where we are guaranteed that primary exists.
    cet::exempt_ptr<EventPrincipal> primaryEP_{};
    cet::exempt_ptr<RunPrincipal> primaryRP_{};
    cet::exempt_ptr<SubRunPrincipal> primarySRP_{};
    std::unique_ptr<RangeSetHandler> subRunRangeSetHandler_{nullptr};
    std::unique_ptr<RangeSetHandler> runRangeSetHandler_{nullptr};
    int64_t saveMemoryObjectThreshold_;
  };
} // namespace art

// Local Variables:
// mode: c++
// End:
#endif /* art_root_io_RootInputFile_h */
