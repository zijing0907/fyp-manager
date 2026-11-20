cmake_minimum_required(VERSION 3.16)
project(fyp_manager VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(FYP_BUILD_TESTS "Build the unit tests (downloads GoogleTest)" ON)
option(FYP_SANITIZE "Build with AddressSanitizer and UBSan" OFF)

# Everything except main() lives in a library so the tests can link against it.
add_library(fyp_core
  src/cli.cpp
  src/grading_service.cpp
  src/models.cpp
  src/registration_service.cpp
  src/rubric.cpp
  src/serialization.cpp
  src/storage.cpp
  src/style.cpp
  src/terminal.cpp
  src/title_service.cpp
  src/util.cpp
)
target_include_directories(fyp_core PUBLIC include)

add_executable(fyp_manager src/main.cpp)
target_link_libraries(fyp_manager PRIVATE fyp_core)
target_compile_definitions(fyp_manager PRIVATE FYP_VERSION="${PROJECT_VERSION}")

foreach(target fyp_core fyp_manager)
  if(MSVC)
    # /utf-8: sources are UTF-8 (some tests contain non-English names).
    target_compile_options(${target} PRIVATE /W4 /utf-8)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)  # std::getenv is fine here
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
  endif()
  if(FYP_SANITIZE AND NOT MSVC)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endif()
endforeach()

if(FYP_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # Windows: match the runtime library
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  include(GoogleTest)
  add_executable(fyp_tests
    tests/cli_test.cpp
    tests/grading_service_test.cpp
    tests/registration_service_test.cpp
    tests/regression_test.cpp
    tests/serialization_test.cpp
    tests/style_test.cpp
    tests/title_service_test.cpp
    tests/util_test.cpp
  )
  target_include_directories(fyp_tests PRIVATE tests)
  target_compile_definitions(fyp_tests PRIVATE FYP_SAMPLE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/sample_data")
  target_link_libraries(fyp_tests PRIVATE fyp_core GTest::gtest_main)
  if(MSVC)
    target_compile_options(fyp_tests PRIVATE /utf-8)
  endif()
  if(FYP_SANITIZE AND NOT MSVC)
    target_compile_options(fyp_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(fyp_tests PRIVATE -fsanitize=address,undefined)
  endif()
  # A per-test timeout turns any accidental infinite loop into a failure instead of a hung run.
  gtest_discover_tests(fyp_tests PROPERTIES TIMEOUT 30)
endif()
