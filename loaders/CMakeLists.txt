set( targetName "loaders" )

include_directories(
	${PROJECT_SOURCE_DIR}/extern/tinygltf
	${PROJECT_SOURCE_DIR}/nodegraph
	${PROJECT_SOURCE_DIR}/renderer
	${PROJECT_SOURCE_DIR}/vulkanutils
        ${PROJECT_SOURCE_DIR}/engine
)

add_library( ${targetName} ${LIB_TYPE}
	gltfloader.h
	gltfloader.cpp
)
target_link_libraries( ${targetName} nodegraph )

