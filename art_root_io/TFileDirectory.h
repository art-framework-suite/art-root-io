#ifndef art_root_io_TFileDirectory_h
#define art_root_io_TFileDirectory_h
// vim: set sw=2 expandtab :

#include "art_root_io/detail/RootDirectorySentry.h"

#include "TDirectory.h"

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

class TFile;

namespace art {

  // There are no public constructors, so can only be made by derived classes.
  class TFileDirectory {
  public:
    // Make new ROOT object of type T using args. It will be made in the current
    // directory, as established with a call to cd.
    template <typename T, typename... ARGS>
    T* make(ARGS&&... args) const;

    // Make and register a new ROOT object of type T, giving it the specified
    // name and title, using args. The type must be registerable, and must
    // support naming and titling.
    template <typename T, typename... ARGS>
    T* makeAndRegister(char const* name,
                       char const* title,
                       ARGS&&... args) const;

    template <typename T, typename... ARGS>
    T* makeAndRegister(std::string const& name,
                       std::string const& title,
                       ARGS&&... args) const;

    // Create a new TFileDirectory, sharing the same TFile as this one, but with
    // an additional dir, and with path being the absolute path of this one.
    TFileDirectory mkdir(std::string const& dir,
                         std::string const& descr = "") const;

  protected:
    using Callback_t = std::function<void()>;
    TFileDirectory(std::string const& dir,
                   std::string const& descr,
                   TFile* file,
                   std::string const& path);

    // Return the full pathname of represented directory, that is path_ + dir_.
    std::string fullPath() const;
    void registerCallback(Callback_t);
    void invokeCallbacks();

    // Protects all data members, including derived classes.
    static std::recursive_mutex mutex_;
    // The root file.
    TFile* file_;
    // Directory name in the root file.
    std::string dir_;
    // Directory title in the root file.
    std::string descr_;
    // Callbacks must exist for each directory. Used only by TFileService.
    bool requireCallback_{false};

  private:
    // Make the current directory be the one implied by the state of this
    // TFileDirectory.
    void cd() const;

    std::string path_;
    std::map<std::string, std::vector<Callback_t>> callbacks_;
  };

  template <typename T, typename... ARGS>
  T*
  TFileDirectory::make(ARGS&&... args) const
  {
    std::lock_guard lock{mutex_};
    detail::RootDirectorySentry rds;
    cd();
    return new T(std::forward<ARGS>(args)...);
  }

  template <typename T, typename... ARGS>
  T*
  TFileDirectory::makeAndRegister(char const* name,
                                  char const* title,
                                  ARGS&&... args) const
  {
    std::lock_guard lock{mutex_};
    detail::RootDirectorySentry rds;
    cd();
    auto ret = new T(std::forward<ARGS>(args)...);
    ret->SetName(name);
    ret->SetTitle(title);
    gDirectory->Append(ret);
    return ret;
  }

  template <typename T, typename... ARGS>
  T*
  TFileDirectory::makeAndRegister(std::string const& name,
                                  std::string const& title,
                                  ARGS&&... args) const
  {
    return makeAndRegister<T>(
      name.c_str(), title.c_str(), std::forward<ARGS>(args)...);
  }

} // namespace art

#endif /* art_root_io_TFileDirectory_h */

// Local Variables: -
// mode: c++ -
// c-basic-offset: 2 -
// indent-tabs-mode: nil -
// End: -
