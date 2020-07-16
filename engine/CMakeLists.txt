set( targetName "engine" )

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/vulkanutils
    ${PROJECT_SOURCE_DIR}/renderer
    ${PROJECT_SOURCE_DIR}/nodegraph
)

add_library( ${targetName} ${LIB_TYPE}
	engine.h
	engine.cpp
	event.h
	event.cpp
	camera.h
	camera.cpp
        vertex.h
	mesh.h
        mesh.cpp
	material.h
)
target_link_libraries( ${targetName} renderer nodegraph )

