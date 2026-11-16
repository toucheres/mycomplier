# Building

## CMake (recommended)

Requirements: a C++23 compiler, CMake >= 3.20, the `antlr4` command, the
ANTLR4 C++ runtime (headers + library), and Boost.ProgramOptions.  `magic_enum`
is vendored under `third_party/magic_enum/`, so configuration works offline.

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake -j
ctest --test-dir build-cmake --output-on-failure

./build-cmake/mycomplier test/hello.c --print-ret-value   # ret: 52
```

The CMake build generates the parser into `build-cmake/generated/` with the
ANTLR version found on `PATH`, so the checked-in `generated/` directory is not
modified.  If the runtime is installed without a CMake package config, pass
`-DANTLR4_INCLUDE_DIR=... -DANTLR4_LIBRARY=...`.

## VS Code / CMake Tools

The workspace settings point CMake Tools at `build-cmake/` with the Ninja
generator.  Do not configure CMake into `build/`: that directory belongs to
xmake, and mixing a Makefiles cache there with Ninja is what causes the
"generator does not match" error.  After changing `CMakeLists.txt`, run
"CMake: Delete Cache and Reconfigure" once if the extension still reports a
stale generator.

## xmake

```sh
xmake f -c -m release
xmake -y
./build/linux/x86_64/release/mycomplier test/hello.c --print-ret-value
```

The xmake build uses the vendored `antlr4.13.2/antlr-4.13.2-complete.jar` and
the `antlr4-runtime 4.13.2` xmake package.  It regenerates the checked-in
`generated/` directory before compiling; the generated files are also kept in
the repository so an offline build can start from them.

## Runtime root

Both build systems compile the source root into the binary as
`MYCOMPLIER_ROOT`, which is where `stdhead/` and `stdlibc/` are looked up.
This makes the compiler independent of the current working directory.  Set the
`MYCOMPLIER_ROOT` environment variable to override it (for example when moving
the binary to another directory).

## Tests

`ctest` runs `test/hello.c` and checks for `ret: 52`.  `test/test1.c` is the
larger C4-style stress test; it is intentionally not part of `ctest` because it
is slow and the guest program currently still crashes:

```sh
./build-cmake/mycomplier test/test1.c --main-args test/test1.c
```
