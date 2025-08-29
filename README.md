# Building on MSYS2 (mingw-w64)

Requirements:

- MSYS2 with mingw-w64 toolchain (mingw64 shell)
- Install packages: mingw-w64-x86_64-gcc, mingw-w64-x86_64-cmake, mingw-w64-x86_64-fftw, mingw-w64-x86_64-gmp (optional)

Example build steps (run in MINGW64 shell):

```powershell
pacman -Syu; pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-fftw mingw-w64-x86_64-gmp
mkdir build; cd build
cmake -G "MinGW Makefiles" ..
mingw32-make
```

The CMake target `example_main` and `tests_runner` will be built if their source files exist under `cpp/examples` and `cpp/tests` respectively.
