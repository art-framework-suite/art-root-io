#include "art_root_io/DummyProductCache.h"
#include "TClass.h"
#include "canvas/Utilities/Exception.h"

art::EDProduct const*
art::DummyProductCache::product(std::string const& wrappedName)
{
  auto it = dummies_.find(wrappedName);
  if (it == dummies_.cend()) {
    TClass* cp = TClass::GetClass(wrappedName.c_str());
    if (cp == nullptr) {
      throw art::Exception(art::errors::DictionaryNotFound)
        << "TClass::GetClass() returned null pointer for name: " << wrappedName
        << '\n';
    }
    std::unique_ptr<EDProduct> dummy{reinterpret_cast<EDProduct*>(cp->New())};
    it = dummies_.emplace(wrappedName, std::move(dummy)).first;
  }
  return it->second.get();
}
