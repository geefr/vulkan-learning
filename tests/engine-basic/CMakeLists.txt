set( targetName "engine-basic" )

include_directories(
	${PROJECT_SOURCE_DIR}/vulkanutils
	${PROJECT_SOURCE_DIR}/renderer/
	${PROJECT_SOURCE_DIR}/engine/
)

add_executable(${targetName}
  main.cpp
	)
target_link_libraries( ${targetName} engine )
