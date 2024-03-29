// ====================================================================
// SamplingInput
//
// - Only one "input file", one Run and one SubRun are created for a
//   job that uses the SamplingInput source.
//
// - Run and SubRun products are available only through the
//   SampledProduct<T> wrapper.  This product wrapper is a container
//   that retains the (Sub)Run products from each dataset.
//
// Technical notes:
//
// - Whereas event principals are created by the individual
//   SamplingInputFile objects, the (sub)run principals are created by
//   the input source itself.  The goal is to encapsulate as much as
//   possible the product internals to the source, and not the
//   underlying files.  This is possible for (sub)run products as
//   there is only one primary (sub)run per job.  For event products,
//   it is more natural to go to the individual input files
//   themselves.  This asymmetry is undesirable, but it might be
//   required due to the conceptual difference between events and
//   (sub)runs.
// ===================================================================

#include "art/Framework/Core/FileBlock.h"
#include "art/Framework/Core/InputSource.h"
#include "art/Framework/Core/InputSourceDescription.h"
#include "art/Framework/Core/InputSourceMacros.h"
#include "art/Framework/Core/UpdateOutputCallbacks.h"
#include "art/Framework/Core/detail/issue_reports.h"
#include "art/Framework/Principal/EventPrincipal.h"
#include "art/Framework/Principal/OpenRangeSetHandler.h"
#include "art/Framework/Principal/RangeSetsSupported.h"
#include "art/Framework/Principal/RunPrincipal.h"
#include "art/Framework/Principal/SubRunPrincipal.h"
#include "art/Persistency/Provenance/ModuleDescription.h"
#include "art/Persistency/Provenance/ProcessHistoryRegistry.h"
#include "art_root_io/detail/DataSetBroker.h"
#include "art_root_io/setup.h"
#include "canvas/Persistency/Common/Wrapper.h"
#include "canvas/Persistency/Provenance/EventID.h"
#include "canvas/Persistency/Provenance/FileFormatVersion.h"
#include "canvas/Persistency/Provenance/ProcessConfiguration.h"
#include "canvas/Persistency/Provenance/ProductTables.h"
#include "canvas/Persistency/Provenance/RangeSet.h"
#include "canvas/Persistency/Provenance/RunID.h"
#include "canvas/Persistency/Provenance/SubRunID.h"
#include "canvas/Persistency/Provenance/TypeLabel.h"
#include "canvas/Utilities/TypeID.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "range/v3/view.hpp"

#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/DelegatedParameter.h"
#include "fhiclcpp/types/OptionalAtom.h"
#include "fhiclcpp/types/Sequence.h"

#include <memory>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

using namespace art;
using namespace fhicl;
using namespace ranges;
using namespace std::string_literals;

using art::detail::Products_t;

namespace {

  std::unique_ptr<EDProduct>
  make_sampled_product(Products_t& read_products, BranchKey const& original_key)
  {
    auto& datasets_with_product = read_products.at(original_key);

    auto const first_entry = begin(datasets_with_product);
    auto const& products = first_entry->second;
    assert(!products.empty());

    InputTag const tag{original_key.moduleLabel_,
                       original_key.productInstanceName_,
                       original_key.processName_};

    auto sampled_product =
      cbegin(products)->second->createEmptySampledProduct(tag);
    for (auto&& [dataset, products_per_id] : datasets_with_product) {
      for (auto&& [id, products] : products_per_id) {
        sampled_product->insertIfSampledProduct(
          dataset, id, std::move(products));
      }
    }
    return sampled_product;
  }

  ProcessConfigurations
  sampled_process_configurations(
    std::map<BranchKey, BranchDescription> const& descriptions,
    ProcessConfiguration const& pc)
  {
    auto processNames =
      descriptions | views::values |
      views::transform([](auto const& pd) { return pd.processName(); }) |
      to<std::set>();

    auto result = processNames | views::transform([&pc](auto const& name) {
                    return ProcessConfiguration{
                      name, pc.parameterSetID(), pc.releaseVersion()};
                  }) |
                  to<ProcessConfigurations>();
    // Current process goes last
    result.push_back(pc);
    return result;
  }

  ProcessHistoryID
  sampled_process_history_id(ProcessConfigurations const& pcs)
  {
    ProcessHistory const result{pcs};
    auto const id = result.id();
    ProcessHistoryRegistry::emplace(id, result);
    return id;
  }

}

namespace art {

  class SamplingInput : public InputSource {
  public:
    struct Config {
      Atom<std::string> module_type{Name{"module_type"}};
      Atom<unsigned> maxEvents{Name{"maxEvents"}, 1u};
      OptionalAtom<RunNumber_t> run{
        Name{"run"},
        Comment{
          "The specified run and subrun numbers are used for all primary "
          "events\n"
          "generated in a given art job.  If no values are specified, default\n"
          "values of 1 and 0 are chosen for the run and subrun, "
          "respectively."}};
      OptionalAtom<SubRunNumber_t> subRun{Name{"subRun"}};
      OptionalAtom<EventNumber_t> firstEvent{Name{"firstEvent"}};
      Atom<std::uint_fast32_t> samplingSeed{
        Name{"samplingSeed"},
        Comment{
          "The 'samplingSeed' parameter is an unsigned integral seed value\n"
          "provided to the constructor of the random-number engine used for\n"
          "sampling datasets.  The default value is the C++ standard\n"
          "library's chosen default."},
        std::minstd_rand0::default_seed};
      DelegatedParameter dataSets{
        Name{"dataSets"},
        Comment{
          "The value of the 'dataSets' parameter is a table of the form:\n\n"
          "  dataSets: {\n"
          "    <dataset name>: {\n"
          "      fileNames: [<string>]\n"
          "      weight: <double>\n"
          "      skipToEvent: \"\"  #default\n"
          "    }\n"
          "    ...\n"
          "  }\n\n"
          "where the '<dataset name>' parameter labels a particular\n"
          "dataset (e.g. 'signal'), the 'fileNames' refers to the files\n"
          "that contains events of the dataset, and the 'weight'\n"
          "is a floating-point number used to determine the frequency\n"
          "with which events from this dataset are sampled, relative to\n"
          "the sum of weights across all datasets.\n\n"
          "The 'skipToEvent' parameter specifies the first event that\n"
          "should be used when sampling from a specific input file.  The\n"
          "correct specification is via the triplet \"run:subrun:event\".\n"
          "For example, to begin sampling at run 3, subrun 2, event 39\n"
          "from this dataset, the user should specify:\n\n"
          "  skipToEvent: \"3:2:39\"\n\n"
          "Specifying the empty string (the default) is equivalent to\n"
          "beginning at the first event of the file.  All other\n"
          "specifications are ill-formed and will result in an exception\n"
          "throw at the beginning of the job.\n\n"
          "The ellipsis indicates that multiple datasets can be configured.\n\n"
          "N.B. Only one file per dataset is currently allowed."}};
      Atom<bool> summary{Name{"summary"}, true};
      Atom<bool> delayedReadEventProducts{Name{"delayedReadEventProducts"},
                                          true};
      Sequence<std::string> inputCommands{Name{"inputCommands"},
                                          std::vector<std::string>{"keep *"}};
      Atom<bool> dropDescendantsOfDroppedBranches{
        Name{"dropDescendantsOfDroppedBranches"},
        true};
      Atom<bool> readParameterSets{Name{"readParameterSets"}, true};
      Atom<unsigned> treeCacheSize{Name("treeCacheSize"), 0u};
      Atom<std::int64_t> treeMaxVirtualSize{Name("treeMaxVirtualSize"), -1};
      Atom<int64_t> saveMemoryObjectThreshold{Name{"saveMemoryObjectThreshold"},
                                              -1};
      Atom<bool> compactRanges{
        Name{"compactRanges"},
        Comment{
          "The 'compactRanges' parameter can be set to 'true'\n"
          "if the user can guarantee that a SubRun is not spread across\n"
          "multiple input files.  Setting it to 'true' can yield some\n"
          "memory savings when reading input files created before art 2.01,\n"
          "but it can also modify the provenance of the workflow.  Please\n"
          "consult your experiment's policy on provenance before setting this\n"
          "parameter to 'true'."},
        false};

      struct KeysToIgnore {
        std::set<std::string>
        operator()() const
        {
          return {"module_label"};
        }
      };
    };

    using Parameters = WrappedTable<Config, Config::KeysToIgnore>;
    explicit SamplingInput(Parameters const& config,
                           InputSourceDescription& isd);

  private:
    template <typename T>
    std::enable_if_t<detail::range_sets_supported(T::branch_type)>
    putSampledProductsInto_(T& principal,
                            Products_t read_products,
                            RangeSet&& rs) const;

    input::ItemType nextItemType() override;
    std::unique_ptr<FileBlock> readFile() override;
    void closeFile() override;
    std::unique_ptr<RunPrincipal> readRun() override;
    std::unique_ptr<SubRunPrincipal> readSubRun(
      cet::exempt_ptr<RunPrincipal const> rp) override;
    std::unique_ptr<EventPrincipal> readEvent(
      cet::exempt_ptr<SubRunPrincipal const> srp) override;
    std::unique_ptr<RangeSetHandler> runRangeSetHandler() override;
    std::unique_ptr<RangeSetHandler> subRunRangeSetHandler() override;

    ModuleDescription const md_;
    ProcessConfiguration const& pc_;
    RunID runID_;
    SubRunID subRunID_;
    EventID nextEventID_;
    input::ItemType currentItemType_{input::IsInvalid};
    unsigned eventsLeft_;
    unsigned totalCounts_{};
    detail::DataSetBroker dataSetBroker_;
    bool const summary_;
    bool const delayedReadEventProducts_;
    std::map<BranchKey, BranchDescription> oldKeyToSampledProductDescription_;
    BranchDescription sampledRunInfoDesc_;
    BranchDescription sampledSubRunInfoDesc_;
    BranchDescription sampledEventInfoDesc_;
    ProcessConfigurations sampledProcessConfigs_{};
    ProcessHistoryID sampledProcessHistoryID_{};
    ProductTable presentSubRunProducts_;
    ProductTable presentRunProducts_;
  };
}

namespace {
  constexpr auto
  nullTimestamp()
  {
    return art::Timestamp{};
  }

  template <typename T>
  auto
  type_label_for(std::string const& emulated_module_name)
  {
    return art::TypeLabel{
      art::TypeID{typeid(T)}, {}, false, emulated_module_name};
  }
}

art::SamplingInput::SamplingInput(Parameters const& config,
                                  art::InputSourceDescription& isd)
  : InputSource{isd.moduleDescription}
  , md_{isd.moduleDescription}
  , pc_{md_.processConfiguration()}
  , eventsLeft_{config().maxEvents()}
  // Setup custom ROOT configurations.  Note the comma operator.
  , dataSetBroker_{(root::setup(), config().dataSets.get<ParameterSet>()),
                   config().samplingSeed()}
  , summary_{config().summary()}
  , delayedReadEventProducts_{config().delayedReadEventProducts()}
{
  auto const readParameterSets = config().readParameterSets();
  if (!readParameterSets) {
    mf::LogWarning("PROVENANCE")
      << "Source parameter readParameterSets was set to false: parameter set "
         "provenance\n"
      << "will NOT be available in this or subsequent jobs using output from "
         "this job.\n"
      << "Check your experiment's policy on this issue to avoid future "
         "problems\n"
      << "with analysis reproducibility.";
  }

  RunNumber_t r{};
  bool const haveFirstRun = config().run(r);
  runID_ = haveFirstRun ? RunID{r} : RunID::firstRun();

  SubRunNumber_t sr{};
  bool const haveFirstSubRun = config().subRun(sr);
  subRunID_ =
    haveFirstSubRun ? SubRunID{runID_, sr} : SubRunID::firstSubRun(runID_);

  EventNumber_t event{};
  bool const haveFirstEvent = config().firstEvent(event);
  nextEventID_ =
    haveFirstEvent ? EventID{subRunID_, event} : EventID::firstEvent(subRunID_);

  // Register SampledRunInfo, SampledSubRunInfo, and SampledEventInfo data
  ProductDescriptions presentSampledProducts;
  auto const emulated_module_name = "SamplingInput"s;
  presentSampledProducts.emplace_back(
    InEvent,
    type_label_for<SampledEventInfo>(emulated_module_name),
    emulated_module_name,
    isd.moduleDescription.parameterSetID(),
    isd.moduleDescription.processConfiguration());
  sampledEventInfoDesc_ = presentSampledProducts.back();
  presentSampledProducts.emplace_back(
    InSubRun,
    type_label_for<SampledSubRunInfo>(emulated_module_name),
    emulated_module_name,
    isd.moduleDescription.parameterSetID(),
    isd.moduleDescription.processConfiguration());
  sampledSubRunInfoDesc_ = presentSampledProducts.back();
  presentSampledProducts.emplace_back(
    InRun,
    type_label_for<SampledRunInfo>(emulated_module_name),
    emulated_module_name,
    isd.moduleDescription.parameterSetID(),
    isd.moduleDescription.processConfiguration());
  sampledRunInfoDesc_ = presentSampledProducts.back();

  auto& outputCallbacks = isd.productRegistry;
  oldKeyToSampledProductDescription_ =
    dataSetBroker_.openInputFiles(config().inputCommands(),
                                  config().dropDescendantsOfDroppedBranches(),
                                  config().treeCacheSize(),
                                  config().treeMaxVirtualSize(),
                                  config().saveMemoryObjectThreshold(),
                                  sampledEventInfoDesc_,
                                  config().compactRanges(),
                                  md_,
                                  readParameterSets,
                                  outputCallbacks);

  sampledProcessConfigs_ =
    sampled_process_configurations(oldKeyToSampledProductDescription_, pc_);
  sampledProcessHistoryID_ = sampled_process_history_id(sampledProcessConfigs_);

  for (auto const& pd : oldKeyToSampledProductDescription_ | views::values) {
    presentSampledProducts.push_back(pd);
  }

  // Specify present products for SubRuns and Runs.  Only the
  // Sampled(Sub)RunInfo products are present for the (Sub)Runs.
  ProductTables const tables{presentSampledProducts};
  outputCallbacks.invoke(tables);
  presentSubRunProducts_ = tables.get(InSubRun);
  presentRunProducts_ = tables.get(InRun);
}

std::unique_ptr<art::FileBlock>
art::SamplingInput::readFile()
{
  return std::make_unique<art::FileBlock>(
    art::FileFormatVersion{1, "SamplingInput_2018"}, "SamplingInput");
}

void
art::SamplingInput::closeFile()
{
  if (!summary_)
    return;

  dataSetBroker_.countSummary();
}

art::input::ItemType
art::SamplingInput::nextItemType()
{
  switch (currentItemType_) {
  case input::IsInvalid: {
    return currentItemType_ = input::IsFile;
  }
  case input::IsFile: {
    return currentItemType_ = input::IsRun;
  }
  case input::IsRun: {
    return currentItemType_ = input::IsSubRun;
  }
  case input::IsSubRun: {
    // Do not return prematurely when moving to the event.
    currentItemType_ = input::IsEvent;
  }
  default: {
  } // Handle other transitions below.
  }

  if (eventsLeft_ == 0u) {
    return input::IsStop;
  }

  bool const inputExhausted = !dataSetBroker_.canReadEvent();

  if (inputExhausted) {
    return input::IsStop;
  }

  --eventsLeft_;
  ++totalCounts_;
  return input::IsEvent;
}

template <typename T>
std::enable_if_t<art::detail::range_sets_supported(T::branch_type)>
art::SamplingInput::putSampledProductsInto_(T& principal,
                                            Products_t read_products,
                                            RangeSet&& rs) const
{
  for (auto const& [old_key, sampled_pd] : oldKeyToSampledProductDescription_) {
    if (old_key.branchType_ != principal.branchType())
      continue;

    principal.put(sampled_pd,
                  std::make_unique<ProductProvenance const>(
                    sampled_pd.productID(), productstatus::present()),
                  make_sampled_product(read_products, old_key),
                  std::make_unique<RangeSet>(std::move(rs)));
  }
}

// N.B. For all principals below, we mark the process history as
// "modified" because this source creates additional products and
// explicitly specifies the process history ID.  (We do something
// similar for the art::SourceHelper class.)

std::unique_ptr<art::RunPrincipal>
art::SamplingInput::readRun()
{
  Products_t read_products;
  auto sampledRunInfo = dataSetBroker_.readAllRunProducts(read_products);

  art::RunAuxiliary aux{runID_, nullTimestamp(), nullTimestamp()};
  aux.setProcessHistoryID(sampledProcessHistoryID_);

  auto rp = std::make_unique<art::RunPrincipal>(
    aux, pc_, cet::make_exempt_ptr(&presentRunProducts_));
  rp->markProcessHistoryAsModified();

  putSampledProductsInto_(
    *rp, std::move(read_products), RangeSet::forRun(runID_));

  // Place sampled run info onto the run
  auto wp =
    std::make_unique<Wrapper<SampledRunInfo>>(std::move(sampledRunInfo));
  rp->put(sampledRunInfoDesc_,
          std::make_unique<ProductProvenance const>(
            sampledRunInfoDesc_.productID(), productstatus::present()),
          std::move(wp),
          std::make_unique<RangeSet>(RangeSet::forRun(runID_)));
  return rp;
}

std::unique_ptr<art::SubRunPrincipal>
art::SamplingInput::readSubRun(cet::exempt_ptr<art::RunPrincipal const> rp)
{
  Products_t read_products;
  auto sampledSubRunInfo = dataSetBroker_.readAllSubRunProducts(read_products);

  art::SubRunAuxiliary aux{subRunID_, nullTimestamp(), nullTimestamp()};
  aux.setProcessHistoryID(sampledProcessHistoryID_);

  auto srp = std::make_unique<SubRunPrincipal>(
    aux, pc_, cet::make_exempt_ptr(&presentSubRunProducts_));
  srp->setRunPrincipal(rp);
  srp->markProcessHistoryAsModified();

  putSampledProductsInto_(
    *srp, std::move(read_products), RangeSet::forSubRun(subRunID_));

  // Place sampled run info onto the run
  auto wp =
    std::make_unique<Wrapper<SampledSubRunInfo>>(std::move(sampledSubRunInfo));
  srp->put(sampledSubRunInfoDesc_,
           std::make_unique<ProductProvenance const>(
             sampledSubRunInfoDesc_.productID(), productstatus::present()),
           std::move(wp),
           std::make_unique<RangeSet>(RangeSet::forSubRun(subRunID_)));
  return srp;
}

std::unique_ptr<art::EventPrincipal>
art::SamplingInput::readEvent(cet::exempt_ptr<art::SubRunPrincipal const> srp)
{
  detail::issue_reports(totalCounts_, nextEventID_);

  auto ep =
    dataSetBroker_.readNextEvent(nextEventID_, sampledProcessConfigs_, pc_);
  ep->setSubRunPrincipal(srp);
  ep->markProcessHistoryAsModified();
  if (!delayedReadEventProducts_) {
    ep->readImmediate();
  }
  nextEventID_ = nextEventID_.next();
  return ep;
}

std::unique_ptr<art::RangeSetHandler>
art::SamplingInput::runRangeSetHandler()
{
  return std::make_unique<art::OpenRangeSetHandler>(runID_.run());
}

std::unique_ptr<art::RangeSetHandler>
art::SamplingInput::subRunRangeSetHandler()
{
  return std::make_unique<art::OpenRangeSetHandler>(subRunID_.run());
}

DEFINE_ART_INPUT_SOURCE(art::SamplingInput)
