#include "art_root_io/TFileDirectory.h"
#include "art_root_io/setup.h"
// vim: set sw=2 expandtab :

#include "canvas/Utilities/Exception.h"

#include "TDirectory.h"
#include "TFile.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace std;
using namespace std::string_literals;

namespace art {

  TFileDirectory::TFileDirectory(string const& dir,
                                 string const& descr,
                                 TFile* file,
                                 string const& path)
    : file_{(root::setup(), file)}, dir_{dir}, descr_{descr}, path_{path}
  {}

  string
  TFileDirectory::fullPath() const
  {
    std::lock_guard lock{mutex_};
    if (path_.empty()) {
      return dir_;
    }
    return path_ + "/"s + dir_;
  }

  void
  TFileDirectory::cd() const
  {
    std::lock_guard lock{mutex_};
    auto const fpath = fullPath();
    if (requireCallback_ and callbacks_.find(dir_) == callbacks_.end()) {
      throw Exception{errors::Configuration,
                      "A TFileService error occured while attempting to make "
                      "a directory or ROOT object.\n"}
        << "File-switching has been enabled for TFileService.  All modules "
           "must register\n"
        << "a callback function to be invoked whenever a file switch occurs. "
           " The callback\n"
        << "must ensure that any pointers to ROOT objects have been "
           "updated.\n\n"
        << "  No callback has been registered for directory '" << dir_
        << "'.\n\n"
        << "Contact artists@fnal.gov for guidance.";
    }
    TDirectory* dir = file_->GetDirectory(fpath.c_str());
    if (dir == nullptr) {
      if (!path_.empty()) {
        dir = file_->GetDirectory(path_.c_str());
        if (dir == nullptr) {
          throw cet::exception("InvalidDirectory")
            << "Can't change directory to path: " << path_;
        }
      } else {
        dir = file_;
      }
      auto newdir = dir->mkdir(dir_.c_str(), descr_.c_str());
      if (newdir == nullptr) {
        throw cet::exception("InvalidDirectory")
          << "Can't create directory " << dir_ << " in path: " << path_;
      }
    }
    auto ok = file_->cd(fpath.c_str());
    if (!ok) {
      throw cet::exception("InvalidDirectory")
        << "Can't change directory to path: " << fpath;
    }
  }

  TFileDirectory
  TFileDirectory::mkdir(string const& dir, string const& descr)
  {
    std::lock_guard lock{mutex_};
    detail::RootDirectorySentry rds;
    cd();
    return TFileDirectory{dir, descr, file_, fullPath()};
  }

  void
  TFileDirectory::invokeCallbacks()
  {
    std::lock_guard lock{mutex_};
    for (auto const& [dir, callbacks] : callbacks_) {
      dir_ = dir;
      for (auto const& cb : callbacks) {
        cb();
      }
    }
  }

  void
  TFileDirectory::registerCallback(Callback_t cb)
  {
    std::lock_guard lock{mutex_};
    callbacks_[dir_].push_back(cb);
  }

} // namespace art
