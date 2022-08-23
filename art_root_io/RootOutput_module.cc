// vim: set sw=2 expandtab :

#include "art/Framework/Core/OutputModule.h"
#include "art/Framework/Core/RPManager.h"
#include "art/Framework/Core/ResultsProducer.h"
#include "art/Framework/IO/ClosingCriteria.h"
#include "art/Framework/IO/FileStatsCollector.h"
#include "art/Framework/IO/PostCloseFileRenamer.h"
#include "art/Framework/IO/detail/SafeFileNameConfig.h"
#include "art/Framework/IO/detail/logFileAction.h"
#include "art/Framework/IO/detail/validateFileNamePattern.h"
#include "art/Framework/Principal/EventPrincipal.h"
#include "art/Framework/Principal/ResultsPrincipal.h"
#include "art/Framework/Principal/RunPrincipal.h"
#include "art/Framework/Principal/SubRunPrincipal.h"
#include "art/Utilities/Globals.h"
#include "art/Utilities/parent_path.h"
#include "art/Utilities/unique_filename.h"
#include "art_root_io/DropMetaData.h"
#include "art_root_io/FastCloningEnabled.h"
#include "art_root_io/RootFileBlock.h"
#include "art_root_io/RootOutputFile.h"
#include "art_root_io/detail/rootOutputConfigurationTools.h"
#include "art_root_io/setup.h"
#include "canvas/Persistency/Provenance/ProductTables.h"
#include "canvas/Utilities/Exception.h"
#include "fhiclcpp/ParameterSet.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/ConfigurationTable.h"
#include "fhiclcpp/types/OptionalAtom.h"
#include "fhiclcpp/types/Table.h"
#include "fhiclcpp/types/TableFragment.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

using namespace std;
using namespace hep::concurrency;

namespace {
  string const dev_null{"/dev/null"};

  bool
  maxCriterionSpecified(art::ClosingCriteria const& cc)
  {
    auto fp = mem_fn(&art::ClosingCriteria::fileProperties);
    return (fp(cc).nEvents() !=
            art::ClosingCriteria::Defaults::unsigned_max()) ||
           (fp(cc).nSubRuns() !=
            art::ClosingCriteria::Defaults::unsigned_max()) ||
           (fp(cc).nRuns() != art::ClosingCriteria::Defaults::unsigned_max()) ||
           (fp(cc).size() != art::ClosingCriteria::Defaults::size_max()) ||
           (fp(cc).age().count() !=
            art::ClosingCriteria::Defaults::seconds_max());
  }

  auto
  shouldFastClone(bool const fastCloningSet,
                  bool const fastCloning,
                  bool const wantAllEvents,
                  art::ClosingCriteria const& cc)
  {
    art::FastCloningEnabled enabled;
    if (fastCloningSet and not fastCloning) {
      enabled.disable(
        "RootOutput configuration explicitly disables fast cloning.");
      return enabled;
    }

    if (not wantAllEvents) {
      enabled.disable(
        "Event-selection has been specified in the RootOutput configuration.");
    }
    if (fastCloning && maxCriterionSpecified(cc) &&
        cc.granularity() < art::Granularity::InputFile) {
      enabled.disable("File-switching has been requested at event, subrun, or "
                      "run boundaries.");
    }
    return enabled;
  }
}

namespace art {

  class RootOutput final : public OutputModule {
  public:
    static constexpr char const* default_tmpDir{"<parent-path-of-filename>"};

    struct Config {
      using Name = fhicl::Name;
      using Comment = fhicl::Comment;
      template <typename T>
      using Atom = fhicl::Atom<T>;
      template <typename T>
      using OptionalAtom = fhicl::OptionalAtom<T>;
      fhicl::TableFragment<OutputModule::Config> omConfig;
      Atom<string> catalog{Name("catalog"), ""};
      OptionalAtom<bool> dropAllEvents{Name("dropAllEvents")};
      Atom<bool> dropAllSubRuns{Name("dropAllSubRuns"), false};
      OptionalAtom<bool> fastCloning{Name("fastCloning")};
      Atom<string> tmpDir{Name("tmpDir"), default_tmpDir};
      Atom<int> compressionLevel{Name("compressionLevel"), 7};
      Atom<int64_t> saveMemoryObjectThreshold{Name("saveMemoryObjectThreshold"),
                                              -1l};
      Atom<int64_t> treeMaxVirtualSize{Name("treeMaxVirtualSize"), -1};
      Atom<int> splitLevel{Name("splitLevel"), 1};
      Atom<int> basketSize{Name("basketSize"), 16384};
      Atom<bool> dropMetaDataForDroppedData{Name("dropMetaDataForDroppedData"),
                                            false};
      Atom<string> dropMetaData{Name("dropMetaData"), "NONE"};
      Atom<bool> writeParameterSets{Name("writeParameterSets"), true};
      fhicl::Table<ClosingCriteria::Config> fileProperties{
        Name("fileProperties"),
        Comment("The 'fileProperties' parameter is specified to enable "
                "output-file switching.")};
      fhicl::TableFragment<detail::SafeFileNameConfig> safeFileName;

      Config()
      {
        // Both RootOutput module and OutputModule use the "fileName"
        // FHiCL parameter.  However, whereas in OutputModule the
        // parameter has a default, for RootOutput the parameter should
        // not.  We therefore have to change the default flag setting
        // for 'OutputModule::Config::fileName'.
        using namespace fhicl::detail;
        ParameterBase* adjustFilename{
          const_cast<fhicl::Atom<string>*>(&omConfig().fileName)};
        adjustFilename->set_par_style(fhicl::par_style::REQUIRED);
      }

      struct KeysToIgnore {
        set<string>
        operator()() const
        {
          set<string> keys{OutputModule::Config::KeysToIgnore::get()};
          keys.insert("results");
          return keys;
        }
      };
    };

    using Parameters = fhicl::WrappedTable<Config, Config::KeysToIgnore>;

    ~RootOutput();
    explicit RootOutput(Parameters const&);
    RootOutput(RootOutput const&) = delete;
    RootOutput(RootOutput&&) = delete;
    RootOutput& operator=(RootOutput const&) = delete;
    RootOutput& operator=(RootOutput&&) = delete;

    void postSelectProducts() override;
    void beginJob() override;
    void endJob() override;
    void beginRun(RunPrincipal const&) override;
    void endRun(RunPrincipal const&) override;
    void beginSubRun(SubRunPrincipal const&) override;
    void endSubRun(SubRunPrincipal const&) override;
    void event(EventPrincipal const&) override;

  private:
    // Replace OutputModule Functions.
    string fileNameAtOpen() const;
    string fileNameAtClose(string const& currentFileName);
    string const& lastClosedFileName() const override;
    Granularity fileGranularity() const override;
    void openFile(FileBlock const&) override;
    void respondToOpenInputFile(FileBlock const&) override;
    void readResults(ResultsPrincipal const& resp) override;
    void respondToCloseInputFile(FileBlock const&) override;
    void incrementInputFileNumber() override;
    void write(EventPrincipal&) override;
    void writeSubRun(SubRunPrincipal&) override;
    void writeRun(RunPrincipal&) override;
    void setSubRunAuxiliaryRangeSetID(RangeSet const&) override;
    void setRunAuxiliaryRangeSetID(RangeSet const&) override;
    bool isFileOpen() const override;
    void setFileStatus(OutputFileStatus) override;
    bool requestsToCloseFile() const override;
    void startEndFile() override;
    void writeFileFormatVersion() override;
    void writeFileIndex() override;
    void writeProcessConfigurationRegistry() override;
    void writeProcessHistoryRegistry() override;
    void writeParameterSetRegistry() override;
    void writeProductDescriptionRegistry() override;
    void writeParentageRegistry() override;
    void doWriteFileCatalogMetadata(
      FileCatalogMetadata::collection_type const& md,
      FileCatalogMetadata::collection_type const& ssmd) override;
    void writeProductDependencies() override;
    void finishEndFile() override;
    void doRegisterProducts(ProductDescriptions& productsToProduce,
                            ModuleDescription const& md) override;

    // Implementation Details.
    void doOpenFile();

    // Data Members.
    mutable std::recursive_mutex mutex_;
    string const catalog_;
    bool dropAllEvents_{false};
    bool dropAllSubRuns_;
    string const moduleLabel_;
    int inputFileCount_{};
    unique_ptr<RootOutputFile> rootOutputFile_{nullptr};
    FileStatsCollector fstats_;
    PostCloseFileRenamer fRenamer_;
    string const filePattern_;
    string tmpDir_;
    string lastClosedFileName_{};
    int const compressionLevel_;
    int64_t const saveMemoryObjectThreshold_;
    int64_t const treeMaxVirtualSize_;
    int const splitLevel_;
    int const basketSize_;
    DropMetaData dropMetaData_;
    bool dropMetaDataForDroppedData_;
    FastCloningEnabled fastCloningEnabled_{};
    // Set false only for cases where we are guaranteed never to need historical
    // ParameterSet information in the downstream file, such as when mixing.
    bool writeParameterSets_;
    ClosingCriteria fileProperties_;
    ProductDescriptions productsToProduce_{};
    ProductTables producedResultsProducts_{ProductTables::invalid()};
    RPManager rpm_;
  };

  RootOutput::~RootOutput() = default;

  RootOutput::RootOutput(Parameters const& config)
    : OutputModule{config().omConfig}
    , catalog_{config().catalog()}
    , dropAllSubRuns_{config().dropAllSubRuns()}
    , moduleLabel_{config.get_PSet().get<string>("module_label")}
    , fstats_{moduleLabel_, processName()}
    , fRenamer_{fstats_}
    , filePattern_{config().omConfig().fileName()}
    , tmpDir_{config().tmpDir() == default_tmpDir ? parent_path(filePattern_) :
                                                    config().tmpDir()}
    , compressionLevel_{config().compressionLevel()}
    , saveMemoryObjectThreshold_{config().saveMemoryObjectThreshold()}
    , treeMaxVirtualSize_{config().treeMaxVirtualSize()}
    , splitLevel_{config().splitLevel()}
    , basketSize_{config().basketSize()}
    , dropMetaData_{config().dropMetaData()}
    , dropMetaDataForDroppedData_{config().dropMetaDataForDroppedData()}
    , writeParameterSets_{config().writeParameterSets()}
    , fileProperties_{config().fileProperties()}
    , rpm_{config.get_PSet()}
  {
    bool const check_filename = config.get_PSet().has_key("fileProperties") and
                                config().safeFileName().checkFileName();
    detail::validateFileNamePattern(check_filename, filePattern_);

    // Setup the streamers and error handlers.
    root::setup();

    bool const dropAllEventsSet{config().dropAllEvents(dropAllEvents_)};
    dropAllEvents_ = detail::shouldDropEvents(
      dropAllEventsSet, dropAllEvents_, dropAllSubRuns_);
    // N.B. Any time file switching is enabled at a boundary other than
    //      InputFile, fastCloningEnabled_ ***MUST*** be deactivated.  This is
    //      to ensure that the Event tree from the InputFile is not
    //      accidentally cloned to the output file before the output
    //      module has seen the events that are going to be processed.
    bool fastCloningEnabled{true};
    bool const fastCloningSet{config().fastCloning(fastCloningEnabled)};
    fastCloningEnabled_ = shouldFastClone(
      fastCloningSet, fastCloningEnabled, wantAllEvents(), fileProperties_);

    if (auto const n = Globals::instance()->nschedules(); n > 1) {
      std::ostringstream oss;
      oss << "More than one schedule (" << n << ") is being used.";
      fastCloningEnabled_.disable(oss.str());
    }

    if (!writeParameterSets_) {
      mf::LogWarning("PROVENANCE")
        << "Output module " << moduleLabel_
        << " has parameter writeParameterSets set to false.\n"
        << "Parameter set provenance will not be available in subsequent "
           "jobs.\n"
        << "Check your experiment's policy on this issue to avoid future "
           "problems\n"
        << "with analysis reproducibility.\n";
    }
  }

  void
  RootOutput::openFile(FileBlock const& fb)
  {
    std::lock_guard sentry{mutex_};
    // Note: The file block here refers to the currently open
    //       input file, so we can find out about the available
    //       products by looping over the branches of the input
    //       file data trees.
    if (!isFileOpen()) {
      doOpenFile();
      respondToOpenInputFile(fb);
    }
  }

  void
  RootOutput::postSelectProducts()
  {
    std::lock_guard sentry{mutex_};
    if (isFileOpen()) {
      rootOutputFile_->selectProducts();
    }
  }

  void
  RootOutput::respondToOpenInputFile(FileBlock const& fb)
  {
    std::lock_guard sentry{mutex_};
    ++inputFileCount_;
    if (!isFileOpen()) {
      return;
    }
    auto const* rfb = dynamic_cast<RootFileBlock const*>(&fb);
    auto fastCloneThisOne = fastCloningEnabled_;
    if (!rfb) {
      fastCloneThisOne.disable("Input source does not read art/ROOT files.");
    } else {
      fastCloneThisOne.merge(rfb->fastClonable());
    }
    rootOutputFile_->beginInputFile(rfb, std::move(fastCloneThisOne));
    fstats_.recordInputFile(fb.fileName());
  }

  void
  RootOutput::readResults(ResultsPrincipal const& resp)
  {
    std::lock_guard sentry{mutex_};
    rpm_.for_each_RPWorker(
      [&resp](RPWorker& w) { w.rp().doReadResults(resp); });
  }

  void
  RootOutput::respondToCloseInputFile(FileBlock const& fb)
  {
    std::lock_guard sentry{mutex_};
    if (isFileOpen()) {
      rootOutputFile_->respondToCloseInputFile(fb);
    }
  }

  void
  RootOutput::write(EventPrincipal& ep)
  {
    std::lock_guard sentry{mutex_};
    if (dropAllEvents_) {
      return;
    }
    if (hasNewlyDroppedBranch()[InEvent]) {
      ep.addToProcessHistory();
      ep.refreshProcessHistoryID();
    }
    rootOutputFile_->writeOne(ep);
    fstats_.recordEvent(ep.eventID());
  }

  void
  RootOutput::setSubRunAuxiliaryRangeSetID(RangeSet const& rs)
  {
    std::lock_guard sentry{mutex_};
    rootOutputFile_->setSubRunAuxiliaryRangeSetID(rs);
  }

  void
  RootOutput::writeSubRun(SubRunPrincipal& sr)
  {
    std::lock_guard sentry{mutex_};
    if (dropAllSubRuns_) {
      return;
    }
    if (hasNewlyDroppedBranch()[InSubRun]) {
      sr.addToProcessHistory();
    }
    rootOutputFile_->writeSubRun(sr);
    fstats_.recordSubRun(sr.subRunID());
  }

  void
  RootOutput::setRunAuxiliaryRangeSetID(RangeSet const& rs)
  {
    std::lock_guard sentry{mutex_};
    rootOutputFile_->setRunAuxiliaryRangeSetID(rs);
  }

  void
  RootOutput::writeRun(RunPrincipal& rp)
  {
    std::lock_guard sentry{mutex_};
    if (hasNewlyDroppedBranch()[InRun]) {
      rp.addToProcessHistory();
    }
    rootOutputFile_->writeRun(rp);
    fstats_.recordRun(rp.runID());
  }

  void
  RootOutput::startEndFile()
  {
    std::lock_guard sentry{mutex_};
    auto resp = make_unique<ResultsPrincipal>(
      ResultsAuxiliary{}, moduleDescription().processConfiguration(), nullptr);
    resp->createGroupsForProducedProducts(producedResultsProducts_);
    resp->enableLookupOfProducedProducts();
    if (!producedResultsProducts_.descriptions(InResults).empty() ||
        hasNewlyDroppedBranch()[InResults]) {
      resp->addToProcessHistory();
    }
    rpm_.for_each_RPWorker(
      [&resp](RPWorker& w) { w.rp().doWriteResults(*resp); });
    rootOutputFile_->writeResults(*resp);
  }

  void
  RootOutput::writeFileFormatVersion()
  {
    std::lock_guard sentry{mutex_};
    rootOutputFile_->writeFileFormatVersion();
  }

  void
  RootOutput::writeFileIndex()
  {
    std::lock_guard sentry{mutex_};
    rootOutputFile_->writeFileIndex();
  }

  void
  RootOutput::writeProcessConfigurationRegistry()
  {
    std::lock_guard sentry{mutex_};
    rootOutputFile_->writeProcessConfigurationRegistry();
  }

  void
  RootOutput::writeProcessHistoryRegistry()
  {
    std::lock_guard sentry{mutex_};
    rootOutputFile_->writeProcessHistoryRegistry();
  }

  void
  RootOutput::writeParameterSetRegistry()
  {
    std::lock_guard sentry{mutex_};
    if (writeParameterSets_) {
      rootOutputFile_->writeParameterSetRegistry();
    }
  }

  void
  RootOutput::writeProductDescriptionRegistry()
  {
    std::lock_guard sentry{mutex_};
    rootOutputFile_->writeProductDescriptionRegistry();
  }

  void
  RootOutput::writeParentageRegistry()
  {
    std::lock_guard sentry{mutex_};
    rootOutputFile_->writeParentageRegistry();
  }

  void
  RootOutput::doWriteFileCatalogMetadata(
    FileCatalogMetadata::collection_type const& md,
    FileCatalogMetadata::collection_type const& ssmd)
  {
    std::lock_guard sentry{mutex_};
    rootOutputFile_->writeFileCatalogMetadata(fstats_, md, ssmd);
  }

  void
  RootOutput::writeProductDependencies()
  {
    std::lock_guard sentry{mutex_};
    rootOutputFile_->writeProductDependencies();
  }

  void
  RootOutput::finishEndFile()
  {
    std::lock_guard sentry{mutex_};
    string const currentFileName{rootOutputFile_->currentFileName()};
    rootOutputFile_->writeTTrees();
    rootOutputFile_.reset();
    fstats_.recordFileClose();
    lastClosedFileName_ = fileNameAtClose(currentFileName);
    detail::logFileAction("Closed output file ", lastClosedFileName_);
    rpm_.invoke(&ResultsProducer::doClear);
  }

  void
  RootOutput::doRegisterProducts(ProductDescriptions& producedProducts,
                                 ModuleDescription const& md)
  {
    std::lock_guard sentry{mutex_};
    // Register Results products from ResultsProducers.
    rpm_.for_each_RPWorker([&producedProducts, &md](RPWorker& w) {
      auto const& params = w.params();
      w.setModuleDescription(
        ModuleDescription{params.rpPSetID,
                          params.rpPluginType,
                          md.moduleLabel() + '#' + params.rpLabel,
                          ModuleThreadingType::legacy,
                          md.processConfiguration()});
      w.rp().registerProducts(producedProducts, w.moduleDescription());
    });
    // Form product table for Results products.  We do this here so we
    // can appropriately set the product tables for the
    // ResultsPrincipal.
    productsToProduce_ = producedProducts;
    producedResultsProducts_ = ProductTables{productsToProduce_};
  }

  void
  RootOutput::setFileStatus(OutputFileStatus const ofs)
  {
    std::lock_guard sentry{mutex_};
    if (isFileOpen()) {
      rootOutputFile_->setFileStatus(ofs);
    }
  }

  bool
  RootOutput::isFileOpen() const
  {
    std::lock_guard sentry{mutex_};
    return rootOutputFile_.get() != nullptr;
  }

  void
  RootOutput::incrementInputFileNumber()
  {
    std::lock_guard sentry{mutex_};
    if (isFileOpen()) {
      rootOutputFile_->incrementInputFileNumber();
    }
  }

  bool
  RootOutput::requestsToCloseFile() const
  {
    std::lock_guard sentry{mutex_};
    return isFileOpen() ? rootOutputFile_->requestsToCloseFile() : false;
  }

  Granularity
  RootOutput::fileGranularity() const
  {
    std::lock_guard sentry{mutex_};
    return fileProperties_.granularity();
  }

  void
  RootOutput::doOpenFile()
  {
    std::lock_guard sentry{mutex_};
    if (inputFileCount_ == 0) {
      throw Exception(errors::LogicError)
        << "Attempt to open output file before input file. "
        << "Please report this to the core framework developers.\n";
    }
    rootOutputFile_ = make_unique<RootOutputFile>(this,
                                                  fileNameAtOpen(),
                                                  fileProperties_,
                                                  compressionLevel_,
                                                  saveMemoryObjectThreshold_,
                                                  treeMaxVirtualSize_,
                                                  splitLevel_,
                                                  basketSize_,
                                                  dropMetaData_,
                                                  dropMetaDataForDroppedData_);
    fstats_.recordFileOpen();
    detail::logFileAction("Opened output file with pattern ", filePattern_);
  }

  string
  RootOutput::fileNameAtOpen() const
  {
    return (filePattern_ == dev_null) ?
             dev_null :
             unique_filename(tmpDir_ + "/RootOutput");
  }

  string
  RootOutput::fileNameAtClose(std::string const& currentFileName)
  {
    return (filePattern_ == dev_null) ?
             dev_null :
             fRenamer_.maybeRenameFile(currentFileName, filePattern_);
  }

  string const&
  RootOutput::lastClosedFileName() const
  {
    std::lock_guard sentry{mutex_};
    if (lastClosedFileName_.empty()) {
      throw Exception(errors::LogicError, "RootOutput::currentFileName(): ")
        << "called before meaningful.\n";
    }
    return lastClosedFileName_;
  }

  void
  RootOutput::beginJob()
  {
    std::lock_guard sentry{mutex_};
    rpm_.invoke(&ResultsProducer::doBeginJob);
  }

  void
  RootOutput::endJob()
  {
    std::lock_guard sentry{mutex_};
    rpm_.invoke(&ResultsProducer::doEndJob);
  }

  void
  RootOutput::event(EventPrincipal const& ep)
  {
    std::lock_guard sentry{mutex_};
    rpm_.for_each_RPWorker([&ep](RPWorker& w) { w.rp().doEvent(ep); });
  }

  void
  RootOutput::beginSubRun(SubRunPrincipal const& srp)
  {
    std::lock_guard sentry{mutex_};
    rpm_.for_each_RPWorker([&srp](RPWorker& w) { w.rp().doBeginSubRun(srp); });
  }

  void
  RootOutput::endSubRun(SubRunPrincipal const& srp)
  {
    std::lock_guard sentry{mutex_};
    rpm_.for_each_RPWorker([&srp](RPWorker& w) { w.rp().doEndSubRun(srp); });
  }

  void
  RootOutput::beginRun(RunPrincipal const& rp)
  {
    std::lock_guard sentry{mutex_};
    rpm_.for_each_RPWorker([&rp](RPWorker& w) { w.rp().doBeginRun(rp); });
  }

  void
  RootOutput::endRun(RunPrincipal const& rp)
  {
    std::lock_guard sentry{mutex_};
    rpm_.for_each_RPWorker([&rp](RPWorker& w) { w.rp().doEndRun(rp); });
  }

} // namespace art

DEFINE_ART_MODULE(art::RootOutput)
