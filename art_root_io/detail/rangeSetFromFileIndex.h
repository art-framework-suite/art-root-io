#ifndef art_root_io_detail_rangeSetFromFileIndex_h
#define art_root_io_detail_rangeSetFromFileIndex_h

#include "canvas/Persistency/Provenance/fwd.h"

namespace art::detail {
  RangeSet rangeSetFromFileIndex(FileIndex const& fileIndex,
                                 RunID runID,
                                 bool compactRanges);

  RangeSet rangeSetFromFileIndex(FileIndex const& fileIndex,
                                 SubRunID subRunID,
                                 bool compactRanges);
}

#endif /* art_root_io_detail_rangeSetFromFileIndex_h */

// Local Variables:
// mode: c++
// End:
