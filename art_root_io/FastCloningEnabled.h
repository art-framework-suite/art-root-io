#ifndef art_root_io_FastCloningEnabled_h
#define art_root_io_FastCloningEnabled_h

#include <string>
#include <vector>

namespace art {
  class FastCloningEnabled {
  public:
    explicit operator bool() const noexcept;
    void disable(std::string reason);
    void merge(FastCloningEnabled other);
    std::string disabledBecause() const;

  private:
    std::vector<std::string> msgs_;
  };
}

#endif /* art_root_io_FastCloningEnabled_h */

// Local Variables:
// mode: c++
// End:
