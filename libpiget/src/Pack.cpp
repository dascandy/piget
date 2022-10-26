#include "piget/Object.hpp"
#include "piget/GitCAM.hpp"
#include "bini/writer.h"
#include <sys/mman.h>
#include <caligo/sha1.h>
#include <fstream>

std::pair<std::vector<uint8_t>, std::vector<uint8_t>> WritePack(const Database& db, std::vector<std::array<uint8_t, 20>> objectIds) {
  Bini::writer w;
  w.add32be(0x4F41434B);
  w.add32be(2);
  w.add32be(objectIds.size());
  std::vector<Pack::IndexEntry> index;
  for (auto& id : objectIds) {
    auto objR = db.get(id);
    if (not objR) {
      throw std::runtime_error("Invalid object id");
    }
    Object& obj = *objR;
    index.push_back({id, w.size()});
    size_t s = obj.data().size();
    s = ((s & 0xFFFFFFFFFFFFF0) << 3) | (s & 0xF);
    if (obj.type() == "commit") {
      s |= 0x10;
    } else if (obj.type() == "tree") {
      s |= 0x20;
    } else if (obj.type() == "object") {
      s |= 0x30;
    }
    w.addPB(s);
    w.add(Decoco::compress(Decoco::ZlibCompressor(), obj.data()));
  }
  w.add(Caligo::SHA1{w}.data());
  // TODO: make index file
  std::vector<uint8_t> indexB;
  return { std::move(w), indexB };
}
/*
struct Pack {
  Pack(std::span<const uint8_t> data, std::span<const uint8_t> index);
  std::vector<uint8_t> get(std::array<uint8_t, 20> id);
private:
  struct IndexEntry {
    std::array<uint8_t, 20> id;
    size_t offset;
  };
  std::span<const uint8_t> data;
  std::vector<IndexEntry> index;
};

*/
