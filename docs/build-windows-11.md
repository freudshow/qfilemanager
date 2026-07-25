# Build on Windows 11

## Requirements

- Windows 11.
- CMake 3.24 or newer.
- Qt 6.11 with Core, Gui, Widgets, and Test modules.
- MSVC Build Tools or Visual Studio with the Desktop C++ workload.

## Visual Studio Generator

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Ninja Generator

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Notes

- Ensure the Qt 6.11 `bin` and CMake package paths are visible to CMake, usually by using a Qt-enabled developer shell or setting `CMAKE_PREFIX_PATH`.
- The app stores settings under the per-user Qt `AppConfigLocation`, typically below `%APPDATA%`.
- Delete operations should use the Windows recycle-bin behavior exposed through Qt/platform services and must not silently permanently delete files after trash failure.
