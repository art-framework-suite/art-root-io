#ifndef art_root_io_RootDB_SQLite3Wrapper_h
#define art_root_io_RootDB_SQLite3Wrapper_h

// Sentry-like entity to manage the lifetime of an SQL database handle.
#include <string>

class TFile;

#include <sqlite3.h>

namespace art {
  class SQLite3Wrapper;
}

class art::SQLite3Wrapper {
public:
  SQLite3Wrapper(SQLite3Wrapper const&) = delete;
  SQLite3Wrapper& operator=(SQLite3Wrapper const&) = delete;

  using callback_t = int (*)(void*, int, char**, char**);

  // A default constructed wrapper is not associated with an SQLite database.
  SQLite3Wrapper();

  // Create a database connected to the file named by 'key';
  explicit SQLite3Wrapper(std::string const& key,
                          int flags = SQLITE_OPEN_READONLY);

  SQLite3Wrapper(TFile* tfile,
                 std::string const& key,
                 int flags = SQLITE_OPEN_READWRITE);

  std::string const&
  key() const
  {
    return key_;
  }

  operator sqlite3*() { return db_; }

  // Query the environment variable ART_DEBUG_SQL to determine whether or
  // not we should do tracing of SQLite interface calls.
  static bool wantTracing();

  void reset();

  void reset(std::string const& key, int flags = SQLITE_OPEN_READONLY);

  void reset(TFile* tfile,
             std::string const& key,
             int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_TRANSIENT_DB);

  ~SQLite3Wrapper();

  void swap(SQLite3Wrapper& other);

private:
  void initDB(int flags, TFile* tfile = nullptr);

  void maybeTrace() const;

  sqlite3* db_{nullptr};
  std::string key_{};
};

#endif /* art_root_io_RootDB_SQLite3Wrapper_h */

// Local Variables:
// mode: c++
// End:
