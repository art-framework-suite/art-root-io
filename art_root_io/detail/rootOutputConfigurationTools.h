#ifndef art_root_io_detail_rootOutputConfigurationTools_h
#define art_root_io_detail_rootOutputConfigurationTools_h

namespace art::detail {
  bool shouldDropEvents(bool dropAllEventsSet,
                        bool dropAllEvents,
                        bool dropAllSubRuns);
}

#endif /* art_root_io_detail_rootOutputConfigurationTools_h */

// Local variables:
// mode: c++
// End:
