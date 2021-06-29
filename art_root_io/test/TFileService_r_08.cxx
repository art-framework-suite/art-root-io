#include "TDirectory.h"
#include "TFile.h"
#include "TGraph.h"
#include "TGraphPolar.h"
#include "TH1F.h"
#include "TH2F.h"

#include <iostream>

using namespace std;

int
TFileService_r_08()
{
  TFile* f = TFile::Open("../TFileService_t_08.d/tfile_output.root");
  if (f == nullptr) {
    return 1;
  }
  // Having obtained 'f', we never use it again. We use the global
  // variable gDirectory, because that's what the 'cd' calls
  // manipulate. Really.
  // Make sure we have a directory named by our module label, "hist".
  if (not gDirectory->cd("hist")) {
    return 1;
  }
  // Make sure we have the subdirectory "a", containing the TH1F
  // histogram named "test1".

  if (not gDirectory->cd("a")) {
    return 2;
  }
  if (gDirectory->Get<TH1F>("test1") == nullptr) {
    cerr << "test1 not found\n";
    return 1;
  }
  if (gDirectory->Get<TH1F>("z") != nullptr) {
    cerr << "z incorrectly found\n";
    return 1;
  }
  if (gDirectory->Get<TH2F>("test1") != nullptr) {
    cerr << "test1 incorrectly identified as a TH2F\n";
    return 1;
  }
  // Make sure we have the subdirectory "b", containing the TH2F
  // histogram named "test2".
  if (not gDirectory->cd("../b")) {
    return 3;
  }
  if (gDirectory->Get<TH2F>("test2") == nullptr) {
    cerr << "test2 not found\n";
    return 3;
  }
  // Make sure we have the subdirectory "respondToOpenInputFile",
  // containing the TH1F histogrm named "test3"
  if (not gDirectory->cd("../respondToOpenInputFile")) {
    return 4;
  }
  if (gDirectory->Get<TH1F>("test3") == nullptr) {
    cerr << "test3 not found\n";
    return 4;
  }
  // Make sure the top-level directory contains a TGraph named
  // "graphAtTopLevel".
  if (not gDirectory->cd("/hist")) {
    return 5;
  }
  auto pgraph = gDirectory->Get<TGraph>("graphAtTopLevel");
  if (pgraph == nullptr) {
    cerr << "graphAtTopLevel not found\n";
    return 5;
  }
  string title = pgraph->GetTitle();
  if (title != "graph at top level") {
    cerr << "TGraph at top level not recovered with correct title\n";
    return 5;
  }
  // Make sure the direcotyr "b" contains a TGraph named
  // "graphInSubdirectory"
  if (not gDirectory->cd("b")) {
    return 6;
  }
  auto ppolar = gDirectory->Get<TGraphPolar>("graphInSubdirectory");
  if (ppolar == nullptr) {
    cerr << "graphInSubdirectory not found\n";
    return 6;
  }
  title = ppolar->GetTitle();
  if (title != "graph in subdirectory") {
    cerr << "TGraph in subdirectory not recovered with correct title\n";
    return 6;
  }
  return 0;
}

int
main()
{
  return TFileService_r_08();
}
