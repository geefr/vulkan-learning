cmake_minimum_required(VERSION 3.16)

project(vulkan-stuff)

if( NOT CMAKE_BUILD_TYPE )
    set( CMAKE_BUILD_TYPE Release CACHE PATH "Choose the type of build." FORCE )
    set_property( CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Release" "Debug" "MinSizeRel" "RelWithDebInfo" )
endif()

# Compiler/Tool requirements
set( CMAKE_CXX_STANDARD 20 )

if( CMAKE_BUILD_TYPE MATCHES "Debug" )
    add_compile_definitions( DEBUG )
endif()

# Enable presentation surfaces

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -pedantic-errors -Werror=shadow -Werror=all -Wno-error=old-style-cast -Wno-error=unused-variable" )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -pedantic-errors -Werror=shadow -Werror=all -Wno-error=old-style-cast -Wno-error=unused-variable -Wno-error=unused-but-set-variable" )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    message( FATAL_ERROR "Project not setup for Intel compiler")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # add_compile_definitions( VK_USE_PLATFORM_WIN32_KHR )
    add_compile_definitions( NOMINMAX ) # Thanks Microsoft..
    add_compile_definitions( _USE_MATH_DEFINES )
endif()

if( MSVC )
    set( glslCompiler "glslangValidator.exe" )
else()
    find_package(X11 REQUIRED)
    set( glslCompiler "$ENV{VULKAN_SDK}/bin/glslangValidator" )
    # Just assume we're on desktop linux, couldn't possibly bite us later :D
    # add_compile_definitions( VK_USE_PLATFORM_XLIB_KHR )
    # add_compile_definitions( VK_USE_PLATFORM_LIB_XCB_KHR )
endif()

# Package/System requirements
find_package(Vulkan REQUIRED)

message( "Looking for glslangValidator: ${glslCompiler}")

# This is a little ugly but will work
if(NOT EXISTS "${glslCompiler}")
    set( glslCompiler "${Vulkan_INCLUDE_DIR}/../bin/${glslCompiler}")
endif()
if(NOT EXISTS "${glslCompiler}")
    message( FATAL_ERROR "Unable to locate 'glslangValidator'. Please install glsl-tools or provide a Vulkan SDK which includes this tool")
endif()
message( "Using GLSL Compiler at: ${glslCompiler}")

if( MSVC )
    # TODO: Hack - The binary glfw releases don't include a cmake config and windows cmake doesn't have a FindGLFW.cmake
    # For now just include it as source
    if(NOT GLFW_SOURCE_PATH )
        message( FATAL_ERROR "You're building on windows, set GLFW_SOURCE_PATH to the location of glfw3's source")
    endif()
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory( ${GLFW_SOURCE_PATH} ${CMAKE_CURRENT_BINARY_DIR}/glfw3 )
else()
    find_package(glfw3 REQUIRED)
endif()
# TODO: For now glfw is required, the renderer won't compile without it
add_compile_definitions( USE_GLFW )

find_package(glm REQUIRED)
# Setup glm for our cs - Left handed, vulkan/Direct3D depth range
# Set glm to be compatible with vulkan
add_compile_definitions( GLM_DEPTH_ZERO_TO_ONE GLM_FORCE_LEFT_HANDED )
include_directories( ${GLM_INCLUDE_DIRS} )

set( LIB_TYPE STATIC )

# Compile a shader to SPIRV and setup the needed dependencies
# _target - The target which requires this shader
# _shadertarget - Name of the custom target for compiling the shader
# _shaderFile - The shader file to compile
# _additionalSrc - Additional files to consider as sources for the target
function(compile_shader _target _shaderTarget _shaderFile _additionalSrc)
    get_filename_component( shaderFileBaseName ${_shaderFile} NAME )
    add_custom_target( ${_shaderTarget}
        COMMAND ${glslCompiler} -V ${_shaderFile} -o ${shaderFileBaseName}.spv
        SOURCES ${_shaderFile} ${_additionalSrc})
    add_dependencies( ${_target} ${_shaderTarget} )
endfunction()

# Debug flags and other hacks
# add_compile_definitions(ENABLE_VK_DEBUG)

# Set to always select discrete gpu over integrated
add_compile_definitions(PREFER_DISCRETE_GPU)

add_subdirectory( vulkanutils )
add_subdirectory( renderer )
add_subdirectory( nodegraph )
add_subdirectory( engine )
add_subdirectory( tests )
