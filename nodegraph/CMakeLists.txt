set( targetName "nodegraph" )

include_directories( 
	${PROJECT_SOURCE_DIR}/renderer 
	${PROJECT_SOURCE_DIR}/vulkanutils
  ${PROJECT_SOURCE_DIR}/engine
)
add_library( ${targetName} ${LIB_TYPE}
	node.h
	node.cpp
	meshnode.h
	meshnode.cpp
	)
target_link_libraries( ${targetName} renderer engine )
