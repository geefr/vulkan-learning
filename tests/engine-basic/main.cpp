
#include "engine.h"

#include <exception>
#include <iostream>

int main(int argc, char* argv[])
{
  try {
    Engine eng;
    eng.run();
  } catch ( std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
