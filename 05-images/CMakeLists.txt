set( targetName "05-images" )

add_executable(${targetName} main.cpp
	util/registrar.h
	util/registrar.cpp
  util/simplebuffer.h
  util/simplebuffer.cpp
	)
target_link_libraries( ${targetName} Vulkan::Vulkan glfw ${X11_LIBRARIES} )

add_custom_target(compileshader_vert
  COMMAND /home/gareth/dev/thirdparty/vulkansdk/1.1.101.0/x86_64/bin/glslangValidator -V ${CMAKE_CURRENT_SOURCE_DIR}/test.vert
  SOURCES test.vert )
add_custom_target(compileshader_frag
  COMMAND /home/gareth/dev/thirdparty/vulkansdk/1.1.101.0/x86_64/bin/glslangValidator -V ${CMAKE_CURRENT_SOURCE_DIR}/test.frag
  SOURCES test.frag )

add_dependencies( ${targetName} compileshader_vert compileshader_frag )
