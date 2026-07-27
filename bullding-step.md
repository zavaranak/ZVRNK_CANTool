# Building Step

This project is a Qt 6 Widgets / CMake / C++20 application.

The repository intentionally ignores build output. Use a separate `build` directory.

## 1. Install Prerequisites

Required:

- Qt 6 with the Widgets module
- CMake 3.24+
- A matching Windows C++ compiler
- x64 `ControlCAN.dll` from the USB-CAN vendor package

Qt is not currently available on this machine through `qmake` on `PATH`, so configure from a Qt Developer Command Prompt or pass `CMAKE_PREFIX_PATH` manually.

Verified Qt path on this machine:

```powershell
C:\Qt\QtOnlineInstall\6.8.3\msvc2022_64
```

## 2. Configure

From the project root:

```powershell
cmake -S . -B build -G "Ninja" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvcXXXX_64" `
  -DCONTROLCAN_DLL_PATH="D:\path\to\ControlCAN.dll"
```

Adjust the Qt path and DLL path for your installation.

If using Visual Studio generator:

```powershell
cmake -S . -B build `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvcXXXX_64" `
  -DCONTROLCAN_DLL_PATH="D:\path\to\ControlCAN.dll"
```

## 3. Build

```powershell
cmake --build build --config Release
```

The build copies `ControlCAN.dll` beside the executable when `CONTROLCAN_DLL_PATH` is set.

## 4. Run

For Ninja single-config builds:

```powershell
.\build\ZVRNK_CANTool.exe
```

For Visual Studio multi-config builds:

```powershell
.\build\Release\ZVRNK_CANTool.exe
```

If the app cannot load `ControlCAN.dll`, either:

- copy `ControlCAN.dll` beside the executable, or
- set the runtime environment variable `CONTROLCAN_DLL_PATH` to the DLL path.

## 5. Git Norms

The repository was initialized with Git.

Build artifacts are ignored in `.gitignore`, including:

- `build/`
- `build-*/`
- `cmake-build-*/`
- Qt autogen folders
- generated binaries and debug symbols

Suggested first commit after local review:

```powershell
git add .
git commit -m "Initial Qt CAN tool scaffold"
```

## 6. Verification Notes

Local verification depends on a configured Qt kit. CMake is installed, but Qt tools are not on `PATH` in the current shell.

Verified configure command:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\QtOnlineInstall\6.8.3\msvc2022_64"
```

Verified build command:

```powershell
cmake --build build --config Debug
```

Hardware verification requires the USB-CAN adapter and vendor driver/DLL.
