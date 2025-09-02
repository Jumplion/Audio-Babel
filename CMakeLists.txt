cmake_minimum_required(VERSION 3.15)
project(SpeakerOfBabel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_FFTW "Link against FFTW3 library if available" ON)
option(ENABLE_GMP "Link against GMP library if available" ON)

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
    /mingw64/include
    /usr/include
    /usr/local/include
    C:/msys64/mingw64/include
    C:/msys64/usr/include
    C:/vcpkg/installed/x64-windows/include
    C:/vcpkg
    C:/Program Files/boost/include
    C:/Program Files (x86) /boost/include
)
if(BOOST_MP_INCLUDE_DIR)
    message(STATUS "Found Boost.Multiprecision headers: ${BOOST_MP_INCLUDE_DIR}")
    target_include_directories(audiolib PUBLIC ${BOOST_MP_INCLUDE_DIR})
else()
    message(WARNING "Boost.Multiprecision header not found. If Boost is installed on another drive (e.g. C:/msys64/mingw64/include), pass -DBOOST_MP_INCLUDE_DIR=path/to/boost/include to cmake or install the mingw-w64 boost package.")
endif()

# Find FFTW3 (simple find by library name)
if(ENABLE_FFTW)
    find_library(FFTW3_LIB NAMES fftw3 PATHS /mingw64/lib /usr/lib /usr/local/lib NO_DEFAULT_PATH)
    if(FFTW3_LIB)
        message(STATUS "Found FFTW3: ${FFTW3_LIB}")
        target_link_libraries(audiolib PUBLIC ${FFTW3_LIB})
    else()
        message(STATUS "FFTW3 not found; continuing without it")
    endif()
endif()

# Find GMP (optional)
if(ENABLE_GMP)
    find_library(GMP_LIB NAMES gmp mpir PATHS /mingw64/lib /usr/lib /usr/local/lib NO_DEFAULT_PATH)
    if(GMP_LIB)
        message(STATUS "Found GMP: ${GMP_LIB}")
        target_link_libraries(audiolib PUBLIC ${GMP_LIB})
    else()
        message(STATUS "GMP not found; continuing without it")
    endif()
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
