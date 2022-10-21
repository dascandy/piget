#include "piget/GitCAM.hpp"
#include "piget/Object.hpp"
#include "tl/expected.hpp"
#include <optional>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <bini/reader.h>
#include <bini/writer.h>
#include "caligo/sha1.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>

static constexpr const uint32_t DIRCACHE_MAGIC_NUMBER = 0x44495243;
static constexpr const uint32_t DIRCACHE_CURRENT_VERSION = 2;

Index::Index(GitCAM& cam, bool withLock)
: cam(cam)
{
  if (withLock) {

    haveLock = true;
  }
  load();
}

Index::~Index() {
  save();
  if (haveLock) {

  }
}

Object Index::toTree(std::optional<std::array<uint8_t, 20>> parentCommit) {
  std::map<std::filesystem::path, Tree> pendingTrees;

  if (parentCommit) {
    auto parent = cam.get(parentCommit.value());
    if (not parent) {
      throw std::runtime_error("Invalid parent commit");
    }
    pendingTrees[""] = parent->readAsTree();
  } else {
    pendingTrees[""];
  }
  for (const auto& [name, obj] : objects) {
    std::vector<std::filesystem::path> treesToLoad;
    treesToLoad.push_back(name.parent_path());
    while (not treesToLoad.empty()) {
      auto path = treesToLoad.back();
      if (auto it = pendingTrees.find(path.parent_path()); it != pendingTrees.end()) {
        auto file = it->second.get(path.filename());
        if (file) {
          auto blob = cam.get(file.value());
          if (not blob) {
            throw std::runtime_error("Corrupted storage; backing file for tree deleted");
          }
          // load tree
          pendingTrees[path] = cam.get(file.value())->readAsTree();
        } else {
          // create new empty tree for this folder
          pendingTrees[path];
        }
        treesToLoad.pop_back();
      } else {
        treesToLoad.push_back(path.parent_path());
      }
    }
    pendingTrees[name.parent_path()].set(name.filename(), DirEntry{static_cast<uint16_t>(obj.mode), name, obj.hash});
  }
  while (pendingTrees.size() > 1) {
    auto it = --pendingTrees.end();
    auto [path, tree] = *it;
    pendingTrees.erase(it);
    Object obj(tree);
    pendingTrees[path.parent_path()].set(path.filename(), { 040000, path.filename(), obj.id() });
    cam.add(obj);
  }
  return Object(pendingTrees[""]);
}

void Index::add(std::filesystem::path path) {
  struct stat statbuf;
  if (stat(path.c_str(), &statbuf) == -1) {
    throw std::runtime_error("error " + std::to_string(errno));
  }
  Entry e;
  e.ctime_sec = statbuf.st_ctim.tv_sec;
  e.ctime_ns = statbuf.st_ctim.tv_nsec;
  e.mtime_sec = statbuf.st_mtim.tv_sec;
  e.mtime_ns = statbuf.st_mtim.tv_nsec;
  e.dev = statbuf.st_dev;
  e.ino = statbuf.st_ino;
  e.mode = statbuf.st_mode;
  e.uid = statbuf.st_uid;
  e.gid = statbuf.st_gid;
  e.filesize = statbuf.st_size;
  e.flags = path.string().size() > 0xFFF ? 0xFFF : path.string().size();
  e.fileName = path;

  Object obj(path);
  e.hash = obj.id();
  cam.add(obj);
  objects.insert(std::make_pair(path, std::move(e)));
}

void Index::remove(std::filesystem::path path) {
  objects.erase(path);
}

void Index::load() {
  if (not std::filesystem::is_regular_file(".git/index")) 
    return;

  size_t indexsize = std::filesystem::file_size(".git/index");
  std::vector<uint8_t> file;
  file.resize(indexsize);
  std::ifstream(".git/index").read((char*)file.data(), file.size());
  
  std::array<uint8_t, 20> calculatedHash = Caligo::SHA1(std::span<const uint8_t>(file.data(), file.size() - 20));
  std::array<uint8_t, 20> readHash;
  memcpy(readHash.data(), file.data() + file.size() - 20, 20);
  if (calculatedHash != readHash) {
    objects.clear();
    throw std::runtime_error("Index corrupted");
  }

  Bini::reader r(file);
  uint32_t id = r.read32be();
  if (id != DIRCACHE_MAGIC_NUMBER) throw std::runtime_error("invalid magic value");
  uint32_t version = r.read32be();
  if (version != DIRCACHE_CURRENT_VERSION) throw std::runtime_error("Unexpected version " + std::to_string(version));
  uint32_t entryCount = r.read32be();
  for (size_t n = 0; n < entryCount; n++) {
    Entry e;
    e.ctime_sec = r.read32be();
    e.ctime_ns = r.read32be();
    e.mtime_sec = r.read32be();
    e.mtime_ns = r.read32be();
    e.dev = r.read32be();
    e.ino = r.read32be();
    e.mode = r.read32be();
    e.uid = r.read32be();
    e.gid = r.read32be();
    e.filesize = r.read32le();
    e.hash = r.getArray<20>();
    e.flags = r.read16be();
    e.fileName = r.getStringNT();
    // align end again
    r.skip(7 - ((e.fileName.size() + 6) % 8));

    if (r.fail()) {
      throw std::runtime_error("Error while reading " + std::to_string(n+1) + "th item from index");
    }
    std::string fileName = e.fileName;
    objects[fileName] = std::move(e);
  }
}

void Index::save() {
  Bini::writer w;
  w.add32be(DIRCACHE_MAGIC_NUMBER);
  w.add32be(DIRCACHE_CURRENT_VERSION);
  w.add32be(objects.size());
  for (auto& [_, e] : objects) {
    w.add32be(e.ctime_sec);
    w.add32be(e.ctime_ns);
    w.add32be(e.mtime_sec);
    w.add32be(e.mtime_ns);
    w.add32be(e.dev);
    w.add32be(e.ino);
    w.add32be(e.mode);
    w.add32be(e.uid);
    w.add32be(e.gid);
    w.add32be(e.filesize);
    w.add(e.hash);
    uint16_t length = (e.fileName.size() > 0xFFF ? 0xFFF : e.fileName.size());
    w.add16be((e.flags & 0xF000) | length);
    w.addNT(e.fileName);
    w.addpadding(7 - ((e.fileName.size() + 6) % 8), '\0');
  }
  w.add(Caligo::SHA1(w).data());
}


