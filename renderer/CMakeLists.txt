set( targetName "renderer" )

include_directories(${PROJECT_SOURCE_DIR}/vulkanutils)

add_definitions( "-DRENDERER_SHADER_ROOT=\"${CMAKE_CURRENT_BINARY_DIR}\"" )

add_library( ${targetName} ${LIB_TYPE}
  renderer.h
  renderer.cpp
	)
target_link_libraries( ${targetName} Vulkan::Vulkan glfw vulkanutils )

compile_shader(${targetName} ${targetName}-flatshading-vert ${CMAKE_CURRENT_SOURCE_DIR}/flatshading.vert)
compile_shader(${targetName} ${targetName}-flatshading-frag ${CMAKE_CURRENT_SOURCE_DIR}/flatshading.frag)
