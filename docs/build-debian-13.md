# Build on Debian 13

## Requirements

- Debian 13.
- CMake 3.24 or newer.
- A C++20 compiler.
- Qt 6.11 with Core, Gui, Widgets, and Test modules.

## Configure, Build, and Test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

For headless test environments, the CTest targets set `QT_QPA_PLATFORM=offscreen` where GUI tests need it.

## Notes

- The app stores settings in the per-user Qt `AppConfigLocation` as `settings.json`.
- Delete operations use Qt's platform trash support and do not fall back to permanent delete.
