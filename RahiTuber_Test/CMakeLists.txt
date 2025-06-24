 # If this minimum version is an issue then lower to 3.15 and remove the "DOWNLOAD_EXTRACT_TIMESTAMP true" line
cmake_minimum_required(VERSION 3.24)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# Pull in Google Test
#
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.8.1.zip
  DOWNLOAD_EXTRACT_TIMESTAMP true
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

#
# Set up the actual executable
#
add_executable(RahiTuber_Test)

target_include_directories(RahiTuber_Test PRIVATE
    ../RahiTuber
    ../RahiTuber/imgui-sfml
    ${CMAKE_SOURCE_DIR}/Libraries/freetype/include
    ${CMAKE_SOURCE_DIR}/Libraries/imgui
    ${CMAKE_SOURCE_DIR}/Libraries/mongoose
    ${CMAKE_SOURCE_DIR}/Libraries/portaudio/include
    ${CMAKE_SOURCE_DIR}/Libraries/SFML/include
)

if(WIN32)
    target_include_directories(RahiTuber_Test PRIVATE ${CMAKE_SOURCE_DIR}/Libraries/Spout2/SPOUTSDK)
endif()

target_sources(RahiTuber_Test PRIVATE
    pch.cpp
    pch.h
    test.cpp
)

if(MSVC)
    target_compile_definitions(RahiTuber_Test PRIVATE
        -DUNICODE -D_UNICODE
    )

    target_compile_definitions(RahiTuber_Test PRIVATE
        PROGRAM_NAME=L"RahiTuber_Test" # Was $<$<CONFIG:Debug>:PROGRAM_NAME=L"RahiTuber_Test">
        IMGUI_DISABLE_WIN32_FUNCTIONS
        SFML_STATIC
        NOMINMAX
        X64
        _CONSOLE
    )
endif()

target_link_libraries(RahiTuber_Test PRIVATE ${OPENGL_LIBRARY}
    freetype
    mongoose
    imgui
    portaudio_static
    tinyxml2
    sfml-graphics
    sfml-window
    sfml-system
    gtest_main
)

if(WIN32)
    target_link_libraries(RahiTuber_Test PRIVATE Spout_static)
    target_link_libraries(RahiTuber_Test PRIVATE Shcore Xinput hid)
endif()

#
# Include Google Test and do test discovery
#
include(GoogleTest)
gtest_discover_tests(RahiTuber_Test)
