#ifndef art_root_io_detail_DataSetSampler_h
#define art_root_io_detail_DataSetSampler_h

#include <cassert>
#include <random>
#include <string>
#include <vector>

namespace art::detail {

  // We use a struct-of-arrays for format since it is better suited
  // for the facilities used here.
  class DataSetSampler {
  public:
    explicit DataSetSampler(std::vector<std::string> const& datasetNames,
                            std::vector<double> const& weights,
                            std::uint_fast32_t seed) noexcept(false);

    auto const&
    sample()
    {
      auto const index = dist_(engine_);
      assert(index < datasetNames_.size());
      return datasetNames_[index];
    }

    auto const&
    datasets() const noexcept
    {
      return datasetNames_;
    }

    auto
    weight(std::string const& dataset) const
    {
      return weights_[index_for(dataset)];
    }

    auto
    probability(std::string const& dataset) const
    {
      return dist_.probabilities()[index_for(dataset)];
    }

  private:
    std::size_t index_for(std::string const& dataset) const;

    std::vector<std::string> datasetNames_;
    std::vector<double> weights_;
    std::minstd_rand0 engine_;
    std::discrete_distribution<unsigned> dist_;
  };
}

// Local variables:
// mode: c++
// End:

#endif /* art_root_io_detail_DataSetSampler_h */
