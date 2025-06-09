cmake_minimum_required(VERSION 3.5)
project(RahiTuber)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "RahiTuber")

#
# Option prep
#
add_compile_definitions(SFML_STATIC)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Turn off to disable SFML building shared libs" FORCE)
set(PA_DISABLE_INSTALL ON CACHE BOOL "Disable targets install and uninstall (for embedded builds)" FORCE)
set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "Build tests for tinyxml2" FORCE)
set(SPOUT_BUILD_CMT OFF CACHE BOOL "For Visual Studio - build /MT to link runtime libraries" FORCE)

#
# Add dependencies
#
add_subdirectory(Libraries/freetype)
add_subdirectory(Libraries/portaudio)
add_subdirectory(Libraries/SFML)
if(WIN32)
    add_subdirectory(Libraries/Spout2)
endif()
add_subdirectory(Libraries/tinyxml2)

include(imgui) # imgui doesn't ship with a standard CMakeLists.txt
include(mongoose) # mongoose doesn't ship with a standard CMakeLists.txt

#
# External dependencies
#
find_package(OpenGL REQUIRED)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11)
    find_package(ALSA)
endif()


#
# Add main project
#
add_subdirectory(RahiTuber)