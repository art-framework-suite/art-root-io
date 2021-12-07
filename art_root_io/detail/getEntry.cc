#include "art/Framework/Core/InputSourceMutex.h"
#include "art_root_io/Inputfwd.h"
// vim: set sw=2 expandtab :

#include "canvas/Utilities/Exception.h"

#include "TBranch.h"
#include "TTree.h"

namespace art::input {

  Int_t
  getEntry(TBranch* branch, EntryNumber entryNumber)
  {
    InputSourceMutexSentry sentry;
    try {
      return branch->GetEntry(entryNumber);
    }
    catch (cet::exception& e) {
      throw art::Exception(art::errors::FileReadError)
        << e.explain_self() << "\n";
    }
  }

  Int_t
  getEntry(TTree* tree, EntryNumber entryNumber)
  {
    InputSourceMutexSentry sentry;
    try {
      return tree->GetEntry(entryNumber);
    }
    catch (cet::exception& e) {
      throw art::Exception(art::errors::FileReadError)
        << e.explain_self() << "\n";
    }
  }

} // namespace art::input
