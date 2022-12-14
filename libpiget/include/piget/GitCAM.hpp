#pragma once

#include <map>
#include <filesystem>
#include <cstdint>
#include <optional>
#include <tl/expected.hpp>
#include <array>
#include <vector>

struct Object;
struct GitCAM;
struct DirEntry;
struct Pack;

struct Index {
  struct Entry {
    uint32_t ctime_sec, ctime_ns;
    uint32_t mtime_sec, mtime_ns;
    uint32_t dev, ino;
    uint32_t mode;
    uint32_t uid, gid;
    uint32_t filesize;
    std::array<uint8_t, 20> hash;
    uint16_t flags;
    std::string fileName;
  };
  Index(GitCAM& cam, bool withLock = false);
  ~Index();
  Object toTree(std::optional<std::array<uint8_t, 20>> parentCommit);
  void add(std::filesystem::path path);
  
  void remove(std::filesystem::path path);

private:
  std::map<std::filesystem::path, Entry> objects;
  GitCAM& cam;
  bool haveLock = false;
  void load();
  void save();
};

struct GitCAM {
  GitCAM(std::filesystem::path root);
  std::array<uint8_t, 20> add(Object object);
  std::optional<Object> get(std::array<uint8_t, 20> id) const;

  std::filesystem::path root;
};

struct Database {
  GitCAM cam;
  std::vector<Pack*> packs;

  Database(std::filesystem::path root);
  std::optional<Object> get(std::array<uint8_t, 20> id) const;
  void add(const Object& object);
  void addPack(Pack p);
};

