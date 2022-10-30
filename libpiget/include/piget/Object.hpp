#pragma once

#include "decoco/decoco.hpp"
#include "caligo/hash.h"
#include <map>
#include <filesystem>
#include <span>
#include <vector>
#include <cstdint>
#include <optional>
#include <string>
#include <tl/expected.hpp>

struct Object;
struct GitCAM;
struct Database;

struct DirEntry {
  uint16_t fileMode;
  std::string fileName;
  std::array<uint8_t, 20> hash;
};

struct User {
  std::string username;
  std::string email;
  friend std::string to_string(const User& uwt) {
    return uwt.username + " <" + uwt.email + ">";
  }
  inline static User from_string(std::string_view sv) {
    User user;
    if (size_t split = sv.find(" <"), end = sv.find_last_of('>'); split == std::string::npos || end == std::string::npos || end < split) {
      user.username = std::string(sv);
    } else {
      user.username = std::string(sv.substr(0, split));
      user.email = std::string(sv.substr(split+2, end - split - 2));
    }
    return user;
  }
};

struct UserWithTime {
  User user;
  int64_t time = 0;
  int16_t timezone = 0;
  friend std::string to_string(const UserWithTime& uwt) {
    char buffer[10];
    sprintf(buffer, "%+05d", uwt.timezone);
    return to_string(uwt.user) + " " + std::to_string(uwt.time) + " " + buffer;
  }
  inline static UserWithTime from_string(std::string_view sv) {
    UserWithTime uwt;
    size_t emailEnd = sv.find("> ");
    size_t space = sv.find_last_of(' ');
    if (emailEnd == std::string::npos || space == std::string::npos) {
      throw std::runtime_error("Invalid user specification");
    }
    uwt.user = User::from_string(sv.substr(0, emailEnd + 1));
    uwt.time = std::stol(std::string(sv.substr(emailEnd+2, space - emailEnd-2)));
    uwt.timezone = std::stol(std::string(sv.substr(space+1)));
    return uwt;
  }
};

struct Tree {
  std::vector<DirEntry> entries;
  void set(std::string fileName, DirEntry entry);
  std::optional<std::array<uint8_t, 20>> get(std::string fileName);
};

struct Commit {
  Commit() {}
  Commit(std::array<uint8_t, 20> root, UserWithTime author) : root(root), author(author), committer(author) {}
  Commit setParent(std::array<uint8_t, 20> parent) { this->parent = parent; return *this; }
  Commit setCommitter(UserWithTime ut) { this->committer = ut; return *this; }
  Commit setMessage(std::string message) { this->message = message; return *this; }
  std::array<uint8_t, 20> root;
  std::optional<std::array<uint8_t, 20>> parent;
  std::string message;
  UserWithTime author;
  UserWithTime committer;
};

struct Object {
  enum class Type {
    Invalid = -1,
    Commit = 0x1,
    Tree = 0x2,
    Object = 0x3,
  };
  Object(Commit commit);
  Object(std::filesystem::path path);
  Object(Tree tree);
  Object(std::vector<uint8_t> data);
  std::vector<uint8_t> buffer;
  Object::Type type() const;
  std::span<const uint8_t> data() const;
  std::array<uint8_t, 20> id() const;
  Tree readAsTree();
  Commit readAsCommit();
};

std::pair<std::vector<uint8_t>, std::vector<uint8_t>> WritePack(const Database& db, std::vector<std::array<uint8_t, 20>> objectIds);

struct Pack {
  Pack(std::span<const uint8_t> data, std::span<const uint8_t> index);
  struct IndexEntry {
    std::array<uint8_t, 20> id;
    std::array<uint8_t, 4> crc;
    size_t offset;
    Object::Type type;
  };
  std::optional<std::vector<uint8_t>> get(std::array<uint8_t, 20> id);
private:
  void LoadIndex(std::span<const uint8_t> in);
  void RegenerateIndex();
  std::span<const uint8_t> data;
  std::vector<IndexEntry> index;
};

