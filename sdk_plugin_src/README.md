# NoHoldBodyDump source

This is the source of the game-tested companion DLL in `../sdk_plugin`. It remaps the non-VR dragging state's native DumpBody input and matching prompt bindings. The SMF entity patches remain separate.

Build prerequisites: Visual Studio 2022 C++ Build Tools, a Windows SDK, CMake, and the official [ZHMModSDK 4.1.1 developer package](https://github.com/OrfeasZ/ZHMModSDK/releases/tag/v4.1.1). Extract the developer package outside the mod repository and pass its directory as `SDK_DIR`.

From the mod repository, for example in this workspace:

```powershell
cmake -S sdk_plugin_src -B ../../agentTools/NoHoldButtons/repo-sdk-build -G "Visual Studio 17 2022" -A x64 -DSDK_DIR="X:/Games/HITMAN3_3.280/Simple Mod Framework/agentTools/NoHoldButtons/sdk-dev-4.1.1"
cmake --build ../../agentTools/NoHoldButtons/repo-sdk-build --config Release
```

The DLL is written to the chosen build directory's `bin/NoHoldBodyDump.dll`; copy it to `../sdk_plugin` only when ready to replace the packaged binary. SDK headers and libraries are external dependencies, not vendored here. The target uses C++23, `/MT`, and UTF-8, matching the developer package's release libraries. `compat/directx/d3d12.h` forwards the developer package's include to the Windows SDK header.

The implementation applies four guarded byte changes in SDK `Init`, before engine startup registers prompt bindings. It checks the exact supported executable hash and original instruction bytes, and restores its own changes on unload. Keep input-update and prompt-binding indices consistent when supporting another executable. Changing offsets or passing offline checks alone does not establish in-game compatibility.

SDK ABI: 1. Exports: `GetPluginInterface`, `CompiledSdkVersion`, and `CompiledSdkAbiVersion`. Runtime dependencies: ZHMModSDK.dll, bcrypt.dll, and KERNEL32.dll. See `../sdk_plugin/README.md` for installation, settings, and supported-build details, and `../sdk_plugin/licenses` for third-party notices.

Research, validators, SDK dependencies, and scratch builds remain under the workspace's `agentTools/NoHoldButtons`. This source directory is tracked in Git but excluded from the release ZIP and GitHub source archives.
