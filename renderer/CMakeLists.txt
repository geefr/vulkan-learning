set( targetName "renderer" )

include_directories(
	${PROJECT_SOURCE_DIR}/vulkanutils
	${PROJECT_SOURCE_DIR}/engine
)

add_definitions( "-DRENDERER_SHADER_ROOT=\"${CMAKE_CURRENT_BINARY_DIR}\"" )

add_library( ${targetName} ${LIB_TYPE}
  renderer.h
  renderer.cpp
	)
target_link_libraries( ${targetName} Vulkan::Vulkan glfw vulkanutils engine )

compile_shader(${targetName} ${targetName}-mesh-vert ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.vert ${CMAKE_CURRENT_SOURCE_DIR}/shaders/interface_uniforms.inc)
compile_shader(${targetName} ${targetName}-flatshading-frag ${CMAKE_CURRENT_SOURCE_DIR}/shaders/flatshading.frag ${CMAKE_CURRENT_SOURCE_DIR}/shaders/interface_uniforms.inc)
compile_shader(${targetName} ${targetName}-phongish-frag ${CMAKE_CURRENT_SOURCE_DIR}/shaders/phongish.frag ${CMAKE_CURRENT_SOURCE_DIR}/shaders/interface_uniforms.inc)


