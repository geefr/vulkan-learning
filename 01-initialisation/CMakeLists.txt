set( targetName "01-initialisation" )
add_executable(${targetName} main.cpp)
target_link_libraries( ${targetName} Vulkan::Vulkan )
