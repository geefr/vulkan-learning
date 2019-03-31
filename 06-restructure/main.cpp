
#include "vulkanapp.h"

#include <exception>

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
