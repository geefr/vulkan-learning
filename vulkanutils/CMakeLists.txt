set( targetName "vulkanutils" )

add_library( vulkanutils ${VULKANUTILS_LIB_TYPE}
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
target_link_libraries( ${targetName} Vulkan::Vulkan glfw )

