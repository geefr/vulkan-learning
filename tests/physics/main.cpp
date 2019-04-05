
#include "vulkanapp.h"
#include "physics.h"

#include <exception>
#include <iostream>

int main(int argc, char* argv[])
{
  try {
    VulkanApp app;
    app.run();
  } catch ( std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
