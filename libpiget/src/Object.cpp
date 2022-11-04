#include "piget/Object.hpp"
#include "tl/expected.hpp"
#include "caligo/sha1.h"
#include "decoco/decoco.hpp"
#include "bini/reader.h"
#include <optional>
#include <filesystem>
#include <fstream>

std::vector<std::string_view> split(std::string_view sv, char split = ' ') {
  std::vector<std::string_view> rv;
  size_t start = 0;
  size_t end = sv.find_first_of(split, start);
  while (end != std::string::npos) {
    rv.push_back(sv.substr(start, end - start));
    start = end + 1;
    end = sv.find_first_of(split, start);
  }
  rv.push_back(sv.substr(start));

  return rv;
}

std::array<uint8_t, 20> fromId(std::string_view sv) {
  static std::array<int, 256> lookup = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 
    0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  };
  if (sv.size() != 40) throw std::runtime_error("invalid id");
  std::array<uint8_t, 20> rv;
  for (size_t n = 0; n < 40; n+=2) {
    rv[n/2] = (lookup[sv[n]] << 4) | (lookup[sv[n+1]]);
  }
  return rv;
}

std::string asId(std::span<const uint8_t> input) {
  char hextab[17] = "0123456789abcdef";
  std::string rv;
  for (auto& c : input) {
    rv.push_back(hextab[c >> 4]);
    rv.push_back(hextab[c & 0xF]);
  }
  return rv;
}

std::array<uint8_t, 20> Object::id() const {
  return Caligo::SHA1(buffer).data();
}

Object::Object(std::filesystem::path path) {
  size_t length = std::filesystem::file_size(path);
  std::string prefix = "blob " + std::to_string(length);
  buffer.resize(prefix.size() + 1 + length);
  memcpy(buffer.data(), prefix.data(), prefix.size() + 1);
  std::ifstream(path).read((char*)buffer.data() + prefix.size() + 1, length);
}

Object::Object(Tree tree) {
  std::string prefix = "tree ";
  size_t dataLength = 0;
  for (auto& entry : tree.entries) {
    dataLength += 28 + entry.fileName.size();
  }
  prefix += std::to_string(dataLength);
  buffer.resize(dataLength + prefix.size() + 1);
  memcpy(buffer.data(), prefix.data(), prefix.size() + 1);
  uint8_t* p = buffer.data() + prefix.size() + 1;
  for (auto& entry : tree.entries) {
    char modeBuffer[20];
    sprintf(modeBuffer, "%06o ", entry.fileMode);
    memcpy(p, modeBuffer, 7);
    p += 7;
    memcpy(p, entry.fileName.c_str(), entry.fileName.size() + 1);
    p += entry.fileName.size() + 1;
    memcpy(p, entry.hash.data(), 20);
    p += 20;
  }
}

Tree Object::readAsTree() {
  if (type() != Type::Tree) {
    throw std::runtime_error("Non-tree object in tree position, repo corrupted");
  }
  Tree t;
  Bini::reader r(data());
  while (r.sizeleft()) {
    std::string_view str(r.getStringNT());
    size_t space = str.find(" ");
    uint16_t mode = std::stol(std::string(str.substr(0, space)), nullptr, 8);
    std::array<uint8_t, 20> hash(r.getArray<20>());
    t.entries.push_back(DirEntry{mode, std::string(str.substr(space+1)), {}});
    t.entries.back().hash = hash;
  }
  return t;
}

Object::Object(Commit commit) {
  std::string body = "tree " + asId(commit.root) + "\n";
  if (commit.parent) body += "parent " + asId(commit.parent.value()) + "\n";
  body += "author " + to_string(commit.author) + "\n";
  body += "committer " + to_string(commit.committer) + "\n\n";
  body += commit.message;
  std::string prefix = "commit " + std::to_string(body.size());
  buffer.resize(prefix.size() + body.size() + 1);
  memcpy(buffer.data(), prefix.data(), prefix.size() + 1);
  memcpy(buffer.data() + prefix.size() + 1, body.data(), body.size());
}

Commit Object::readAsCommit() {
  if (type() != Object::Type::Commit) {
    throw std::runtime_error("Non-commit object in commit position, repo corrupted");
  }
  Commit commit;
  std::string_view str((const char*)data().data(), data().size());
  size_t splitPoint = str.find("\n\n");
  if (splitPoint == std::string::npos) throw std::runtime_error("no commit message");
  commit.message = std::string(str.substr(splitPoint+2));
  for (auto& headerEntry : split(str.substr(0, splitPoint), '\n')) {
    size_t firstSpace = headerEntry.find(" ");
    if (firstSpace == std::string::npos) continue;
    std::string_view name = headerEntry.substr(0, firstSpace);
    std::string_view value = headerEntry.substr(firstSpace+1);
    if (name == "author") {
      commit.author = UserWithTime::from_string(value);
    } else if (name == "committer") {
      commit.committer = UserWithTime::from_string(value);
    } else if (name == "tree") {
      commit.root = fromId(value);
    } else if (name == "parent") {
      commit.parent = fromId(value);
    }
  }
  return commit;
}

Object::Object(Object::Type type, std::vector<uint8_t> data) 
: buffer(std::move(data))
{
  std::string prefix;
  switch(type) {
    case Object::Type::Tree: prefix = "tree "; break;
    case Object::Type::Commit: prefix = "commit "; break;
    case Object::Type::Object: prefix = "blob "; break;
    case Object::Type::Invalid: prefix = "invalid "; break;
  }
  prefix += std::to_string(data.size());
  buffer.resize(prefix.size() + 1 + data.size());
  memcpy(buffer.data(), prefix.data(), prefix.size() + 1);
  memcpy(buffer.data() + prefix.size() + 1, data.data(), data.size());
}

Object::Object(std::vector<uint8_t> data) 
: buffer(std::move(data))
{
}

Object::Type Object::type() const {
  const char* start = (const char*)buffer.data();
  const char* end = strchr(start, ' ');
  std::string_view sv{start, end};
  if (sv == "tree") {
    return Object::Type::Tree;
  } else if (sv == "commit") {
    return Object::Type::Commit;
  } else if (sv == "blob") {
    return Object::Type::Object;
  } else {
    return Object::Type::Invalid;
  }
}

std::span<const uint8_t> Object::data() const {
  auto it = std::find(buffer.begin(), buffer.end(), '\0');
  ++it;
  return {it, buffer.end()};
}

