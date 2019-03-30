set( targetName "05-images" )
add_executable(${targetName} main.cpp
	util/registrar.h
	util/registrar.cpp
  util/simplebuffer.h
  util/simplebuffer.cpp
	)
target_link_libraries( ${targetName} Vulkan::Vulkan ${X11_LIBRARIES} )
