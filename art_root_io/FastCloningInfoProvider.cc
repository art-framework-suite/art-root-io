#include "art_root_io/FastCloningInfoProvider.h"

#include "art/Framework/Core/ProcessingLimits.h"
#include "canvas/Utilities/Exception.h"

namespace art {

  FastCloningInfoProvider::FastCloningInfoProvider(
    cet::exempt_ptr<ProcessingLimits> limits)
    : limits_{limits}
  {}

  int
  FastCloningInfoProvider::remainingEvents() const
  {
    if (!fastCloningPermitted()) {
      throw Exception(errors::LogicError)
        << "FastCloningInfoProvider::remainingEvents() has no meaning"
        << " in this context:\n"
        << "Check fastCloningPermitted() first.\n";
    }
    return limits_->remainingEvents();
  }

  int
  FastCloningInfoProvider::remainingSubRuns() const
  {
    if (!fastCloningPermitted()) {
      throw Exception(errors::LogicError)
        << "FastCloningInfoProvider::remainingSubRuns() has no meaning"
        << " in this context:\n"
        << "Check fastCloningPermitted() first.\n";
    }
    return limits_->remainingSubRuns();
  }

}
