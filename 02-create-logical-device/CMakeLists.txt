set( targetName "02-create-logical-device" )
add_executable(${targetName} main.cpp)
target_link_libraries( ${targetName} Vulkan::Vulkan )
