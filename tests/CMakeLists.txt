CPMAddPackage(
    NAME Catch2
    GITHUB_REPOSITORY catchorg/Catch2
    VERSION 3.16.1
)

file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)

add_executable(LastStandTests ${TEST_SOURCES})

# tests/ holds a raylib.h stub with the plain structs the game logic uses, so the pure
# headers compile and run without a window or GL context.
target_include_directories(LastStandTests BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(LastStandTests PRIVATE
    Catch2::Catch2WithMain
    nlohmann_json::nlohmann_json
)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(LastStandTests)
