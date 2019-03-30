set( targetName "04-restructure" )
add_executable(${targetName} main.cpp
	util/registrar.h
	util/registrar.cpp
	)
target_link_libraries( ${targetName} Vulkan::Vulkan )
