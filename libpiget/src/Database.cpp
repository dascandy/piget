#include "piget/GitCAM.hpp"
#include "piget/Object.hpp"

Database::Database(std::filesystem::path root) 
: cam(root)
{
  // TODO: read all pack indexes
}

std::optional<Object> Database::get(std::array<uint8_t, 20> id) const {
  // logic: often-used objects should be in separate files, so we look there first
  auto rv = cam.get(id);
  if (rv) return rv;
  for (auto& p : packs) {
    auto obj = p->get(id);
    if (obj) return obj;
  }
  return std::nullopt;
}

void Database::add(const Object& object) {
  cam.add(object);
}

void Database::addPack(Pack p) {
  packs.push_back(new Pack(std::move(p)));
}


