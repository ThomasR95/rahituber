cmake_minimum_required(VERSION 3.5)
project(RahiTuber)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "RahiTuber")

if(${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
   message(FATAL_ERROR "In-tree builds are NOT supported.")
endif()

#
# Option prep
#

option(USE_SYSTEM_LIBS "Use system-installed libraries instead of bundled ones (Linux only)" ${CMAKE_SYSTEM_NAME_MATCHES_LINUX})


# Default USE_SYSTEM_LIBS to ON when building on Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    option(USE_SYSTEM_LIBS "Use system-installed libraries instead of bundled ones" ON)
else()
    option(USE_SYSTEM_LIBS "Use system-installed libraries instead of bundled ones" OFF)
endif()



if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    ## ???
    #add_compile_definitions(SFML_STATIC)
    #set(BUILD_SHARED_LIBS OFF CACHE BOOL "Turn off to disable SFML building shared libs" FORCE)
    #set(PA_DISABLE_INSTALL ON CACHE BOOL "Disable targets install and uninstall (for embedded builds)" FORCE)
    #set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "Build tests for tinyxml2" FORCE)
else()
    add_compile_definitions(SFML_STATIC)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Turn off to disable SFML building shared libs" FORCE)
    set(PA_DISABLE_INSTALL ON CACHE BOOL "Disable targets install and uninstall (for embedded builds)" FORCE)
    set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "Build tests for tinyxml2" FORCE)
    set(SPOUT_BUILD_CMT OFF CACHE BOOL "For Visual Studio - build /MT to link runtime libraries" FORCE)
endif()

#
# Add dependencies
#

# --- freetype ---
if(USE_SYSTEM_LIBS)
    find_package(Freetype)
endif()
#if(NOT TARGET freetype AND NOT FREETYPE_FOUND)
if(WIN32)
    add_subdirectory(Libraries/freetype)
endif()

# --- portaudio ---
if(USE_SYSTEM_LIBS)
    find_package(portaudio)
endif()
#if(NOT TARGET portaudio AND NOT portaudio_FOUND)
if(WIN32)
    add_subdirectory(Libraries/portaudio)
endif()

# --- SFML ---
if(USE_SYSTEM_LIBS)
    find_package(SFML 2 COMPONENTS graphics window system audio)
endif()
#if(NOT SFML_FOUND)
if(WIN32)
    add_subdirectory(Libraries/SFML)
endif()

if(WIN32)
    add_subdirectory(Libraries/Spout2)
endif()

# --- tinyxml2 ---
if(USE_SYSTEM_LIBS)
    find_package(tinyxml2)
endif()
#if(NOT TARGET tinyxml2 AND NOT tinyxml2_FOUND)
if(WIN32)
    add_subdirectory(Libraries/tinyxml2)
endif()


# Set folder of Spout projects (for IDEs which support them)
if(WIN32)
    set_target_properties("Spout" PROPERTIES FOLDER "Spout")
    set_target_properties("Spout_static" PROPERTIES FOLDER "Spout")
    set_target_properties("SpoutLibrary" PROPERTIES FOLDER "Spout")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(imgui)
else()
    include(imgui) # imgui doesn't ship with a standard CMakeLists.txt
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(mongoose)
else()
    include(mongoose) # mongoose doesn't ship with a standard CMakeLists.txt
endif()


if(WIN32)
    target_include_directories(sfml-graphics PRIVATE ${CMAKE_SOURCE_DIR}/Libraries/freetype/include)
    target_include_directories(freetype PRIVATE ${PROJECT_BINARY_DIR}/include/freetype/config)
    target_include_directories(sfml-graphics PRIVATE ${CMAKE_SOURCE_DIR}/Libraries/Simple-FFT/include)
endif()

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

#
# Add test project
#
if(MSVC)
    add_subdirectory(RahiTuber_Test)
endif()
