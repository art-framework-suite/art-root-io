#ifndef art_root_io_detail_SamplingDelayedReader_h
#define art_root_io_detail_SamplingDelayedReader_h
// vim: set sw=2:

#include "art/Framework/Principal/DelayedReader.h"
#include "art_root_io/Inputfwd.h"
#include "canvas/Persistency/Provenance/BranchType.h"
#include "canvas/Persistency/Provenance/Compatibility/BranchIDList.h"
#include "canvas/Persistency/Provenance/EventID.h"
#include "canvas/Persistency/Provenance/FileFormatVersion.h"
#include "canvas/Persistency/Provenance/RangeSet.h"
#include "canvas/Persistency/Provenance/fwd.h"
#include "cetlib/exempt_ptr.h"

#include <memory>
#include <string>

struct sqlite3;

namespace art::detail {

  class SamplingDelayedReader final : public DelayedReader {
  public:
    ~SamplingDelayedReader() = default;

    SamplingDelayedReader(SamplingDelayedReader const&) = delete;
    SamplingDelayedReader& operator=(SamplingDelayedReader const&) = delete;

    SamplingDelayedReader(FileFormatVersion version,
                          sqlite3* db,
                          std::vector<input::EntryNumber> const& entrySet,
                          input::BranchMap const& branches,
                          TBranch* provenanceBranch,
                          int64_t saveMemoryObjectThreshold,
                          cet::exempt_ptr<BranchIDLists const> branchIDLists,
                          BranchType branchType,
                          EventID const& id,
                          bool compactSubRunRanges);

    std::unique_ptr<EDProduct> getProduct(ProductID,
                                          std::string const& wrappedType,
                                          RangeSet&) const;

  private:
    std::unique_ptr<EDProduct> getProduct_(Group const*,
                                           ProductID,
                                           RangeSet&) const override;
    void setPrincipal_(cet::exempt_ptr<Principal>) override;
    std::vector<ProductProvenance> readProvenance_() const override;

    FileFormatVersion fileFormatVersion_;
    sqlite3* db_;
    std::vector<input::EntryNumber> const entrySet_;
    input::BranchMap const& branches_;
    TBranch* provenanceBranch_;
    int64_t saveMemoryObjectThreshold_;
    cet::exempt_ptr<Principal const> principal_;
    cet::exempt_ptr<BranchIDLists const>
      branchIDLists_; // Only for backwards compatibility
    BranchType branchType_;
    EventID eventID_;
    bool const compactSubRunRanges_;
  };

} // namespace art::detail

// Local Variables:
// mode: c++
// End:

#endif /* art_root_io_detail_SamplingDelayedReader_h */
