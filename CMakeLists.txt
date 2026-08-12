cmake_minimum_required(VERSION 3.20)
project(compiler CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- compiler_core: everything except the CLI entry point ---
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*src/driver/main\\.cpp$")

add_library(compiler_core STATIC ${CORE_SOURCES})
target_include_directories(compiler_core PUBLIC src)

# --- CLI driver ---
add_executable(compiler src/driver/main.cpp)
target_link_libraries(compiler PRIVATE compiler_core)

# --- tests ---
enable_testing()
include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
)
# doctest's own CMakeLists.txt predates CMake 4's minimum policy floor.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(doctest)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS tests/unit/*.cpp)
add_executable(compiler_tests ${TEST_SOURCES})
target_link_libraries(compiler_tests PRIVATE compiler_core doctest::doctest)
target_include_directories(compiler_tests PRIVATE src)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(compiler_tests)
