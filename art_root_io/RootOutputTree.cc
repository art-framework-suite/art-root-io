#include "art_root_io/RootOutputTree.h"
// vim: set sw=2:

#include "canvas/Persistency/Common/EDProduct.h"
#include "canvas/Persistency/Provenance/BranchDescription.h"
#include "canvas/Utilities/Exception.h"
#include "cetlib/container_algorithms.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "RtypesCore.h"
#include "TBranch.h"
#include "TClass.h"
#include "TClassRef.h"
#include "TFile.h"
#include "TTreeCloner.h"

#include <atomic>
#include <limits>
#include <string>

namespace art {

  TTree*
  RootOutputTree::makeTTree(TFile* filePtr,
                            std::string const& name,
                            int const splitLevel)
  {
    TTree* tree = new TTree(name.c_str(), "", splitLevel);
    if (!tree) {
      throw Exception{errors::FatalRootError}
        << "Failed to create the tree: " << name << "\n";
    }
    if (tree->IsZombie()) {
      throw Exception{errors::FatalRootError} << "Tree: " << name
                                              << " is a zombie.\n";
    }
    tree->SetDirectory(filePtr);
    // Turn off autosave because it leaves too many deleted tree
    // keys in the output file.
    tree->SetAutoSave(std::numeric_limits<Long64_t>::max());
    return tree;
  }

  bool
  RootOutputTree::checkSplitLevelAndBasketSize(
    cet::exempt_ptr<TTree const> inputTree) const
  {
    // Do the split level and basket size match in the input and output?
    if (inputTree == nullptr) {
      return false;
    }
    for (auto outputBranch : readBranches_) {
      if (outputBranch == nullptr) {
        continue;
      }
      TBranch* inputBranch =
        const_cast<TTree*>(inputTree.get())->GetBranch(outputBranch->GetName());
      if (inputBranch == nullptr) {
        continue;
      }
      if ((inputBranch->GetSplitLevel() != outputBranch->GetSplitLevel()) ||
          (inputBranch->GetBasketSize() != outputBranch->GetBasketSize())) {
        return false;
      }
    }
    return true;
  }

  void
  RootOutputTree::writeTTree(TTree* tree) noexcept(false)
  {
    // Update the tree-level entry count because we have been using
    // branch fill instead of tree fill.
    if (tree->GetNbranches() != 0) {
      tree->SetEntries(-1);
    }
    // Use auto save here instead of write because it deletes the old
    // tree key from the file, does not flush the baskets, and writes
    // out the streamer infos, unlike write.
    tree->AutoSave();
  }

  void
  RootOutputTree::writeTree() const
  {
    writeTTree(tree_.load());
    writeTTree(metaTree_.load());
  }

  bool
  RootOutputTree::fastCloneTree(cet::exempt_ptr<TTree const> intree)
  {
    unclonedReadBranches_.clear();
    unclonedReadBranchNames_.clear();

    fastCloningEnabled_ = false;
    if (intree->GetEntries() != 0) {
      auto event_tree = const_cast<TTree*>(intree.get());

      // Remove EventAuxiliary branches from fast cloning so we can
      // update the stored ProcessHistoryID.
      auto branches = event_tree->GetListOfBranches();
      auto aux_branch = event_tree->GetBranch("EventAuxiliary");
      assert(aux_branch);
      auto const aux_index = branches->IndexOf(aux_branch);
      assert(aux_index >= 0);
      branches->RemoveAt(aux_index);
      branches->Compress();

      TTreeCloner cloner(event_tree,
                         tree_.load(),
                         "",
                         TTreeCloner::kIgnoreMissingTopLevel |
                           TTreeCloner::kNoWarnings |
                           TTreeCloner::kNoFileCache);
      if (cloner.IsValid()) {
        tree_.load()->SetEntries(tree_.load()->GetEntries() +
                                 intree->GetEntries());
        cloner.Exec();
        fastCloningEnabled_ = true;
      } else {
        mf::LogInfo("fastCloneTree")
          << "INFO: Unable to fast clone tree " << intree->GetName() << '\n'
          << "INFO: ROOT reason is:\n"
          << "INFO: " << cloner.GetWarning() << '\n'
          << "INFO: Processing will continue, tree will be slow cloned.";
      }

      // Add EventAuxiliary branch back
      auto last = branches->GetLast();
      if (last >= 0) {
        branches->AddAtAndExpand(branches->At(last), last + 1);
        for (Int_t ind = last - 1; ind >= aux_index; --ind) {
          branches->AddAt(branches->At(ind), ind + 1);
        }
        branches->AddAt(aux_branch, aux_index);
      } else {
        branches->Add(aux_branch);
      }
    }
    for (auto branch : readBranches_) {
      if (branch->GetEntries() != tree_.load()->GetEntries()) {
        unclonedReadBranches_.push_back(branch);
        unclonedReadBranchNames_.push_back(branch->GetName());
      }
    }
    cet::sort_all(unclonedReadBranchNames_);
    return fastCloningEnabled_;
  }

