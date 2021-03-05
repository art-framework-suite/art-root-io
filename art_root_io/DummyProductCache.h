#ifndef art_root_io_DummyProductCache_h
#define art_root_io_DummyProductCache_h

#include "canvas/Persistency/Common/EDProduct.h"

#include <map>
#include <memory>
#include <string>

namespace art {
    class DummyProductCache {
    public:
      EDProduct const* product(std::string const& wrappedName);

    private:
      std::map<std::string, std::unique_ptr<EDProduct>> dummies_;
    };
} // namespace art

#endif /* art_root_io_DummyProductCache_h */

// Local variables:
// mode: c++
// End:
