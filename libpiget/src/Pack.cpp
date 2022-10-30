#include "piget/Object.hpp"
#include "piget/GitCAM.hpp"
#include "bini/writer.h"
#include "bini/reader.h"
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
  return main;
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
    index.push_back({id, Caligo::CRC32(obj.data()), w.size(), obj.type()});
    size_t s = obj.data().size();
    s = ((s & 0xFFFFFFFFFFFFF0) << 3) | (s & 0xF) | ((int)obj.type() << 4);
    w.addPB(s);
    w.add(Decoco::compress(Decoco::ZlibCompressor(), obj.data()));
  }
  w.add(Caligo::SHA1{w}.data());
  return { std::move(w), CreateIndexFile(std::move(index)) };
}

Pack::Pack(std::span<const uint8_t> data, std::span<const uint8_t> in_index)
: data(data)
{
  LoadIndex(in_index);

  if (index.empty()) {
    RegenerateIndex();
  }
};

void Pack::LoadIndex(std::span<const uint8_t> in) {
  (void)in;
  // TODO: load index
}

void Pack::RegenerateIndex() {
  Bini::reader r(data);
  uint32_t magic = r.read32be();
  uint32_t version = r.read32be();
  (void)magic;
  (void)version;
  uint32_t objcount = r.read32be();
  for (size_t n = 0; n < objcount; n++) {
    uint64_t size = r.getPB();
    Object::Type type = (Object::Type)((size >> 4) & 0x7);
    size = ((size & 0xFFFF'FFFF'FFFF'FF80) >> 3) | (size & 0xF);
    std::vector<uint8_t> data;
    auto decomp = Decoco::ZlibDecompressor();
    Bini::reader r2 = r;
    data = Decoco::decompress(decomp, r2.get(r2.sizeleft()));
    index.push_back({Caligo::SHA1(data).data(), Caligo::CRC32(data).data(), data.size() - r.sizeleft(), type});
    r.skip(decomp->bytesUsed());
  }
}

std::optional<std::vector<uint8_t>> Pack::get(std::array<uint8_t, 20> id) {
  auto it = std::lower_bound(index.begin(), index.end(), id, [](const IndexEntry& e, const std::array<uint8_t, 20>& id) {
    for (size_t n = 0; n < 20; n++) {
      if (e.id[n] < id[n]) return true;
      if (e.id[n] > id[n]) return false;
    }
    return false;
  });
  if (it == index.end() ||
      it->id != id) return std::nullopt;
  return Decoco::decompress(Decoco::ZlibDecompressor(), data.subspan(it->offset));
}


