---
description: "Scaffold a new CLI tool binary for the Audio Babel C++ project"
agent: "agent"
argument-hint: "Describe what the CLI tool should do"
---
Create a new CLI tool for the Audio Babel project. Follow the existing pattern exactly:

## Steps

1. **Create the source file** at `cpp/tools/<name>_cli.cpp`:
   - Parse `argc`/`argv` with usage message on wrong args (exit code 2)
   - Wrap all logic in `try { ... } catch (const std::exception& e) { ... }` (exit code 1)
   - Use `AudioBabel::` namespace functions from the core library
   - Include only headers from `cpp/include/`

2. **Add the CMake target** to root `CMakeLists.txt`, following this exact pattern:
   ```cmake
   set(Netname_SRC "${CPP_ROOT}/tools/<name>_cli.cpp")
   if(EXISTS ${netname_SRC})
       add_executable(<name>_cli ${NEWNAME_SRC})
       target_link_libraries(<name>_cli PRIVATE audiolib)
       target_include_directories(<name>_cli PRIVATE ${CPP_ROOT}/include)
   endif()
   ```

3. **Build and verify**: Run `tools/build.ps1 -Configuration Debug` to confirm it compiles.

Reference [extract_index_cli.cpp](../../cpp/tools/extract_index_cli.cpp) and [reconstruct_cli.cpp](../../cpp/tools/reconstruct_cli.cpp) for working examples.
