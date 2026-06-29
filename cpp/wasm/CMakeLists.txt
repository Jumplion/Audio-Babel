# WebAssembly build configuration for Index library
cmake_minimum_required(VERSION 3.10)
project(IndexWasm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Emscripten-specific compiler flags
if(EMSCRIPTEN)
    # Core WASM settings. The app talks to the module exclusively through embind
    # (Module.encodeIndex(...) etc.), so no ccall/cwrap/HEAP runtime methods are
    # exported — that list was dead glue.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sWASM=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sALLOW_MEMORY_GROWTH=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sMODULARIZE=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sEXPORT_ES6=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sEXPORT_NAME=IndexModule")

    # Browser-only module: drop the Node/shell/worker loader glue and the virtual
    # filesystem. No file I/O reaches the WASM (FileIO is native-only and is not
    # compiled in below), so FILESYSTEM=0 is safe and trims the loader.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sENVIRONMENT=web")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sFILESYSTEM=0")

    # Memory settings for large audio indexes
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sINITIAL_MEMORY=67108864") # 64MB
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sMAXIMUM_MEMORY=2147483648") # 2GB

    # Native WebAssembly exceptions: smaller and faster than the legacy JS-based
    # exception emulation (-sDISABLE_EXCEPTION_CATCHING=0). Must be set at both
    # compile and link time, so it lives in CMAKE_CXX_FLAGS.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fwasm-exceptions")

    # Link-time optimization across the whole module (applies at compile + link).
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")

    # Optimization for production
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
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

# Source files. FileIO (WAV read/write via std::fstream) is intentionally NOT
# compiled into the WASM module: the browser passes raw PCM bytes across the
# embind boundary, so no file I/O happens here. Leaving it out keeps FILESYSTEM=0
# valid and shrinks the binary.
set(INDEX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/Index.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/IndexMetadata.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/IndexNaming.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/LibraryPosition.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/IndexScramble.cpp
)

# WASM bindings
set(WASM_BINDINGS
    ${CMAKE_CURRENT_SOURCE_DIR}/wasm_bindings.cpp
)

# Create WASM library
add_executable(index ${INDEX_SOURCES} ${WASM_BINDINGS})

# Bake in the reversible index scramble for the shipped web app. Without this the
# bijective base-65536 index leaves short indices (every browseable library
# position) decoding to one or two samples of near-silence; the scramble spreads
# them across a wide range of varied, much longer clips. See IndexScramble.h.
target_compile_definitions(index PRIVATE AUDIOBABEL_SCRAMBLE)

# Link with embind
if(EMSCRIPTEN)
    set_target_properties(index PROPERTIES
        LINK_FLAGS "--bind"
    )
endif()

# No additional target includes needed - already set globally above

# Build to local directory first (avoid path issues)
set_target_properties(index PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    OUTPUT_NAME "index"
)

# Define output directory for copying
set(DOCS_WASM_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/wasm")

# Copy .wasm and .js files to output directory after build
if(EMSCRIPTEN)
    # Create output directory if it doesn't exist
    file(MAKE_DIRECTORY "${DOCS_WASM_OUTPUT_DIR}")

    # Copy files to docs folder after build
    add_custom_command(TARGET index POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/index.wasm"
        "${DOCS_WASM_OUTPUT_DIR}/index.wasm"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/index.js"
        "${DOCS_WASM_OUTPUT_DIR}/index.js"
        COMMENT "Copying WASM files to ${DOCS_WASM_OUTPUT_DIR}"
        VERBATIM
    )
endif()
