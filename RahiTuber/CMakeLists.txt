# Created by and for Qt Creator This file was created for editing the project sources only.
# You may attempt to use it for building too, by modifying this file here.

cmake_minimum_required(VERSION 3.5)
project(RahiTuber)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    .
    mongoose-7.16
    imgui
    imgui/backends
    imgui/freetype
    imgui/freetype/config
    imgui/misc/cpp
    imgui/misc/freetype
    imgui/misc/single_file
    imgui-sfml
    imgui-sfml/GL
    imgui-sfml/KHR
    portaudio/bindings/java/c/src
    portaudio/include
    portaudio/pablio
    portaudio/qa/loopback/src
    portaudio/src/common
    portaudio/src/hostapi/asio
    portaudio/src/hostapi/coreaudio
    portaudio/src/hostapi/dsound
    portaudio/src/hostapi/wasapi/mingw-include
    portaudio/src/os/unix
    portaudio/src/os/win
    ${CMAKE_SOURCE_DIR}/../../SFML-2.6.1/include/

    /usr/include
    .
)

set (SRCS
    imgui/backends/imgui_impl_opengl3.cpp
    imgui/backends/imgui_impl_opengl3.h
    imgui/backends/imgui_impl_opengl3_loader.h
    imgui/freetype/freetype.h
    imgui/misc/freetype/imgui_freetype.cpp
    imgui/misc/freetype/imgui_freetype.h
    imgui/misc/single_file/imgui_single_file.h
    imgui/ft2build.h
    imgui/imconfig.h
    imgui/imgui.cpp
    imgui/imgui.h
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_internal.h
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imstb_rectpack.h
    imgui/imstb_textedit.h
    imgui/imstb_truetype.h
    imgui-sfml/GL/glext.h
    imgui-sfml/KHR/khrplatform.h
    imgui-sfml/imconfig-SFML.h
    imgui-sfml/imgui-SFML.cpp
    imgui-sfml/imgui-SFML.h
    imgui-sfml/imgui-SFML_export.h
    portaudio/include/pa_asio.h
    portaudio/include/pa_jack.h
    portaudio/include/pa_linux_alsa.h
    portaudio/include/pa_mac_core.h
    portaudio/include/pa_win_ds.h
    portaudio/include/pa_win_wasapi.h
    portaudio/include/pa_win_waveformat.h
    portaudio/include/pa_win_wdmks.h
    portaudio/include/pa_win_wmme.h
    portaudio/include/portaudio.h
    tinyxml2/tinyxml2.cpp
    tinyxml2/tinyxml2.h
    mongoose-7.16/mongoose.c
    mongoose-7.16/mongoose.h
    Config.h
    defines.h
    file_browser_modal.cpp
    file_browser_modal.h
    LayerManager.cpp
    LayerManager.h
    main.cpp
    resource.h
    SpriteSheet.cpp
    SpriteSheet.h
    TextureManager.cpp
    TextureManager.h
    xmlConfig.cpp
    xmlConfig.h
    websocket.h
)

#add_compile_definitions( SFML_STATIC )

add_compile_options( -Wl,-rpath./lib/ )

add_executable(${CMAKE_PROJECT_NAME} ${SRCS})

target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/portaudio/build/Desktop_GCC-RelWithDebInfo/
    ${CMAKE_SOURCE_DIR}/../../SFML-2.6.1/lib/
    /usr/local/lib/ )

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE X11 GL freetype asound
    portaudio
    sfml-graphics
    sfml-window
    sfml-system
     )

 cmake_policy(SET CMP0095 OLD)
 set_target_properties(${CMAKE_PROJECT_NAME}
     PROPERTIES
         INSTALL_RPATH "\\\$ORIGIN/lib"
 )

