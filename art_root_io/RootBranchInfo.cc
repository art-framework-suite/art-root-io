#include "art_root_io/RootBranchInfo.h"

#include "TBranch.h"

art::RootBranchInfo::RootBranchInfo(TBranch* branch)
  : branch_(branch), branchName_(branch ? branch_->GetName() : "")
{}
