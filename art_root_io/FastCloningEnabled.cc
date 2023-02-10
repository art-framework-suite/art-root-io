#include "art_root_io/FastCloningEnabled.h"
#include "cetlib/trim.h"

#include <sstream>

namespace art {
  FastCloningEnabled::operator bool() const noexcept
  {
    return empty(msgs_);
  }

  void
  FastCloningEnabled::disable(std::string reason)
  {
    msgs_.push_back(std::move(reason));
  }

  void
  FastCloningEnabled::merge(FastCloningEnabled enabled)
  {
    msgs_.insert(end(msgs_), begin(enabled.msgs_), end(enabled.msgs_));
  }

  std::string
  FastCloningEnabled::disabledBecause() const
  {
    std::ostringstream oss;
    oss << "Fast cloning has been deactivated for the following reasons:\n";
    for (auto const& msg : msgs_) {
      oss << " - " << msg << '\n';
    }
    return cet::trim_right_copy(oss.str(), "\n");
  }
}