  namespace {
    void
    fillBranches(std::vector<TBranch*> const& branches,
                 bool const saveMemory,
                 int64_t const threshold)
    {
      for (auto const b : branches) {
        auto bytesWritten = b->Fill();
        if (saveMemory and bytesWritten > threshold) {
          b->FlushBaskets();
          b->DropBaskets("all");
        }
      }
    }
  }

  void
  RootOutputTree::fillTree()
  {
    fillBranches(metaBranches_, false, saveMemoryObjectThreshold_);
    bool const saveMemory{saveMemoryObjectThreshold_ > -1};
    fillBranches(producedBranches_, saveMemory, saveMemoryObjectThreshold_);
    if (fastCloningEnabled_.load()) {
      fillBranches(
        unclonedReadBranches_, saveMemory, saveMemoryObjectThreshold_);
    } else {
      fillBranches(readBranches_, saveMemory, saveMemoryObjectThreshold_);
    }
    ++nEntries_;
  }

  void
  RootOutputTree::resetOutputBranchAddress(BranchDescription const& bd)
  {
    TBranch* br = tree_.load()->GetBranch(bd.branchName().c_str());
    if (br == nullptr) {
      return;
    }
    tree_.load()->ResetBranchAddress(br);
  }

  // Note: RootOutputFile::selectProducts() calls this on all products
  //       seleted for output, to reset the branch address, or create
  //       a new branch as needed.
  void
  RootOutputTree::addOutputBranch(BranchDescription const& bd,
                                  void const*& pProd)
  {
    TClassRef cls = TClass::GetClass(bd.wrappedName().c_str());
    if (TBranch* br = tree_.load()->GetBranch(bd.branchName().c_str())) {
      // Already have this branch, possibly update the branch address.
      if (pProd == nullptr) {
        // The OutputItem is freshly constructed and has not been
        // passed to SetAddress yet.  If selectProducts has just been
        // called, we get here just after the branch object has been
        // deleted with a ResetBranchAddress() to prepare for the
        // OutputItem being replaced, and the OutputItem has just been
        // recreated.
        EDProduct* prod = reinterpret_cast<EDProduct*>(cls->New());
        pProd = prod;
        br->SetAddress(&pProd);
        pProd = nullptr;
        delete prod;
      }
      return;
    }
    auto bsize = bd.basketSize();
    if (bsize == BranchDescription::invalidBasketSize) {
      bsize = basketSize_;
    }
    auto splitlvl = bd.splitLevel();
    if (splitlvl == BranchDescription::invalidSplitLevel) {
      splitlvl = splitLevel_;
    }

    if (pProd != nullptr) {
      throw art::Exception(art::errors::FatalRootError)
        << "OutputItem product pointer is not nullptr!\n";
    }
    auto* prod = reinterpret_cast<EDProduct*>(cls->New());
    pProd = prod;
    auto* branch = tree_.load()->Branch(bd.branchName().c_str(),
                                        bd.wrappedName().c_str(),
                                        &pProd,
                                        bsize,
                                        splitlvl);
    // Note that root will have just allocated a dummy product as the
    // I/O buffer for the branch we have created.  We will replace
    // this I/O buffer in RootOutputFile::fillBranches() with the
    // actual product or our own dummy using
    // TBranchElement::SetAddress(), which will cause root to
    // automatically delete the dummy product it allocated here.
    pProd = nullptr;
    delete prod;

    if (bd.compression() != BranchDescription::invalidCompression) {
      branch->SetCompressionSettings(bd.compression());
    }

    if (nEntries_.load() > 0) {
      // Backfill the branch with dummy entries to match the number of
      // entries already written to the data tree.
      std::unique_ptr<EDProduct> dummy{static_cast<EDProduct*>(cls->New())};
      pProd = dummy.get();
      int bytesWritten{};
      for (auto i = nEntries_.load(); i > 0; --i) {
        auto cnt = branch->Fill();
        if (cnt <= 0) {
          // FIXME: Throw a fatal error here!
        }
        bytesWritten += cnt;
        if ((saveMemoryObjectThreshold_ > -1) &&
            (bytesWritten > saveMemoryObjectThreshold_)) {
          branch->FlushBaskets();
          branch->DropBaskets("all");
          bytesWritten = 0;
        }
      }
    }
    if (bd.produced()) {
      producedBranches_.push_back(branch);
    } else {
      readBranches_.push_back(branch);
    }
  }

} // namespace art
