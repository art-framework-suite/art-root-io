#include "art_root_io/GetFileFormatVersion.h"

////////////////////////////////////////////////////////////////////////
// Version history
////////////////////////////////////
//
// In general:
//
// * A change to the persistent format which is both backward- and
// forward-compatible requires no change to the version or era.
//
// * A backward-compatible change should change the version.
//
// * A non-backward-compatible change should change the era,
//
// Era / Version   Comments
//
// ART_2011a   1   Initial art-only format
// ART_2011a   2   No FileID in ROOT file.
// ART_2011a   3   Bunch crossing, orbit number and store number removed
//                 from EventAuxiliary.
// ART_2011a   4   PtrVector member indicies_-> indices_.
// ART_2011a   5   ProductRegistry changes.
//                 BranchDescription improvements.
//                 ParameterSets written to MetaDataDB.
// ART_2011a   6   Improvements to ParameterSet information in MetaDataDB.
// ART_2011a   7   FileIndex moved to separate tree.
// ART_2011a   8   EventID unnecessary data member removed.
// ART_2011a   9   RangeSet information stored in RootFileDB.
//                 Stored ParameterSetID for SelectEvents now
//                 corresponds to containing ParameterSet of module
//                 (i.e. the nested SelectEvents.SelectEvents is
//                 deprecated).
// ART_2011a  10   Remove persistence of BranchIDListRegistry.
//                 ProductID schema change to storing checksum.
// ART_2011a  11   BranchDescription gains supportView_ data member.
// ART_2011a  12   RangeSets corresponding to a full run no longer
//                 have a representation of an empty list of EventRanges.
//                 They now have one element with an EventRange that is
//                 used to represent a full run.
// ART_2011a  13   The BranchListIndexes data member has been removed
//                 from the History class.
// ART_2011a  14   Underscore have been removed from the data members of
//                 FileIndex::Element
// ART_2011a  15   Remove art::History and add art::ProcessHistoryID to
//                 art::EventAuxiliary
//
////////////////////////////////////////////////////////////////////////

namespace art {
  int
  getFileFormatVersion()
  {
    return 15;
  }
} // namespace art
