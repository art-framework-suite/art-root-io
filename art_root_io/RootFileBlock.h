#ifndef art_root_io_RootFileBlock_h
#define art_root_io_RootFileBlock_h

// =======================================================================
// RootFileBlock: Properties of a ROOT input file.
// =======================================================================

#include "art/Framework/Core/FileBlock.h"
#include "art_root_io/FastCloningEnabled.h"
#include "cetlib/exempt_ptr.h"

#include <memory>
#include <string>

class TTree;

namespace art {

  class RootFileBlock : public FileBlock {
  public:
    RootFileBlock(FileFormatVersion const& version,
                  std::string const& fileName,
                  std::unique_ptr<ResultsPrincipal>&& resp,
                  cet::exempt_ptr<TTree const> ev,
                  FastCloningEnabled fastCopy)
      : FileBlock{version, fileName, std::move(resp)}
      , tree_{ev}
      , fastCopyable_{std::move(fastCopy)}
    {}

    cet::exempt_ptr<TTree const>
    tree() const
    {
      return tree_;
    }
    FastCloningEnabled const&
    fastClonable() const noexcept
    {
      return fastCopyable_;
    }

  private:
    cet::exempt_ptr<TTree const> tree_; // ROOT owns the tree
    FastCloningEnabled fastCopyable_;
  };
} // namespace art

#endif /* art_root_io_RootFileBlock_h */

// Local Variables:
// mode: c++
// End:
