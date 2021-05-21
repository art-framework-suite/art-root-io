#ifndef art_root_io_detail_resolveRangeSet_h
#define art_root_io_detail_resolveRangeSet_h

#include "art_root_io/detail/RangeSetInfo.h"
#include "canvas/Persistency/Provenance/BranchType.h"
#include "canvas/Persistency/Provenance/RangeSet.h"

#include <string>

struct sqlite3;

namespace art::detail {

  RangeSetInfo resolveRangeSetInfo(sqlite3*,
                                   std::string const& filename,
                                   BranchType,
                                   unsigned RangeSetID,
                                   bool compact);

  RangeSet resolveRangeSet(RangeSetInfo const& rs);

  RangeSet resolveRangeSet(sqlite3*,
                           std::string const& filename,
                           BranchType,
                           unsigned rangeSetID,
                           bool compact);
}

#endif /* art_root_io_detail_resolveRangeSet_h */

// Local variables:
// mode: c++
// End:
