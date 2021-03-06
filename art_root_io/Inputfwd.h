#ifndef art_root_io_Inputfwd_h
#define art_root_io_Inputfwd_h
// vim: set sw=2 expandtab :

#include "Rtypes.h"

#include <map>
#include <vector>

class TBranch;
class TTree;

namespace art {

  class BranchDescription;
  struct FileFormatVersion;
  class ProductID;
  class RootInputFile;
  class RootDelayedReader;
  class RootInputTree;
  class RootInputFileSequence;
  class FileCatalogItem;
  class RootInput;

  namespace input {

    struct BranchInfo {
      BranchInfo(BranchDescription const& prod, TBranch* const branch)
        : branchDescription_{prod}, productBranch_{branch}
      {}

      // Ideally, a reference to the branch-description does not need
      // to be retained.  It is used to fill the groups in the
      // principal.
      BranchDescription const& branchDescription_;
      TBranch* productBranch_;
    };

    using BranchMap = std::map<ProductID, BranchInfo>;
    using EntryNumber = Long64_t;
    using EntryNumbers = std::vector<EntryNumber>;
    Int_t getEntry(TBranch* branch, EntryNumber entryNumber);
    Int_t getEntry(TTree* tree, EntryNumber entryNumber);

  } // namespace input
} // namespace art

#endif /* art_root_io_Inputfwd_h */

// Local Variables:
// mode: c++
// End:
