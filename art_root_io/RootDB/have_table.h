#ifndef art_root_io_RootDB_have_table_h
#define art_root_io_RootDB_have_table_h

#include <string>

struct sqlite3;

namespace art {
  bool have_table(sqlite3* db,
                  std::string const& table,
                  std::string const& filename);
}

#endif /* art_root_io_RootDB_have_table_h */

// Local Variables:
// mode: c++
// End:
