cmake_minimum_required(VERSION 3.12)

project(vulkan-experiments)

if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE Release CACHE PATH "Choose the type of build." FORCE )
  set_property( CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Release" "Debug" "MinSizeRel" "RelWithDebInfo" )
endif()

# Compiler/Tool requirements
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_FLAGS "-Werror=all -Wno-error=old-style-cast" )
if( CMAKE_BUILD_TYPE MATCHES "Debug" )
  add_compile_definitions( DEBUG )
endif()

# Package/System requirements
find_package(Vulkan REQUIRED)

add_subdirectory( 01-initialisation )
add_subdirectory( 02-create-logical-device )
add_subdirectory( 03-enable-standard-validation-layers-in-debug )
