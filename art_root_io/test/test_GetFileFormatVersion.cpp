#include "art_root_io/GetFileFormatVersion.h"
#include <cassert>

int
main()
{
  assert(art::getFileFormatVersion() == 14);
}
