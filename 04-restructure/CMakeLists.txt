set( targetName "04-restructure" )
add_executable(${targetName} main.cpp
	util/registrar.h
	util/registrar.cpp
  util/simplebuffer.h
  util/simplebuffer.cpp
	)
target_link_libraries( ${targetName} Vulkan::Vulkan )
