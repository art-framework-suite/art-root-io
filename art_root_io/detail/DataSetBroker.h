#ifndef art_root_io_detail_DataSetBroker_h
#define art_root_io_detail_DataSetBroker_h

#include "art_root_io/detail/DataSetSampler.h"
#include "art_root_io/detail/SamplingInputFile.h"
#include "canvas/Persistency/Provenance/BranchKey.h"
#include "canvas/Persistency/Provenance/EventID.h"
#include "canvas/Persistency/Provenance/ProcessConfiguration.h"
#include "canvas/Persistency/Provenance/SampledInfo.h"
#include "cetlib/exempt_ptr.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace art {
  namespace detail {

    using ProductsForDataset_t =
      std::map<std::string, SamplingInputFile::InstanceForID_t>;
    using Products_t = std::map<BranchKey, ProductsForDataset_t>;

    class DataSetBroker {
    public:
      explicit DataSetBroker(fhicl::ParameterSet const& pset,
                             std::uint_fast32_t seed);

      std::map<BranchKey, BranchDescription> openInputFiles(
        std::vector<std::string> const& inputCommands,
        bool dropDescendants,
        unsigned int treeCacheSize,
        int64_t treeMaxVirtualSize,
        int64_t saveMemoryObjectThreshold,
        BranchDescription const& sampledEventInfoDesc,
        bool compactRangeSetsForReading,
        ModuleDescription const& md,
        bool readParameterSets,
        UpdateOutputCallbacks& outputCallbacks);

      bool canReadEvent();

      std::unique_ptr<SampledRunInfo> readAllRunProducts(
        Products_t& read_products);

      std::unique_ptr<SampledSubRunInfo> readAllSubRunProducts(
        Products_t& read_products);

      std::unique_ptr<EventPrincipal> readNextEvent(
        EventID const& id,
        ProcessConfigurations const& sampled_pcs,
        ProcessConfiguration const& current_pc);

      void countSummary() const;

    private:
      struct Config {
        std::string fileName;
        EventID firstEvent;
      };
      std::map<std::string, Config> configs_{};
      std::map<std::string, art::detail::SamplingInputFile> files_;
      std::unique_ptr<DataSetSampler> dataSetSampler_{nullptr};
      std::map<std::string, unsigned> counts_;
      unsigned totalCounts_{};
      cet::exempt_ptr<std::string const> currentDataset_{nullptr};
    };
  }
}

#endif /* art_root_io_detail_DataSetBroker_h */

// Local Variables:
// mode: c++
// End:
