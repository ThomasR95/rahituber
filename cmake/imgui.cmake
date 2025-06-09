add_library(imgui STATIC
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/backends/imgui_impl_opengl3.cpp
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/backends/imgui_impl_opengl3.h
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/backends/imgui_impl_opengl3_loader.h
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/misc/freetype/imgui_freetype.cpp
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/misc/freetype/imgui_freetype.h
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/misc/single_file/imgui_single_file.h
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/imconfig.h
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/imgui.h
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/imgui_internal.h
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/imstb_rectpack.h
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/imstb_textedit.h
    ${CMAKE_SOURCE_DIR}/Libraries/imgui/imstb_truetype.h
)

target_include_directories(imgui PRIVATE
    ${CMAKE_SOURCE_DIR}/Libraries/imgui
    ${CMAKE_SOURCE_DIR}/Libraries/freetype
)

target_link_libraries(imgui PRIVATE freetype ${OPENGL_LIBRARY})