#include "piget/Repository.hpp"
#include <print>
#include <span>
#include <string_view>
#include <map>
#include <functional>
#include <filesystem>
#include <vector>

void git_init(std::span<std::string_view> args) {
  (void)args;
  std::filesystem::path cwd = std::filesystem::current_path();
  auto repo = Piget::Repository::Init(cwd, false);
  if (not repo) {
    std::print("Could not initialize repository at {}\n", cwd);
    exit(-1);
  }
}

void git_commit(std::span<std::string_view> args) {
  (void)args;

}

void git_help(std::span<std::string_view> args);

struct Operation {
  std::string_view description;
  std::function<void(std::span<std::string_view> args)> call;
};

std::map<std::string_view, Operation> operations = {
  { "init", { "Create an empty Git repository or reinitialize an existing one", git_init } },
  { "help", { "Show an overview of commands that can be run", git_help } },
  { "commit", { "Record changes to the repository", git_commit } },
};

void git_help(std::span<std::string_view> args) {
  std::print("These are common Git commands used in various situations:\n\n");
  for (auto& [name, operation] : operations) {
    std::print("   {}   {}\n", name, operation.description);
  }
  (void)args;
  std::print("\n");
}

int main(int argc, char** argv) {
  std::vector<std::string_view> args{argv, argv + argc};
  if (args.size() == 0) {
    std::print("Invalid command invocation\n");
    exit(-1);
  } else if (args.size() == 1) {
    git_help(args);
    exit(-1);
  }
  auto it = operations.find(args[1]);
  if (it == operations.end()) {
    std::print("{}: '{}' is not a piget command. See '{} help'.\n", args[0], args[1], args[0]);
  } else {
    it->second.call(args);
  }
}


