set( targetName "03-device-queues" )
add_executable(${targetName} main.cpp)
target_link_libraries( ${targetName} Vulkan::Vulkan )
