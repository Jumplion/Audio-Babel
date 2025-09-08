cmake_minimum_required(VERSION 3.15)
project(SpeakerOfBabel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Put project sources under cpp/
set(CPP_ROOT "${CMAKE_SOURCE_DIR}/cpp")
include_directories(${CPP_ROOT}/include)

file(GLOB AUDIO_SOURCES
    "${CPP_ROOT}/src/*.cpp"
)

add_library(audiolib STATIC ${AUDIO_SOURCES})
target_include_directories(audiolib PUBLIC ${CPP_ROOT}/include)

# Find Boost.Multiprecision header (header-only)
find_path(DBOOST_MP_INCLUDE_DIR
    NAMES boost/multiprecision/cpp_int.hpp
    PATHS C:/vcpkg/installed/x64-mingw-dynamic/include
)
if(DBOOST_MP_INCLUDE_DIR)
    message(STATUS "Found Boost.Multiprecision headers: ${DBOOST_MP_INCLUDE_DIR}")
    target_include_directories(audiolib PUBLIC ${DBOOST_MP_INCLUDE_DIR})
else()
    message(WARNING "Boost.Multiprecision header not found. If Boost is installed on another drive (e.g. C:/msys64/mingw64/include), pass -DBOOST_MP_INCLUDE_DIR=path/to/boost/include to cmake or install the mingw-w64 boost package.")
endif()

# Example executable
set(EXAMPLE_SRC "${CPP_ROOT}/examples/main.cpp")
if(EXISTS ${EXAMPLE_SRC})
    add_executable(example_main ${EXAMPLE_SRC})
    target_link_libraries(example_main PRIVATE audiolib)
    target_include_directories(example_main PRIVATE ${CPP_ROOT}/include)
endif()

# Tests executable (simple runner)
set(TEST_SRC "${CPP_ROOT}/tests/test_main.cpp")
if(EXISTS ${TEST_SRC})
    add_executable(tests_runner ${TEST_SRC})
    target_link_libraries(tests_runner PRIVATE audiolib)
    target_include_directories(tests_runner PRIVATE ${CPP_ROOT}/include)
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
