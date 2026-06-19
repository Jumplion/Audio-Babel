cmake_minimum_required(VERSION 3.15)
project(SpeakerOfBabel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch Catch2 v3 for modern unit testing
include(FetchContent)
fetchcontent_declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.4 # Latest stable as of Oct 2025
    GIT_SHALLOW TRUE
)
fetchcontent_makeavailable(Catch2)

# Add Catch2 CMake module path for test discovery
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

# Put project sources under cpp/
set(CPP_ROOT "${CMAKE_SOURCE_DIR}/cpp")
include_directories(${CPP_ROOT}/include)

file(GLOB AUDIO_SOURCES
    "${CPP_ROOT}/src/*.cpp"
)

add_library(audiolib STATIC ${AUDIO_SOURCES})
target_include_directories(audiolib PUBLIC ${CPP_ROOT}/include)

# Find Boost.Multiprecision header (header-only)
find_path(BOOST_MP_INCLUDE_DIR
    NAMES boost/multiprecision/cpp_int.hpp
    PATHS
        C:/msys64/mingw64/include
        C:/vcpkg/installed/x64-mingw-dynamic/include
)
if(BOOST_MP_INCLUDE_DIR)
    message(STATUS "Found Boost.Multiprecision headers: ${BOOST_MP_INCLUDE_DIR}")
    target_include_directories(audiolib PUBLIC ${BOOST_MP_INCLUDE_DIR})
else()
    message(WARNING "Boost.Multiprecision header not found. If Boost is installed on another drive (e.g. C:/msys64/mingw64/include), pass -BOOST_MP_INCLUDE_DIR=path/to/boost/include to cmake or install the mingw-w64 boost package.")
endif()

# Example executable
set(EXAMPLE_SRC "${CPP_ROOT}/examples/main.cpp")
if(EXISTS ${EXAMPLE_SRC})
    add_executable(example_main ${EXAMPLE_SRC})
    target_link_libraries(example_main PRIVATE audiolib)
    target_include_directories(example_main PRIVATE ${CPP_ROOT}/include)
endif()

# Catch2 test executable with all unit tests
set(CATCH2_TEST_SOURCES
    "${CPP_ROOT}/tests/test_index.cpp"
    "${CPP_ROOT}/tests/test_base64.cpp"
    "${CPP_ROOT}/tests/test_bijection.cpp"
    "${CPP_ROOT}/tests/test_scramble.cpp"
    "${CPP_ROOT}/tests/test_metadata.cpp"
    "${CPP_ROOT}/tests/test_library_position.cpp"
    "${CPP_ROOT}/tests/test_wav_parsing.cpp"
    "${CPP_ROOT}/tests/test_integration.cpp"
)
add_executable(tests_catch2 ${CATCH2_TEST_SOURCES})
target_link_libraries(tests_catch2 PRIVATE audiolib Catch2::Catch2WithMain)
target_include_directories(tests_catch2 PRIVATE ${CPP_ROOT}/include ${CPP_ROOT}/tests)

# Enable CTest integration for Catch2 tests
include(CTest)
include(Catch)
catch_discover_tests(tests_catch2)

# Performance benchmarks executable (separate from unit tests)
set(PERF_SRC "${CPP_ROOT}/tests/test_performance.cpp")
if(EXISTS ${PERF_SRC})
    add_executable(performance_benchmarks ${PERF_SRC})
    target_link_libraries(performance_benchmarks PRIVATE audiolib)
    target_include_directories(performance_benchmarks PRIVATE ${CPP_ROOT}/include ${CPP_ROOT}/tests)
    # Recommend Release mode for accurate benchmarks
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING "Performance benchmarks should be run in Release mode for accurate results. Use: cmake -DCMAKE_BUILD_TYPE=Release ..")
    endif()
endif()

# Small reconstruction CLI used by the web server
set(RECON_SRC "${CPP_ROOT}/tools/reconstruct_cli.cpp")
if(EXISTS ${RECON_SRC})
    add_executable(reconstruct_cli ${RECON_SRC})
    target_link_libraries(reconstruct_cli PRIVATE audiolib)
    target_include_directories(reconstruct_cli PRIVATE ${CPP_ROOT}/include)
endif()

# Small extract CLI used by the web server (WAV -> index)
set(EXTRACT_SRC "${CPP_ROOT}/tools/extract_index_cli.cpp")
if(EXISTS ${EXTRACT_SRC})
    add_executable(extract_index_cli ${EXTRACT_SRC})
    target_link_libraries(extract_index_cli PRIVATE audiolib)
    target_include_directories(extract_index_cli PRIVATE ${CPP_ROOT}/include)
endif()

message(STATUS "Project configured: ${PROJECT_NAME}")
