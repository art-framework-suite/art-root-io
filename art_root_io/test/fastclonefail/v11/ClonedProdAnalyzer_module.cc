#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art_root_io/test/fastclonefail/v11/ClonedProd.h"

namespace art::test {

  class ClonedProdAnalyzer : public EDAnalyzer {
  public:
    struct Config {};
    using Parameters = Table<Config>;
    explicit ClonedProdAnalyzer(Parameters const&);

  private:
    void analyze(Event const&) override;
  };

  ClonedProdAnalyzer::ClonedProdAnalyzer(Parameters const& ps) : EDAnalyzer{ps}
  {}

  void
  ClonedProdAnalyzer::analyze(Event const& e)
  {
    auto clonedProd = e.getProduct<ClonedProd>("ClonedProdProducer");
    assert(clonedProd.value == 3.);
  }

} // namespace arttest

DEFINE_ART_MODULE(art::test::ClonedProdAnalyzer)
