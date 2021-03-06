#ifndef art_root_io_detail_dropBranch_h
#define art_root_io_detail_dropBranch_h

class TTree;

#include <string>

namespace art::detail {
  void dropBranch(TTree* tree, std::string const& branchName);
}

#endif /* art_root_io_detail_dropBranch_h */

// Local Variables:
// mode: c++
// End:
