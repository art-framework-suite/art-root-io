#ifndef art_root_io_RootInputFileSequence_h
#define art_root_io_RootInputFileSequence_h
// vim: set sw=2:

#include "art/Framework/Core/GroupSelectorRules.h"
#include "art/Framework/Core/InputSource.h"
#include "art/Framework/Core/fwd.h"
#include "art_root_io/DuplicateChecker.h"
#include "art_root_io/Inputfwd.h"
#include "art_root_io/RootInputFile.h"
#include "canvas/Persistency/Provenance/EventID.h"
#include "canvas/Persistency/Provenance/fwd.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/OptionalAtom.h"
#include "fhiclcpp/types/OptionalSequence.h"
#include "fhiclcpp/types/Sequence.h"
#include "fhiclcpp/types/Table.h"
#include "fhiclcpp/types/TableFragment.h"

#include <memory>
#include <string>
#include <vector>

namespace art {

  class InputFileCatalog;
  class UpdateOutputCallbacks;

  class RootInputFileSequence {
  public:
    using RootInputFileSharedPtr = std::shared_ptr<RootInputFile>;
    using EntryNumber = input::EntryNumber;

    RootInputFileSequence(RootInputFileSequence const&) = delete;
    RootInputFileSequence& operator=(RootInputFileSequence const&) = delete;

    struct Config {

      using Name = fhicl::Name;
      using Comment = fhicl::Comment;
      template <typename T>
      using Atom = fhicl::Atom<T>;
      template <typename T>
      using OptionalAtom = fhicl::OptionalAtom<T>;
      template <typename T>
      using OptionalSequence = fhicl::OptionalSequence<T>;
      template <typename T>
      using Sequence = fhicl::Sequence<T>;
      template <typename T>
      using Table = fhicl::Table<T>;
      template <typename T>
      using TableFragment = fhicl::TableFragment<T>;

      TableFragment<DuplicateChecker::Config> dc;
      Atom<EventNumber_t> skipEvents{Name("skipEvents"), 0};
      Atom<bool> noEventSort{Name("noEventSort"), false};
      Atom<bool> skipBadFiles{Name("skipBadFiles"), false};
      Atom<unsigned> cacheSize{Name("cacheSize"), 0u};
      Atom<std::int64_t> treeMaxVirtualSize{Name("treeMaxVirtualSize"), -1};
      Atom<std::int64_t> saveMemoryObjectThreshold{
        Name("saveMemoryObjectThreshold"),
        -1};
      Atom<bool> delayedReadEventProducts{Name("delayedReadEventProducts"),
                                          true};
      Atom<bool> delayedReadSubRunProducts{Name("delayedReadSubRunProducts"),
                                           false};
      Atom<bool> delayedReadRunProducts{Name("delayedReadRunProducts"), false};
      Sequence<std::string> inputCommands{Name("inputCommands"),
                                          std::vector<std::string>{"keep *"}};
      Atom<bool> dropDescendantsOfDroppedBranches{
        Name("dropDescendantsOfDroppedBranches"),
        true};
      Atom<bool> readParameterSets{Name("readParameterSets"), true};

      struct SecondaryFile {
        Atom<std::string> a{Name("a"), ""};
        Sequence<std::string> b{Name("b"), std::vector<std::string>{}};
      };

      OptionalSequence<Table<SecondaryFile>> secondaryFileNames{
        Name("secondaryFileNames")};
      OptionalAtom<RunNumber_t> hasFirstRun{Name("firstRun")};
      OptionalAtom<SubRunNumber_t> hasFirstSubRun{Name("firstSubRun")};
      OptionalAtom<EventNumber_t> hasFirstEvent{Name("firstEvent")};
      Atom<bool> compactSubRunRanges{
        Name("compactEventRanges"),
        Comment(
          "If users can guarantee that SubRuns do not span multiple input\n"
          "files, the 'compactEventRanges' parameter can be set to 'true'\n"
          "to ensure the most compact representation of event-ranges "
          "associated\n"
          "with all Runs and SubRuns stored in the input file.\n\n"
          "WARNING: Enabling compact event ranges creates a history that can\n"
          "         cause file concatenation problems if a given SubRun spans\n"
          "         multiple input files.  Use with care."),
        false};
    };

    RootInputFileSequence(fhicl::TableFragment<Config> const&,
                          InputFileCatalog&,
                          ProcessingLimits const&,
                          UpdateOutputCallbacks&,
                          ProcessConfiguration const&);
    void endJob();

    std::unique_ptr<FileBlock> readFile_();

    std::unique_ptr<Principal> readFromSecondaryFile(int idx,
                                                     BranchType bt,
                                                     EventID const& eventID);
    std::unique_ptr<Principal> nextSecondaryPrincipal(int& idx,
                                                      BranchType bt,
                                                      EventID const& eventID);

    void closeFile_();

    void skip(int offset);

    EventID seekToEvent(EventID const&, bool exact = false);
    EventID seekToEvent(int offset);

    input::ItemType getNextItemType();

    void readIt(RunID const&);

    std::unique_ptr<RunPrincipal> readRun_();

    void readIt(SubRunID const&);

    std::unique_ptr<SubRunPrincipal> readSubRun_();

    void readIt(EventID const&, bool exact = false);

    std::unique_ptr<EventPrincipal> readEvent_();

    RootInputFileSharedPtr
    rootFileForLastReadEvent() const
    {
      return rootFileForLastReadEvent_;
    }

    RootInputFileSharedPtr
    rootFile() const
    {
      return rootFile_;
    }

    std::unique_ptr<RangeSetHandler> runRangeSetHandler();
    std::unique_ptr<RangeSetHandler> subRunRangeSetHandler();

    std::vector<std::vector<std::string>> const&
    secondaryFileNames() const
    {
      return secondaryFileNames_;
    }

    void finish();

  private:
    std::shared_ptr<RootInputFile> initFile(bool skipBadFiles = false);
    std::shared_ptr<RootInputFile> nextFile();
    std::shared_ptr<RootInputFile> previousFile();

    RootInputFile& secondaryFile(int idx);
    bool atEnd(int idx);

    InputFileCatalog& catalog_;
    RootInputFileSharedPtr rootFile_{nullptr};
    std::vector<std::unique_ptr<RootInputFile>> secondaryFilesForPrimary_;
    std::vector<std::shared_ptr<FileIndex>> fileIndexes_;
    bool firstFile_{true};
    EventID origEventID_{};
    EventNumber_t eventsToSkip_;
    bool const compactSubRunRanges_;
    bool const noEventSort_;
    bool const skipBadFiles_;
    unsigned int const treeCacheSize_;
    int64_t const treeMaxVirtualSize_;
    int64_t const saveMemoryObjectThreshold_;
    bool const delayedReadEventProducts_;
    bool const delayedReadSubRunProducts_;
    bool const delayedReadRunProducts_;
    GroupSelectorRules groupSelectorRules_;
    std::shared_ptr<DuplicateChecker> duplicateChecker_{nullptr};
    bool const dropDescendants_;
    bool const readParameterSets_;
    RootInputFileSharedPtr rootFileForLastReadEvent_;
    ProcessingLimits const& processingLimits_;
    ProcessConfiguration const& processConfiguration_;
    std::vector<std::vector<std::string>> secondaryFileNames_{};
    UpdateOutputCallbacks& outputCallbacks_;
    bool pendingClose_{false};
  };

} // namespace art

// Local Variables:
// mode: c++
// End:
#endif /* art_root_io_RootInputFileSequence_h */
