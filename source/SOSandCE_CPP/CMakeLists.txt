cmake_minimum_required(VERSION 3.16)
project(sosandce LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
find_package(RmlUi CONFIG REQUIRED)


file(GLOB_RECURSE GAME_SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)


set(IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/include/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/include/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/include/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/include/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/include/imgui/imgui_impl_sdl2.cpp
    ${CMAKE_SOURCE_DIR}/include/imgui/imgui_impl_sdlrenderer2.cpp
)

set(APP_ICON_RESOURCE)
if (WIN32)
    set(APP_ICON_RESOURCE ${CMAKE_SOURCE_DIR}/resources/windows_app.rc)
    add_executable(sosandce WIN32 ${GAME_SOURCES} ${IMGUI_SOURCES} ${APP_ICON_RESOURCE})
else()
    add_executable(sosandce ${GAME_SOURCES} ${IMGUI_SOURCES})
endif()

target_include_directories(sosandce PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/imgui
)

target_link_libraries(sosandce PRIVATE
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
    $<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer>,SDL2_mixer::SDL2_mixer,SDL2_mixer::SDL2_mixer-static>
    $<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf>,SDL2_ttf::SDL2_ttf,SDL2_ttf::SDL2_ttf-static>
    RmlUi::RmlUi
)


add_custom_command(TARGET sosandce POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying UI asset pack to output directory"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/ui_pack"
        "$<TARGET_FILE_DIR:sosandce>/assets/ui_pack"
)
