#pragma once

#include <optional>
#include <filesystem>
#include "tl/expected.hpp"
#include "GitCAM.hpp"

namespace Piget {

struct Repository {
  static std::optional<Repository> Init(std::filesystem::path root, bool isBare);
  static std::optional<Repository> Open(std::filesystem::path path);
 
  std::filesystem::path repository, workspace;
  GitCAM objects;
  bool isBare = false;
  bool fileMode = true;
  bool ignoreCase = false;
  bool precomposeUnicode = true;
  bool logAllRefUpdates = true;
private:
  Repository(std::filesystem::path repository, std::filesystem::path workspace);
  tl::expected<void, std::error_code> writeConfig();
  tl::expected<void, std::error_code> readConfig();
};

}


