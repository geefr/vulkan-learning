cmake_minimum_required(VERSION 3.12)

project(vulkan-experiments)

if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE Release CACHE PATH "Choose the type of build." FORCE )
  set_property( CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Release" "Debug" "MinSizeRel" "RelWithDebInfo" )
endif()

# Compiler/Tool requirements
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_FLAGS "-Werror=all -Wno-error=old-style-cast -Wno-error=unused-variable -Wno-error=unused-but-set-variable" )
if( CMAKE_BUILD_TYPE MATCHES "Debug" )
  add_compile_definitions( DEBUG )
endif()

# Enable presentation surfaces
if( MSVC )
  add_compile_definitions( VK_USE_PLATFORM_WIN32_KHR )
else()
  find_package(X11 REQUIRED)
  find_package(glfw3 REQUIRED)

  # Just assume we're on desktop linux, couldn't possibly bite us later :D
  # add_compile_definitions( VK_USE_PLATFORM_XLIB_KHR )
  # add_compile_definitions( VK_USE_PLATFORM_LIB_XCB_KHR )
  add_compile_definitions( USE_GLFW )
endif()

# Package/System requirements
find_package(Vulkan REQUIRED)

add_subdirectory( 01-initialisation )
add_subdirectory( 02-create-logical-device )
add_subdirectory( 03-device-queues )
add_subdirectory( 04-restructure )
add_subdirectory( 05-images )
add_subdirectory( 06-restructure )
