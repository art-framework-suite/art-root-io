#ifndef art_root_io_TFileService_h
#define art_root_io_TFileService_h
// vim: set sw=2 expandtab :

#include "art/Framework/Core/OutputFileGranularity.h"
#include "art/Framework/IO/ClosingCriteria.h"
#include "art/Framework/IO/FileStatsCollector.h"
#include "art/Framework/IO/PostCloseFileRenamer.h"
#include "art/Framework/IO/detail/SafeFileNameConfig.h"
#include "art/Framework/Services/Registry/ServiceDeclarationMacros.h"
#include "art_root_io/TFileDirectory.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/Name.h"
#include "fhiclcpp/types/OptionalTable.h"
#include "fhiclcpp/types/TableFragment.h"

#include <chrono>
#include <string>

namespace art {

  class ActivityRegistry;
  class ModuleContext;
  class ModuleDescription;

  class TFileService : public TFileDirectory {
  public:
    static constexpr const char* default_tmpDir = "<parent-path-of-filename>";
    using Callback_t = TFileDirectory::Callback_t;

    struct Config {
      fhicl::Atom<bool> closeFileFast{fhicl::Name("closeFileFast"), true};
      fhicl::Atom<std::string> fileName{
        fhicl::Name("fileName"),
        fhicl::Comment(
          R"(The "fileName" parameter is a pattern used to form the name of
the output file.  It can be a literal string, or it can contain the
placeholders as described at:

  https://cdcvs.fnal.gov/redmine/projects/art_root_io/wiki/Output_file_renaming_for_ROOT_files
)")};
      fhicl::Atom<std::string> tmpDir{fhicl::Name("tmpDir"), default_tmpDir};
      fhicl::OptionalTable<ClosingCriteria::Config> fileProperties{
        fhicl::Name("fileProperties")};
      fhicl::TableFragment<detail::SafeFileNameConfig> safeFileName;
    };
    using Parameters = ServiceTable<Config>;

    ~TFileService();
    TFileService(Parameters const&, ActivityRegistry&);
    TFileService(TFileService const&) = delete;
    TFileService(TFileService&&) = delete;
    TFileService& operator=(TFileService const&) = delete;
    TFileService& operator=(TFileService&&) = delete;

    void registerFileSwitchCallback(Callback_t);
    template <typename T>
    void registerFileSwitchCallback(T* provider, void (T::*)());

    TFile&
    file() const
    {
      std::lock_guard lock{mutex_};
      return *file_;
    }

  private:
    // set current directory according to module name and prepare to create
    // directory
    void setDirectoryName_(art::ModuleDescription const&);
    void setDirectoryNameViaContext_(art::ModuleContext const&);
    void openFile_();
    void closeFile_();
    void maybeSwitchFiles_();
    bool requestsToCloseFile_();
    std::string fileNameAtOpen_();
    std::string fileNameAtClose_(std::string const&);

    bool const closeFileFast_;
    FileStatsCollector fstats_;
    PostCloseFileRenamer fRenamer_{fstats_};
    std::string filePattern_;
    std::string uniqueFilename_;
    std::string tmpDir_;

    // File-switching mechanics
    std::string lastClosedFile_{};
    Granularity currentGranularity_{Granularity::Unset};
    std::chrono::steady_clock::time_point beginTime_{};
    // Do not use default-constructed "ClosingCriteria" as that will be
    // sure to trigger a file switch at the first call of
    // requestsToCloseFile_().
    ClosingCriteria fileSwitchCriteria_{ClosingCriteria::Config{}};
    OutputFileStatus status_{OutputFileStatus::Closed};
    FileProperties fp_{};
  };

  template <typename T>
  void
  TFileService::registerFileSwitchCallback(T* provider, void (T::*f)())
  {
    std::lock_guard lock{mutex_};
    registerFileSwitchCallback([provider, f] { return (provider->*f)(); });
  }

} // namespace art

DECLARE_ART_SERVICE(art::TFileService, SHARED)

#endif /* art_root_io_TFileService_h */

// Local Variables:
// mode: c++
// End:
