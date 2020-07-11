
#include "renderer.h"

#include <exception>

int main(int argc, char* argv[])
{
  try {
    Renderer rend;
    rend.run();
  } catch ( std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
