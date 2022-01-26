#ifndef art_root_io_FastCloningInfoProvider_h
#define art_root_io_FastCloningInfoProvider_h

#include "cetlib/exempt_ptr.h"

namespace art {
  class ProcessingLimits;

  class FastCloningInfoProvider {
  public:
    FastCloningInfoProvider() = default;
    explicit FastCloningInfoProvider(cet::exempt_ptr<ProcessingLimits> limits);

    bool fastCloningPermitted() const;

    int remainingEvents() const;
    int remainingSubRuns() const;

  private:
    cet::exempt_ptr<ProcessingLimits> limits_{};
  };

  inline bool
  FastCloningInfoProvider::fastCloningPermitted() const
  {
    return !limits_.empty();
  }

} // namespace art

#endif /* art_root_io_FastCloningInfoProvider_h */

// Local Variables:
// mode: c++
// End:
