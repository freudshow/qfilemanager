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

## GUI Executable and Icon

On Windows, `filemanager_app.exe` is built as a GUI-subsystem executable. Launching it opens only the file-manager window; it does not open a separate command-prompt window. The executable embeds the Qt File Manager icon for Explorer, the taskbar, Alt+Tab, and pinned shortcuts.

## Windows Manual Verification

1. Configure and build from a Qt-enabled Developer PowerShell:

   ```powershell
   cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64
   cmake --build build-win --config Release
   ctest --test-dir build-win -C Release --output-on-failure
   ```

2. Confirm the executable uses the Windows GUI subsystem:

   ```powershell
   dumpbin /headers build-win\Release\filemanager_app.exe | Select-String "subsystem"
   ```

   The output must identify the Windows GUI subsystem, not the Windows CUI subsystem.

3. Start `build-win\Release\filemanager_app.exe` from Explorer. Confirm no Command Prompt window appears, and confirm the custom folder icon is visible on the taskbar and in Alt+Tab. If an older pinned shortcut shows a cached generic icon, unpin it, close the application, reopen the executable, and pin it again.

4. Open a directory in the file manager, create or delete a file with a separate program, and confirm the active tab updates automatically. Click `Refresh` beside the Up button and confirm the same directory reloads without changing Back/Forward history.
