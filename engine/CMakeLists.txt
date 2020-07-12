set( targetName "engine" )

include_directories(
	${PROJECT_SOURCE_DIR}/vulkanutils
	${PROJECT_SOURCE_DIR}/renderer
	${PROJECT_SOURCE_DIR}/nodegraph
)

add_library( ${targetName} ${LIB_TYPE}
	engine.h
	engine.cpp
)
target_link_libraries( ${targetName} renderer nodegraph )

