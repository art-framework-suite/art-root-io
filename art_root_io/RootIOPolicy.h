#ifndef art_root_io_RootIOPolicy_h
#define art_root_io_RootIOPolicy_h

////////////////////////////////////////////////////////////////////////
// RootIOPolicy for mixing operations
////////////////////////////////////////////////////////////////////////

#include "art/Framework/IO/ProductMix/MixIOPolicy.h"
#include "art_root_io/RootBranchInfoList.h"
#include "canvas/Persistency/Provenance/FileFormatVersion.h"

#include "TFile.h"

#include <array>

class TTree;

namespace art {

  class RootIOPolicy : public MixIOPolicy {
    std::size_t
    nEventsInFile() const override
    {
      return nEventsInCurrentFile_;
    }

    EventAuxiliarySequence generateEventAuxiliarySequence(
      EntryNumberSequence const&) override;

    bool
    fileOpen() const override
    {
      return ffVersion_.isValid();
    }

    FileIndex const&
    fileIndex() const override
    {
      return fileIndexInCurrentFile_;
    }

    cet::exempt_ptr<BranchIDLists const>
    branchIDLists() const override
    {
      return branchIDListsInCurrentFile_.get();
    }

    void openAndReadMetaData(std::string fileName, MixOpList& mixOps) override;
    SpecProdList readFromFile(MixOpBase const& mixOp,
                              EntryNumberSequence const& seq) override;

    std::unique_ptr<TFile> currentFile_{};
    cet::exempt_ptr<TTree> currentMetaDataTree_{nullptr};
    std::array<cet::exempt_ptr<TTree>, art::BranchType::NumBranchTypes>
      currentDataTrees_{{nullptr}};
    std::array<RootBranchInfoList, art::BranchType::NumBranchTypes>
      dataBranches_{{}};
    std::size_t nEventsInCurrentFile_{};
    FileFormatVersion ffVersion_{};
    FileIndex fileIndexInCurrentFile_{};
    EventIDIndex eventIDIndexInCurrentFile_{};
    std::unique_ptr<BranchIDLists const> branchIDListsInCurrentFile_{nullptr};
    std::map<ProductID, RootBranchInfo> branchInfos_{};
  };
}
#endif /* art_root_io_RootIOPolicy_h */

// Local Variables:
// mode: c++
// End:
