
#include <vulkan/vulkan.hpp>

#include <iostream>

int main(void)
{
  std::cout << "Creating a Vulkan instance" << std::endl;

  vk::ApplicationInfo applicationInfo(
              "01-initialisation", // Application Name
              0, // Application version
              "geefr", // engine name
              0, // engine version
              VK_API_VERSION_1_0 // absolute minimum vulkan api
              );

  vk::InstanceCreateInfo instanceCreateInfo(
              vk::InstanceCreateFlags(),
              &applicationInfo, // Application Info
              0, // enabledLayerCount - don't need any validation layers right now
              nullptr, // ppEnabledLayerNames
              0, // enabledExtensionCount - don't need any extensions yet. In vulkan these need to be requested on init unlike in gl
              nullptr // ppEnabledExtensionNames
              );

  vk::Instance instance;
  if( vk::createInstance(
              &instanceCreateInfo, // Creation info
              nullptr, // Allocator - use the built-in one for now
              &instance // Output, the created instance
              // Dispatch loader optional - don't know what it does ;)
              ) != vk::Result::eSuccess ) {
      std::cerr << "Failed to create Vulkan instance" << std::endl;
      return 1;
  }

  std::cout << "Woot! created my first vulkan instance" << std::endl;

  return 0;
}
