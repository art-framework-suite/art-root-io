#ifndef art_root_io_RootInput_h
#define art_root_io_RootInput_h
// vim: set sw=2 expandtab :

#include "art/Framework/Core/InputSource.h"
#include "art/Framework/Core/ProcessingLimits.h"
#include "art/Framework/Core/fwd.h"
#include "art/Framework/IO/Catalog/InputFileCatalog.h"
#include "art/Framework/Principal/fwd.h"
#include "art_root_io/Inputfwd.h"
#include "art_root_io/RootInputFileSequence.h"
#include "fhiclcpp/types/ConfigurationTable.h"
#include "fhiclcpp/types/Table.h"
#include "fhiclcpp/types/TableFragment.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <memory>
#include <string>

namespace art {

  class RootInput final : public InputSource {
  public:
    struct Config {

      fhicl::Atom<std::string> module_type{fhicl::Name("module_type")};
      fhicl::TableFragment<ProcessingLimits::Config> limits_config;
      fhicl::TableFragment<InputFileCatalog::Config> ifc_config;
      fhicl::TableFragment<RootInputFileSequence::Config> rifs_config;

      struct KeysToIgnore {
        std::set<std::string>
        operator()() const
        {
          return {"module_label"};
        }
      };
    };

    using Parameters = fhicl::WrappedTable<Config, Config::KeysToIgnore>;

  public:
    RootInput(Parameters const&, InputSourceDescription&);
    ~RootInput();

    RootInput(RootInput const&) = delete;
    RootInput(RootInput&&) = delete;

    RootInput& operator=(RootInput const&) = delete;
    RootInput& operator=(RootInput&&) = delete;

    // Find the requested event and set the system up to read run and
    // subRun records where appropriate.
    bool seekToEvent(art::EventID const& id, bool exact = false);
    bool seekToEvent(int offset);

  private:
    class AccessState {
    public:
      enum State {
        SEQUENTIAL = 0,
        SEEKING_FILE,   // 1
        SEEKING_RUN,    // 2
        SEEKING_SUBRUN, // 3
        SEEKING_EVENT   // 4
      };

      ~AccessState();
      AccessState();

      State state() const;

      void setState(State state);
      void resetState();

      EventID const& lastReadEventID() const;
      void setLastReadEventID(EventID const&);

      EventID const& wantedEventID() const;
      void setWantedEventID(EventID const&);

      std::shared_ptr<RootInputFile> rootFileForLastReadEvent() const;
      void setRootFileForLastReadEvent(std::shared_ptr<RootInputFile> const&);

    private:
      State state_{SEQUENTIAL};
      EventID lastReadEventID_{};
      std::shared_ptr<RootInputFile> rootFileForLastReadEvent_{nullptr};
      EventID wantedEventID_{};
    };
    void updateAccessState(art::EventID const& id);

    using EntryNumber = input::EntryNumber;

    input::ItemType nextItemType() override;

    std::unique_ptr<RunPrincipal> readRun() override;
    std::unique_ptr<SubRunPrincipal> readSubRun(
      cet::exempt_ptr<RunPrincipal const>) override;
    std::unique_ptr<EventPrincipal> readEvent(
      cet::exempt_ptr<SubRunPrincipal const>) override;
    std::unique_ptr<FileBlock> readFile() override;
    void closeFile() override;

    std::unique_ptr<RangeSetHandler> runRangeSetHandler() override;
    std::unique_ptr<RangeSetHandler> subRunRangeSetHandler() override;

    void doEndJob() override;

    ProcessingLimits limits_;
    InputFileCatalog catalog_;
    RootInputFileSequence primaryFileSequence_;
    AccessState accessState_{};
  };

} // namespace art

// Local Variables:
// mode: c++
// End:

#endif /* art_root_io_RootInput_h */
