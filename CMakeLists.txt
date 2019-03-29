cmake_minimum_required(VERSION 3.7)

project(vulkan-experiments)

if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE Release CACHE PATH "Choose the type of build." FORCE )
  set_property( CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Release" "Debug" "MinSizeRel" "RelWithDebInfo" )
endif()

# Compiler/Tool requirements
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_FLAGS "-Werror=all" )

# Package/System requirements
find_package(Vulkan REQUIRED)

add_subdirectory( 01-initialisation )
