# 本机构建环境与常见问题

仅在处理构建环境、CMake、Qt 依赖或编译问题时读取。本文中的命令供用户自行执行，不改变 [项目入口](../AGENTS.md) 中不自行构建、运行或测试的约定。

## 工具与 Qt 版本

- Windows 使用 Visual Studio 2022 的 MSVC v143 x64 和 Windows SDK。
- 使用官方独立版 CMake：`D:\software\cmake-3.31.6\bin\cmake.exe`。
- **不要使用 VS BuildTools 自带的 CMake**，即 `...\Common7\IDE\CommonExtensions\Microsoft\CMake\...`。本机该版本执行 `cmake -E cmake_autogen` 会在 AUTOMOC 阶段崩溃，表现为 MSBuild 报 `系统找不到指定的批处理标签 - VCEnd` / MSB8066。
- 正式程序使用 Qt 6.8+，依赖 Core、Gui、Network、Widgets、SVG、QML、Quick、Quick Widgets、Quick Controls 2 和 Shader Tools。正式程序与预览当前共同使用 Qt 6.8.3。
- 指定实际 Qt 安装目录，静态、动态和预览构建使用各自目录，不复用其他 Qt kit 的构建缓存。

| 用途 | Qt 安装目录 | 项目构建目录 | 输出 |
| --- | --- | --- | --- |
| 动态版，供用户本地确认 | `D:\Qt\6.8.3\msvc2022_64` | `build-qt6-dyn` | `build-qt6-dyn\Release` |
| 静态版，应用发布 | `D:\Qt\6.8.3-static` | `build-qt6` | `build-qt6\Release` |
| 共用界面预览 | 与正式程序相同 Qt 版本 | 由预览脚本维护 | 见 [预览说明](ui-preview.md) |

## Path / PATH 重复

如果 MSBuild 报 `System.ArgumentException: 已添加项。字典中的关键字:“Path”所添加的关键字:“PATH”`，说明进程环境同时存在两个大小写不同的条目。构建前在当前 PowerShell 进程清理重复项，只保留一个 `Path`；不要修改机器或用户的持久环境变量。

用户自行进行动态构建时可使用：

```powershell
[System.Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[System.Environment]::SetEnvironmentVariable('Path', 'C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0', 'Process')
& 'D:\software\cmake-3.31.6\bin\cmake.exe' -S . -B build-qt6-dyn -G 'Visual Studio 17 2022' -A x64 -DCMAKE_PREFIX_PATH='D:/Qt/6.8.3/msvc2022_64'
& 'D:\software\cmake-3.31.6\bin\cmake.exe' --build build-qt6-dyn --config Release --target XiaoJiAutoKey
```

动态版产物需要 Qt DLL 和插件，用户启动前使用对应 kit 的部署工具：

```powershell
& 'D:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe' --release --qmldir src\qml build-qt6-dyn\Release\XiaoJiAutoKey.exe
```

## 静态 Qt

重建 Qt 时优先使用项目脚本；完整选项、磁盘空间和中间目录说明见 [Qt 6 静态构建](build-qt6-static.md)。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-qt-build.ps1 -QtVersion 6.8.3 -QtRoot D:\Qt -CMakePath D:\software\cmake-3.31.6\bin\cmake.exe -RemoveIntermediates
```

应用包内容和版本发行见 [应用打包、发布与更新](release-process.md)；仅修改插件时，按需读取 [插件开发](plugin-development.md) 和 [插件发布](plugin-release.md)。不要为修改文档或目录命名自行触发这些命令。
