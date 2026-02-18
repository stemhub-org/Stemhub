# StemHub JUCE Build Bootstrap

This folder only sets up JUCE + CMake so you can write your own plugin code.

## Prerequisites (macOS)

- Xcode Command Line Tools
- CMake 3.22+

## Build

From this directory:

```bash
cmake -S . -B build
cmake --build build --config Debug
```

Without a `CMakeLists.user.cmake`, the build will only run a `stemhub_poc_ready` target (dependency bootstrap only).

If `JUCE/` is missing or incomplete, CMake will fetch JUCE from GitHub automatically.

If you want to force local JUCE:

```bash
cmake -S . -B build -DSTEMHUB_POC_FETCH_JUCE=OFF -DSTEMHUB_POC_JUCE_PATH=/absolute/path/to/JUCE
```

## Add your plugin

Create `CMakeLists.user.cmake` in this folder and define your own `juce_add_plugin(...)` target there.

`stemhub_juce_defaults` is provided as an interface target for JUCE recommended compiler flags:

```cmake
target_link_libraries(YourPluginTarget
    PUBLIC
        stemhub_juce_defaults
)
```
