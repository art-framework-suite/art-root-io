#ifndef art_root_io_DuplicateChecker_h
#define art_root_io_DuplicateChecker_h

// ======================================================================
//
// DuplicateChecker - Used by RootInput to detect events with the same
//                    run and event number.
//
// The class can be configured to check for duplicates
//   - within the scope of each single input file, or
//   - all input files, or
//   - not at all.
//
// ======================================================================

#include "canvas/Persistency/Provenance/EventID.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/TableFragment.h"
#include <set>
#include <string>

// ----------------------------------------------------------------------

namespace art {

  class FileIndex;

  class DuplicateChecker {
  public:
    struct Config {
      fhicl::Atom<std::string> duplicateCheckMode{
        fhicl::Name("duplicateCheckMode"),
        "checkEachRealDataFile"};
    };

    DuplicateChecker(fhicl::TableFragment<Config> const& config);

    void init(bool realData, FileIndex const& fileIndex);

    void inputFileClosed();

    bool isDuplicateAndCheckActive(EventID const& eventID,
                                   std::string const& fileName);

  private:
    enum DuplicateCheckMode {
      noDuplicateCheck,
      checkEachFile,
      checkEachRealDataFile,
      checkAllFilesOpened
    };

    DuplicateCheckMode duplicateCheckMode_;

    enum DataType { isRealData, isSimulation, unknown };

    DataType dataType_;

    std::set<EventID> eventIDs_;

    bool itIsKnownTheFileHasNoDuplicates_;
  }; // DuplicateChecker

} // namespace art

// ======================================================================

#endif /* art_root_io_DuplicateChecker_h */

// Local Variables:
// mode: c++
// End:
