# WebAssembly build configuration for Audio Index library
cmake_minimum_required(VERSION 3.10)
project(AudioIndexWASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Emscripten-specific compiler flags
if(EMSCRIPTEN)
    # Core WASM settings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sWASM=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sALLOW_MEMORY_GROWTH=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,getValue,setValue")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sMODULARIZE=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sEXPORT_NAME=AudioIndexModule")

    # Memory settings for large audio indexes
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sINITIAL_MEMORY=67108864") # 64MB
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sMAXIMUM_MEMORY=2147483648") # 2GB

    # Exception support (needed for C++ error handling)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sDISABLE_EXCEPTION_CATCHING=0")

    # Optimization for production
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

    # Export malloc and free for memory management
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sEXPORTED_FUNCTIONS=_malloc,_free")
endif()

# Include directories - use parent directory paths
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Try to find Boost or use local copy
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/boost")
    set(BOOST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/boost")
    message(STATUS "Using local Boost: ${BOOST_ROOT}")
    include_directories(${BOOST_ROOT})
else()
    message(STATUS "Boost not found locally. Using Emscripten's system includes (Boost should be installed via emsdk ports).")
    message(STATUS "If build fails, install Boost with: embuilder build boost_headers")
endif()

# Source files
set(AUDIO_INDEX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AudioIndex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/IndexMetadata.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/FileWriters.cpp
)

# WASM bindings
set(WASM_BINDINGS
    ${CMAKE_CURRENT_SOURCE_DIR}/wasm_bindings.cpp
)

# Create WASM library
add_executable(audio-index ${AUDIO_INDEX_SOURCES} ${WASM_BINDINGS})

# Link with embind
if(EMSCRIPTEN)
    set_target_properties(audio-index PROPERTIES
        LINK_FLAGS "--bind"
    )
endif()

# No additional target includes needed - already set globally above

# Build to local directory first (avoid path issues)
set_target_properties(audio-index PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    OUTPUT_NAME "audio-index"
)

# Define output directory for copying
set(WASM_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../audio-babel-record-store/public/wasm")

# Copy .wasm and .js files to output directory after build
if(EMSCRIPTEN)
    # Create output directory if it doesn't exist
    file(MAKE_DIRECTORY "${WASM_OUTPUT_DIR}")

    # Copy files after build
    add_custom_command(TARGET audio-index POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/audio-index.wasm"
        "${WASM_OUTPUT_DIR}/audio-index.wasm"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/audio-index.js"
        "${WASM_OUTPUT_DIR}/audio-index.js"
        COMMENT "Copying WASM files to ${WASM_OUTPUT_DIR}"
        VERBATIM
    )
endif()
