#ifndef art_root_io_RootDB_TKeyVFSOpenPolicy_h
#define art_root_io_RootDB_TKeyVFSOpenPolicy_h

class TFile;

#include <sqlite3.h>
#include <string>

namespace art {

  class TKeyVFSOpenPolicy {
  public:
    explicit TKeyVFSOpenPolicy(TFile* tfile, int flags = SQLITE_OPEN_READONLY);

    sqlite3* open(std::string const& key);

  private:
    TFile* tfile_;
    int flags_;
  };
} // namespace art

#endif /* art_root_io_RootDB_TKeyVFSOpenPolicy_h */

// Local Variables:
// mode: c++
// End:
