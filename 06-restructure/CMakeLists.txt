set( targetName "06-restructure" )

add_executable(${targetName} main.cpp
	util/simplebuffer.h
	util/simplebuffer.cpp
  util/util.h
  util/util.cpp
  util/windowintegration.h
  util/windowintegration.cpp
  util/framebuffer.h
  util/framebuffer.cpp
  util/deviceinstance.h
  util/deviceinstance.cpp
  util/graphicspipeline.h
  util/graphicspipeline.cpp
	)
target_link_libraries( ${targetName} Vulkan::Vulkan glfw ${X11_LIBRARIES} )

add_custom_target(06-compileshader_vert
	COMMAND /home/gareth/dev/thirdparty/vulkansdk/1.1.101.0/x86_64/bin/glslangValidator -V ${CMAKE_CURRENT_SOURCE_DIR}/test.vert
	SOURCES test.vert )
add_custom_target(06-compileshader_frag
	COMMAND /home/gareth/dev/thirdparty/vulkansdk/1.1.101.0/x86_64/bin/glslangValidator -V ${CMAKE_CURRENT_SOURCE_DIR}/test.frag
	SOURCES test.frag )

add_dependencies( ${targetName} 06-compileshader_vert 06-compileshader_frag )
