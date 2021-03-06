#ifndef art_root_io_RootBranchInfo_h
#define art_root_io_RootBranchInfo_h

namespace art {
  class RootBranchInfo;
}

class TBranch;

#include <string>

class art::RootBranchInfo {
public:
  explicit RootBranchInfo(TBranch* branch = 0);
  std::string const& branchName() const;
  TBranch const* branch() const;
  TBranch* branch();

private:
  TBranch* branch_;
  std::string branchName_;
};

inline std::string const&
art::RootBranchInfo::branchName() const
{
  return branchName_;
}

inline TBranch const*
art::RootBranchInfo::branch() const
{
  return branch_;
}

inline TBranch*
art::RootBranchInfo::branch()
{
  return branch_;
}

#endif /* art_root_io_RootBranchInfo_h */

// Local Variables:
// mode: c++
// End:
