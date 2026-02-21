cmake_minimum_required(VERSION 3.25)
project(LastStand VERSION 0.1.0 LANGUAGES CXX)

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
    GIT_TAG 5.5
    OPTIONS "BUILD_EXAMPLES OFF" "BUILD_GAMES OFF"
)

CPMAddPackage(
    NAME EnTT
    GITHUB_REPOSITORY skypjack/entt
    VERSION 3.14.0
)

CPMAddPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    VERSION 3.11.3
)

file(GLOB_RECURSE SOURCES
    src/*.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    raylib
    EnTT::EnTT
    nlohmann_json::nlohmann_json
)

if(EMSCRIPTEN)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic -fexperimental-library
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
    target_link_options(${PROJECT_NAME} PRIVATE
        "SHELL:-s USE_GLFW=3"
        "SHELL:-s WASM=1"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s ASYNCIFY"
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/assets@assets"
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
