CPMAddPackage(
    NAME Catch2
    GITHUB_REPOSITORY catchorg/Catch2
    VERSION 3.7.1
)

file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)

add_executable(LastStandTests ${TEST_SOURCES})

# tests/ dir contains raylib.h stub — must come before other includes
target_include_directories(LastStandTests BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(LastStandTests PRIVATE
    Catch2::Catch2WithMain
    EnTT::EnTT
    nlohmann_json::nlohmann_json
)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(LastStandTests)
