set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(RahiTuber)

target_include_directories(RahiTuber PRIVATE
    ./
    ./imgui-sfml
    ${CMAKE_SOURCE_DIR}/Libraries/freetype/include
    ${CMAKE_SOURCE_DIR}/Libraries/imgui
    ${CMAKE_SOURCE_DIR}/Libraries/mongoose
    ${CMAKE_SOURCE_DIR}/Libraries/portaudio/include
    ${CMAKE_SOURCE_DIR}/Libraries/SFML/include
    ${CMAKE_SOURCE_DIR}/Libraries/tinyxml2
)

if(WIN32)
    target_include_directories(RahiTuber PRIVATE ${CMAKE_SOURCE_DIR}/Libraries/Spout2/SPOUTSDK)
endif()



target_sources(RahiTuber PRIVATE
    imgui-sfml/GL/glext.h
    imgui-sfml/KHR/khrplatform.h
    imgui-sfml/imconfig-SFML.h
    imgui-sfml/imgui-SFML.cpp
    imgui-sfml/imgui-SFML.h
    imgui-sfml/imgui-SFML_export.h
    Config.h
    defines.h
    file_browser_modal.cpp
    file_browser_modal.h
    LayerManager.cpp
    LayerManager.h
    main.cpp
    SpriteSheet.cpp
    SpriteSheet.h
    TextureManager.cpp
    TextureManager.h
    xmlConfig.cpp
    xmlConfig.h
    websocket.h
    MainEngine.h
)

if(WIN32)
    target_sources(RahiTuber PRIVATE
        resource.h
        RahiTuber.rc
    )
endif()

if(NOT MSVC)
    target_compile_options(RahiTuber PRIVATE -Wl,-rpath./lib/)
endif()

if(MSVC)
    target_compile_definitions(RahiTuber PRIVATE
        -DUNICODE -D_UNICODE
    )
	
	set_target_properties( RahiTuber PROPERTIES OUTPUT_NAME "$(ProjectName)_64")

    target_compile_definitions(RahiTuber PRIVATE
        IMGUI_DISABLE_WIN32_FUNCTIONS
        SFML_STATIC
        NOMINMAX
        _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
    )
endif()

target_link_libraries(RahiTuber PRIVATE ${OPENGL_LIBRARY}
    freetype
    mongoose
    imgui
    portaudio_static
    tinyxml2
    sfml-graphics
    sfml-window
    sfml-system
)

if(WIN32)
    target_link_libraries(RahiTuber PRIVATE Spout_static)
    target_link_libraries(RahiTuber PRIVATE Shcore Xinput hid)
	
	set(sourceResDir ${CMAKE_CURRENT_LIST_DIR}/res)
	cmake_path(NATIVE_PATH sourceResDir sourceResDirWin)
		
	add_custom_command(
		TARGET RahiTuber POST_BUILD
		COMMENT "Adding symlink for resource directory"
		COMMAND IF EXIST \"$<SHELL_PATH:$<TARGET_FILE_DIR:RahiTuber>/res/>\" (ECHO ...already exists)  ELSE (mklink /J \"$<SHELL_PATH:$<TARGET_FILE_DIR:RahiTuber>/res>\" \"${sourceResDirWin}\" )
		USES_TERMINAL
		)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(X11_FOUND)
        target_link_libraries(RahiTuber PRIVATE ${X11_LIBRARIES})
    endif()

     if(ALSA_FOUND)
        target_link_libraries(RahiTuber PRIVATE ${ALSA_LIBRARIES})
     endif()
 endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    cmake_policy(SET CMP0095 OLD)
    set_target_properties(RahiTuber
        PROPERTIES
            INSTALL_RPATH "\\\$ORIGIN/lib"
    )
endif()

