#ifndef art_root_io_detail_InfoDumperInputFile_h
#define art_root_io_detail_InfoDumperInputFile_h

#include "art_root_io/Inputfwd.h"
#include "canvas/Persistency/Provenance/Compatibility/BranchIDList.h"
#include "canvas/Persistency/Provenance/FileFormatVersion.h"
#include "canvas/Persistency/Provenance/FileIndex.h"
#include "canvas/Persistency/Provenance/ProcessHistory.h"
#include "canvas/Persistency/Provenance/RangeSet.h"
#include "canvas/Persistency/Provenance/RunAuxiliary.h"

#include "TFile.h"
class TTree;

struct sqlite3;

#include <memory>
#include <ostream>
#include <string>

namespace art::detail {

  class InfoDumperInputFile {
  public:
    using EntryNumber = input::EntryNumber;
    using EntryNumbers = input::EntryNumbers;

    InfoDumperInputFile(std::string const& filename);
    void print_process_history(std::ostream&) const;
    void print_range_sets(std::ostream&, bool compactRanges) const;
    void print_event_list(std::ostream&) const;
    void print_file_index(std::ostream&) const;
    void print_branchIDLists(std::ostream& os) const;
    TFile*
    tfile() const
    {
      return file_.get();
    }

  private:
    RunAuxiliary getAuxiliary(TTree* tree, EntryNumber entry) const;

    RangeSet getRangeSet(TTree* tree,
                         EntryNumbers const& entries,
                         sqlite3* db,
                         std::string const& filename,
                         bool compactRanges) const;

    std::unique_ptr<TFile> file_;
    BranchIDLists branchIDLists_{};
    ProcessHistoryMap pHistMap_;
    FileIndex fileIndex_;
    FileFormatVersion fileFormatVersion_;
  };

} // namespace art::detail

#endif /* art_root_io_detail_InfoDumperInputFile_h */

// Local variables:
// mode: c++
// End:
