#include "art_root_io/detail/rootOutputConfigurationTools.h"
// vim: set sw=2 expandtab :

#include "canvas/Utilities/Exception.h"

namespace art::detail {

  bool
  shouldDropEvents(bool const dropAllEventsSet,
                   bool const dropAllEvents,
                   bool const dropAllSubRuns)
  {
    if (!dropAllSubRuns) {
      return dropAllEvents;
    }
    if (dropAllEventsSet && !dropAllEvents) {
      throw Exception(errors::Configuration)
        << "\nThe following FHiCL specification is illegal\n\n"
        << "   dropAllEvents  : false \n"
        << "   dropAllSubRuns : true  \n\n"
        << "[1] Both can be 'true', "
        << "[2] both can be 'false', or "
        << "[3] 'dropAllEvents : true' and 'dropAllSubRuns : false' "
        << "is allowed.\n\n";
    }
    return true;
  }

} // namespace art::detail
