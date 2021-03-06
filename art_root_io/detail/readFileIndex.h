#ifndef art_root_io_detail_readFileIndex_h
#define art_root_io_detail_readFileIndex_h

#include "art/Framework/Core/InputSourceMutex.h"
#include "art_root_io/Inputfwd.h"
#include "art_root_io/rootErrMsgs.h"
#include "canvas/Persistency/Provenance/FileIndex.h"
#include "canvas/Persistency/Provenance/rootNames.h"

#include "TBranch.h"
#include "TFile.h"
#include "TTree.h"

#include <memory>

// This function selects retrieves the FileIndex based on whether it
// is a branch (file format < 7) or tree (file format >= 7).

namespace art::detail {

  inline void
  readFileIndex(TFile* file, TTree* metaDataTree, FileIndex*& findexPtr)
  {
    InputSourceMutexSentry sentry;
    if (auto branch =
          metaDataTree->GetBranch(rootNames::metaBranchRootName<FileIndex>())) {
      branch->SetAddress(&findexPtr);
      input::getEntry(branch, 0);
      branch->SetAddress(nullptr);
    } else {
      std::unique_ptr<TTree> fileIndexTree{
        file->Get<TTree>(rootNames::fileIndexTreeName().c_str())};
      if (!fileIndexTree)
        throw Exception{errors::FileReadError}
          << couldNotFindTree(rootNames::fileIndexTreeName());

      FileIndex::Element element;
      auto elemPtr = &element;
      fileIndexTree->SetBranchAddress(
        rootNames::metaBranchRootName<FileIndex::Element>(), &elemPtr);
      for (size_t i{0}, sz = fileIndexTree->GetEntries(); i != sz; ++i) {
        input::getEntry(fileIndexTree.get(), i);
        findexPtr->addEntryOnLoad(elemPtr->eventID, elemPtr->entry);
      }
      fileIndexTree->SetBranchAddress(
        rootNames::metaBranchRootName<FileIndex::Element>(), nullptr);
    }
  }
} // namespace art::detail

#endif /* art_root_io_detail_readFileIndex_h */

// Local Variables:
// mode: c++
// End:
