#include "piget/Repository.hpp"
#include <filesystem>

namespace Piget {
/*
  std::error_code ec;
  std::filesystem::create_directory(repository, ec);
  if (ec)
    return std::nullopt;

  std::filesystem::create_directory(root / ".git" / "objects", ec);
  if (ec)
    return std::nullopt;

  std::filesystem::create_directory(root / ".git" / "refs", ec);
  if (ec)
    return std::nullopt;

  std::filesystem::create_directory(root / ".git" / "refs" / "heads", ec);
  if (ec)
    return std::nullopt;
*/

std::optional<Repository> Repository::Init(std::filesystem::path root, bool isBare) {
  std::filesystem::path repository, workspace;
  if (isBare) {
    repository = root;
  } else {
    repository = root / ".git";
    workspace = root;
  }
  
  Repository repo(repository, workspace);
  repo.isBare = isBare;

  if (not repo.writeConfig())
    return std::nullopt;

  return repo;
}

std::optional<Repository> Repository::Open(std::filesystem::path root) {
  std::filesystem::path repository, workspace;
  if (std::filesystem::is_directory(root / ".git")) {
    repository = root / ".git";
    workspace = root;
  } else {
    repository = root;
  }

  Repository repo{repository, workspace};

  if (not repo.readConfig())
    return std::nullopt;

  return repo;
}

Repository::Repository(std::filesystem::path repository, std::filesystem::path workspace)
: repository(repository)
, workspace(workspace)
, objects(repository / "objects")
{
}

tl::expected<void, std::error_code> Repository::writeConfig() {
  
  return {};
}

tl::expected<void, std::error_code> Repository::readConfig() {

  return {};
}

}


