#ifndef art_root_io_RootDelayedReader_h
#define art_root_io_RootDelayedReader_h
// vim: set sw=2 expandtab :

#include "art/Persistency/Common/DelayedReader.h"
#include "art_root_io/Inputfwd.h"
#include "canvas/Persistency/Provenance/BranchType.h"
#include "canvas/Persistency/Provenance/Compatibility/BranchIDList.h"
#include "canvas/Persistency/Provenance/EventID.h"
#include "canvas/Persistency/Provenance/FileFormatVersion.h"
#include "canvas/Persistency/Provenance/ProductID.h"
#include "canvas/Persistency/Provenance/RangeSet.h"

#include <atomic>
#include <map>
#include <memory>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif
  typedef struct sqlite3 sqlite3;
#ifdef __cplusplus
}
#endif

class TBranch;
class TFile;

namespace art {
  using secondary_reader_t =
    std::function<int(int, BranchType, EventID const&)>;

  class Group;
  class Principal;
  class ProductProvenance;

  class RootDelayedReader final : public DelayedReader {
  public:
    ~RootDelayedReader();
    RootDelayedReader(RootDelayedReader const&) = delete;
    RootDelayedReader& operator=(RootDelayedReader const&) = delete;
    RootDelayedReader(RootDelayedReader&&) = delete;
    RootDelayedReader& operator=(RootDelayedReader&&) = delete;
    RootDelayedReader(FileFormatVersion,
                      sqlite3* db,
                      std::vector<input::EntryNumber> const& entrySet,
                      cet::exempt_ptr<input::BranchMap const>,
                      TBranch* provenanceBranch,
                      int64_t saveMemoryObjectThreshold,
                      secondary_reader_t secondaryFileReader,
                      cet::exempt_ptr<BranchIDLists const> branchIDLists,
                      BranchType branchType,
                      EventID,
                      bool compactSubRunRanges);

  private:
    std::unique_ptr<EDProduct> getProduct_(Group const*,
                                           ProductID,
                                           RangeSet&) const override;
    void setPrincipal_(cet::exempt_ptr<Principal>) override;
    std::vector<ProductProvenance> readProvenance_() const override;
    bool isAvailableAfterCombine_(ProductID) const override;
    int readFromSecondaryFile_(int idx) override;

    FileFormatVersion fileFormatVersion_;
    sqlite3* db_;
    std::vector<input::EntryNumber> const entrySet_;
    cet::exempt_ptr<input::BranchMap const> branches_;
    TBranch* provenanceBranch_;
    int64_t saveMemoryObjectThreshold_;
    cet::exempt_ptr<Principal> principal_;
    secondary_reader_t secondaryFileReader_;
    cet::exempt_ptr<BranchIDLists const> branchIDLists_;
    BranchType branchType_;
    EventID eventID_;
    bool const compactSubRunRanges_;
  };
} // namespace art

// Local Variables:
// mode: c++
// End:
#endif /* art_root_io_RootDelayedReader_h */
