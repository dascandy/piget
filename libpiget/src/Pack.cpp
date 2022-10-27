#include "piget/Object.hpp"
#include "piget/GitCAM.hpp"
#include "bini/writer.h"
#include <sys/mman.h>
#include <caligo/sha1.h>
#include <caligo/crc.h>
#include <fstream>

std::vector<uint8_t> CreateIndexFile(std::vector<Pack::IndexEntry> index) {
  std::sort(index.begin(), index.end(), [](const Pack::IndexEntry& lhs, const Pack::IndexEntry& rhs) {
    for (size_t n = 0; n < 20; n++) {
      if (lhs.id[n] < rhs.id[n]) return true;
      if (lhs.id[n] > rhs.id[n]) return false;
    } 
    return false;
  });
  Bini::writer main, crcs, ids, offsets, largeoffsets, ii;
  uint16_t lastId = 0;
  size_t n;
  for (n = 0; n < index.size(); n++) {
    uint8_t start = index[n].id[0];
    while (lastId < start) {
      ii.add32be(n);
      lastId++;
    }
    crcs.add(index[n].crc);
    ids.add(index[n].id);
    if (index[n].offset < 0x8000'0000) {
      offsets.add32be(index[n].offset);
    } else {
      offsets.add32be(largeoffsets.size() | 0x8000'0000);
      largeoffsets.add64be(index[n].offset);
    }
  }
  while (lastId < 0x100) {
    ii.add32be(n);
    lastId++;
  }
  main.add32be(0xFF744F63);
  main.add32be(2);
  main.add(ii);
  main.add(ids);
  main.add(crcs);
  main.add(offsets);
  main.add(largeoffsets);
}

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
    index.push_back({id, Caligo::CRC32(obj.data()), w.size()});
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
  return { std::move(w), CreateIndexFile(std::move(index)) };
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
