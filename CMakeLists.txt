cmake_minimum_required(VERSION 3.25)
project(LastStand VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CPM.cmake)

if(EMSCRIPTEN)
    set(PLATFORM Web CACHE STRING "" FORCE)
endif()

CPMAddPackage(
    NAME raylib
    GITHUB_REPOSITORY raysan5/raylib
    GIT_TAG 6.2
    OPTIONS "BUILD_EXAMPLES OFF" "BUILD_GAMES OFF"
)

CPMAddPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    VERSION 3.12.0
)

file(GLOB_RECURSE SOURCES
    src/*.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    raylib
    nlohmann_json::nlohmann_json
)

if(EMSCRIPTEN)
    # Only what the game loads ships to the browser; the packs hold far more than that.
    set(WEB_ASSETS
        assets/fonts/LilitaOne-Regular.ttf
        assets/packs/ninja-adventure/audio/music/theme_dream.ogg
        assets/packs/ninja-adventure/audio/music/theme_plain.ogg
        assets/packs/ninja-adventure/audio/music/theme_swamp.ogg
        assets/packs/ninja-adventure/audio/music/theme_lost_village.ogg
        "assets/packs/kenney-tower-defense-top-down/PNG/Default size/towerDefense_tile130.png"
        "assets/packs/kenney-tower-defense-top-down/PNG/Default size/towerDefense_tile131.png"
        "assets/packs/kenney-tower-defense-top-down/PNG/Default size/towerDefense_tile271.png"
        "assets/packs/kenney-tower-defense-top-down/PNG/Default size/towerDefense_tile294.png"
        "assets/packs/kenney-particle-pack/PNG (Transparent)/smoke_04.png"
        "assets/packs/kenney-particle-pack/PNG (Transparent)/star_06.png"
        "assets/packs/kenney-particle-pack/PNG (Transparent)/scorch_02.png"
    )
    set(WEB_PRELOADS "")
    foreach(asset ${WEB_ASSETS})
        list(APPEND WEB_PRELOADS "SHELL:--preload-file \"${CMAKE_SOURCE_DIR}/${asset}@${asset}\"")
    endforeach()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic -fexperimental-library
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
    target_link_options(${PROJECT_NAME} PRIVATE
        "SHELL:-s USE_GLFW=3"
        "SHELL:-s WASM=1"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s ASYNCIFY"
        ${WEB_PRELOADS}
        "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/web/shell.html"
    )
else()
    if(UNIX AND NOT APPLE)
        target_link_libraries(${PROJECT_NAME} PRIVATE m pthread dl)
    endif()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
    )
    # Copy assets to build directory
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    )
endif()

option(BUILD_TESTING "Build unit tests" OFF)
if(BUILD_TESTING AND NOT EMSCRIPTEN)
    enable_testing()
    add_subdirectory(tests)
endif()
