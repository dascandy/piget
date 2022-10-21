#include "piget/GitCAM.hpp"
#include "piget/Object.hpp"
#include "tl/expected.hpp"
#include "caligo/sha1.h"
#include "decoco/decoco.hpp"
#include <optional>
#include <filesystem>
#include <fstream>

GitCAM::GitCAM(std::filesystem::path root)
: root(root)
{
}

static std::string asId(std::span<const uint8_t> input) {
  char hextab[17] = "0123456789abcdef";
  std::string rv;
  for (auto& c : input) {
    rv.push_back(hextab[c >> 4]);
    rv.push_back(hextab[c & 0xF]);
  }
  return rv;
}

std::array<uint8_t, 20> GitCAM::add(Object object) {
  auto hash = object.id();
  std::string id = asId(hash);
  std::filesystem::path filename = root / id.substr(0, 2) / id.substr(2);
  if (std::filesystem::is_regular_file(filename)) {
    // soft error, already exists
    return hash;
  } else if (std::filesystem::exists(filename)) {
    // expected!
    throw std::runtime_error("broken repository");
  } else {
    std::filesystem::create_directories(filename.parent_path());
    std::vector<uint8_t> compressedData = Decoco::compress(Decoco::ZlibCompressor(), object.buffer);
    std::ofstream(filename).write(reinterpret_cast<const char*>(compressedData.data()), compressedData.size());
    return hash;
  }
}

tl::expected<Object, std::error_code> GitCAM::get(std::array<uint8_t, 20> hash) {
  std::string id = asId(hash);
  std::filesystem::path file = root / id.substr(0, 2) / id.substr(2);
  std::error_code ec;
  size_t size = std::filesystem::file_size(file, ec);
  if (ec) return tl::make_unexpected(ec);
  std::vector<uint8_t> buffer;
  buffer.resize(size);
  std::ifstream(file).read(reinterpret_cast<char*>(buffer.data()), buffer.size());
  return Object(Decoco::decompress(Decoco::ZlibDecompressor(), buffer));
}

