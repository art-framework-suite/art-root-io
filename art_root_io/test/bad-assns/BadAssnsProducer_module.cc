////////////////////////////////////////////////////////////////////////
// Class:       BadAssnsProducer
// Plugin Type: producer (art v1_19_00_rc3)
// File:        BadAssnsProducer_module.cc
//
// Generated at Thu Apr 14 08:54:19 2016 by Christopher Green using cetskelgen
// from cetlib version v1_17_04.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Principal/Event.h"
#include "art/test/TestObjects/ToyProducts.h"
#include "art_root_io/test/bad-assns/DummyA.h"
#include "canvas/Persistency/Common/Assns.h"
#include "fhiclcpp/fwd.h"

#include <memory>

namespace arttest {
  class BadAssnsProducer;
}

class arttest::BadAssnsProducer : public art::EDProducer {
public:
  explicit BadAssnsProducer(fhicl::ParameterSet const& p);
  // The compiler-generated destructor is fine for non-base
  // classes without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  BadAssnsProducer(BadAssnsProducer const&) = delete;
  BadAssnsProducer(BadAssnsProducer&&) = delete;
  BadAssnsProducer& operator=(BadAssnsProducer const&) = delete;
  BadAssnsProducer& operator=(BadAssnsProducer&&) = delete;

  // Required functions.
  void produce(art::Event& e) override;

private:
};

arttest::BadAssnsProducer::BadAssnsProducer(fhicl::ParameterSet const& ps)
  : EDProducer{ps}
{
  produces<art::Assns<DummyA, DummyProduct>>();
}

void
arttest::BadAssnsProducer::produce(art::Event& e)
{
  e.put(std::make_unique<art::Assns<DummyA, DummyProduct>>());
}

DEFINE_ART_MODULE(arttest::BadAssnsProducer)
