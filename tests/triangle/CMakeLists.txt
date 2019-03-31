set( targetName "triangle" )

include_directories(${PROJECT_SOURCE_DIR}/vulkanutils)

add_executable(${targetName}
  main.cpp
  vulkanapp.h
  vulkanapp.cpp
	)
target_link_libraries( ${targetName} Vulkan::Vulkan glfw vulkanutils )

set( glslCompiler "${Vulkan_INCLUDE_DIR}/../Bin/glslangValidator")

add_custom_target(triangle-shader-vert
	COMMAND ${glslCompiler} -V ${CMAKE_CURRENT_SOURCE_DIR}/test.vert
	SOURCES test.vert )
add_custom_target(triangle-shader-frag
	COMMAND ${glslCompiler} -V ${CMAKE_CURRENT_SOURCE_DIR}/test.frag
	SOURCES test.frag )

add_dependencies( ${targetName} triangle-shader-vert triangle-shader-frag )
