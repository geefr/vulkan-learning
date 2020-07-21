set( targetName "engine" )

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/vulkanutils
    ${PROJECT_SOURCE_DIR}/renderer
    ${PROJECT_SOURCE_DIR}/nodegraph
		${PROJECT_SOURCE_DIR}/extern/tinygltf
)

add_library( ${targetName} ${LIB_TYPE}
	engine.h
	engine.cpp
	event.h
	event.cpp
	camera.h
	camera.cpp
  light.h
  light.cpp
  vertex.h
	mesh.h
  mesh.cpp
	material.h
	material.cpp
	loaders/gltfloader.h
	loaders/gltfloader.cpp
)
target_link_libraries( ${targetName} renderer nodegraph )

