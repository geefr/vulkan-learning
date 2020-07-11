set( targetName "renderer-basic" )

include_directories(
	${PROJECT_SOURCE_DIR}/vulkanutils
	${PROJECT_SOURCE_DIR}/renderer/
)

add_executable(${targetName}
  main.cpp
	)
target_link_libraries( ${targetName} renderer )#Vulkan::Vulkan glfw vulkanutils )
