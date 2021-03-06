#include "art_root_io/RootBranchInfoList.h"

#include "canvas/Utilities/Exception.h"
#include "canvas/Utilities/InputTag.h"

#include <regex>

#include "TBranch.h"
#include "TObjArray.h"
#include "TTree.h"

art::RootBranchInfoList::RootBranchInfoList() = default;

art::RootBranchInfoList::RootBranchInfoList(TTree* const tree)
{
  reset(tree);
}

void
art::RootBranchInfoList::reset(TTree* const tree)
{
  if (!tree) {
    throw Exception(errors::NullPointerError)
      << "RootInfoBranchList given null TTree pointer.\n";
  }
  TObjArray* branches = tree->GetListOfBranches();
  size_t nBranches = branches->GetEntriesFast();
  data_.clear();
  data_.reserve(nBranches);
  TIter it(branches, kIterBackward);
  // Load the list backward, then searches can take place in the forward
  // direction.
  while (auto b = dynamic_cast<TBranch*>(it.Next())) {
    data_.emplace_back(b);
  }
  if (nBranches != data_.size()) {
    throw Exception(errors::DataCorruption, "RootBranchInfoList")
      << "Could not read expected number of branches from TTree's list.\n";
  }
}

bool
art::RootBranchInfoList::findBranchInfo(TypeID const& type,
                                        InputTag const& tag,
                                        RootBranchInfo& rbInfo) const
{
  std::ostringstream pat_s;
  pat_s << '^' << type.friendlyClassName() << '_' << tag.label() << '_'
        << tag.instance() << '_';
  if (tag.process().empty()) {
    pat_s << ".*";
  } else {
    pat_s << tag.process();
  }
  pat_s << "\\.$";
  std::regex const r{pat_s.str()};
  // data_ is ordered so that the first match is the best.
  for (auto const& datum : data_) {
    if (std::regex_match(datum.branchName(), r)) {
      rbInfo = datum;
      return true;
    }
  }
  return false;
}
