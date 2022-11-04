#include "catch2/catch_all.hpp"
#include "piget/GitCAM.hpp"
#include "piget/Object.hpp"

namespace Piget {

TEST_CASE("store and read file from gitcam") {
  std::array<uint8_t, 20> helloid;
  {
    Object hello("libpiget/test/hello.txt");
    helloid = hello.id();

    GitCAM db("objects");
    db.add(hello);
  }

  {
    GitCAM db("objects");
    auto hello2 = db.get(helloid);
    (void)hello2;
  }
}

}
