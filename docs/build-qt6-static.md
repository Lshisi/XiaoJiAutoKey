# Qt 6 静态构建规范

本文记录当前机器的 Qt 6 静态版构建、项目编译和发布方式。旧 Qt 5 的 `run-qt-build.ps1` 入口保留同名习惯；Qt 6 现在也是一条命令完成下载源码、初始化 MSVC 环境、下载 Ninja、编译和安装。

仅在重建静态 Qt 或处理其工具链、中间目录时按需读取。本机 kit 与动态版说明见 [构建环境](build-environment.md)，应用包约束见 [发布流程](release-process.md)。以下命令供用户自行执行，全局工作约束见 [项目入口](../AGENTS.md)。

## 当前约定

| 用途 | 路径 |
| --- | --- |
| Qt 静态安装目录（保留） | `D:\Qt\6.8.3-static` |
| Qt 源码压缩包（可清理） | `D:\Qt\qt-everywhere-src-6.8.3.zip` |
| Qt 源码目录（可清理） | `D:\Qt\qt-everywhere-src-6.8.3` |
| Qt 中间构建目录（可清理） | `D:\Qt\qt-6.8.3-static-build` |
| 项目构建目录（保留） | `build-qt6` |
| 项目发布目录 | `dist\XiaoJiAutoKey` |

Qt 静态安装目录需要长期保留；源码压缩包、源码目录和 Qt 中间构建目录只服务于这次 Qt 编译，成功后可以删除。

## 前置条件

- Visual Studio 2022 Build Tools：MSVC v143 x64 编译工具和 Windows 10/11 SDK。
- 官方独立版 CMake：默认使用 `D:\software\cmake-3.31.6\bin\cmake.exe`。
- Python 3：可在 `PATH` 中，或用 `-PythonPath` 指定。
- Ninja：脚本会在缺失时自动下载到 `D:\Qt\tools\ninja\ninja.exe`。
- 磁盘空间：Qt 源码、中间目录和安装目录合计建议预留 25 GB 以上。

不要使用 VS BuildTools 自带 CMake。本机它的 `cmake -E cmake_autogen` 会在 AUTOMOC 阶段崩溃，脚本会显式拒绝该路径。

## 构建 Qt 6 静态版

在仓库根目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-qt-build.ps1 `
  -QtVersion 6.8.3 `
  -QtRoot D:\Qt `
  -CMakePath D:\software\cmake-3.31.6\bin\cmake.exe `
  -RemoveIntermediates
```

需要代理下载源码或 Ninja 时加上：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-qt-build.ps1 `
  -QtVersion 6.8.3 `
  -QtRoot D:\Qt `
  -CMakePath D:\software\cmake-3.31.6\bin\cmake.exe `
  -Proxy http://127.0.0.1:7897 `
  -RemoveIntermediates
```

脚本行为：

- 自动导入 VS2022 x64 编译环境，不要求先打开 Native Tools Command Prompt。
- 下载 Qt 6.8.3 官方源码，并校验固定 SHA-256。
- 缺少 Ninja 时自动下载。
- 只编译本项目需要的 `qtbase`、`qtshadertools`、`qtdeclarative`、`qtsvg`。
- 安装到 `D:\Qt\6.8.3-static`，并检查安装结果必须是静态 Qt。
- `-RemoveIntermediates` 会在成功后删除源码压缩包、源码目录和 Qt 中间构建目录。

首次编译通常需要几十分钟到数小时，取决于机器性能。若已保留中间目录并只想增量重编 Qt，可加 `-Incremental`；若追求更小体积可加 `-Ltcg`，链接时间会明显增加。

常用参数：`-Parallel` 限制并行编译数（默认最多 10），`-Ltcg` 启用链接期优化，`-Incremental` 保留 Qt 中间目录，`-RemoveIntermediates` 成功后清理中间内容。

## 编译和打包项目

推荐直接使用发布脚本，它会配置 `build-qt6`、编译 Release、整理运行时文件并生成 zip：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-release.ps1 `
  -QtPrefixPath D:\Qt\6.8.3-static `
  -CMakePath D:\software\cmake-3.31.6\bin\cmake.exe `
  -PluginCatalogPublicKey ((Get-Content -LiteralPath .\registry\public-key.txt -Raw).Trim())
```

产物结构：

```text
build-qt6\Release\XiaoJiAutoKey.exe
build-qt6\Release\使用说明.mhtml
build-qt6\Release\audio\
build-qt6\Release\plugins\builtin\<插件ID>\<插件版本>\

dist\XiaoJiAutoKey\XiaoJiAutoKey.exe
dist\XiaoJiAutoKey\使用说明.mhtml
dist\XiaoJiAutoKey\audio\
dist\XiaoJiAutoKey\plugins\builtin\<插件ID>\<插件版本>\
dist\XiaoJiAutoKey.zip
```

主程序默认构建和部署小叽与 DD 两种内置驱动，USB 保留独立目标，由插件中心按需安装；小叽管理组件随小叽插件交付。已有 `build-qt6\Release` 可能包含历史遗留文件和用户数据，整理干净发布目录时只按清单收集当前随包小叽和 DD 版本。`dist\XiaoJiAutoKey.zip` 用于 Gitee 和 GitHub Release，两端使用同一份发布包，流程见 [双平台同步](repository-sync.md)。发布包绝不包含 `config.json`、用户安装插件、插件数据和缓存，具体清单见 [发布包内容](release-process.md#发布包内容)。

Qt 6 QML、Quick Controls、SVG 和 Shader Tools 静态链接进 exe 后，体积会大于旧 Widgets 版本，每次产物以实际构建为准。编译通过与功能运行确认分别记录；构建产物、日志及本机配置均不提交 Git。

在 Windows 的 Visual Studio 根工程中单独编译驱动插件时，目标需要带生成工程的相对目录：

```powershell
& 'D:\software\cmake-3.31.6\bin\cmake.exe' --build build-qt6 --config Release --target plugins/driver-dd/XiaoJiPluginDDPackage plugins/driver-usb/XiaoJiPluginUSBPackage
```

直接以 `plugins/driver-dd` 或 `plugins/driver-usb` 为源码目录独立配置时，使用对应的普通目标名即可。上述目标可刷新独立插件输出；DD 同时属于默认随包插件，USB 仍需按需安装。

## 手动清理

如果构建 Qt 时没有带 `-RemoveIntermediates`，成功后可清理：

```powershell
Remove-Item -LiteralPath `
  "D:\Qt\qt-everywhere-src-6.8.3.zip", `
  "D:\Qt\qt-everywhere-src-6.8.3", `
  "D:\Qt\qt-6.8.3-static-build" `
  -Recurse -Force
```

不要删除 `D:\Qt\6.8.3-static`；否则项目需要重新编译 Qt。项目侧需要重编时删除 `build-qt6` 即可，不要复用动态 Qt 的 `build-qt6-dyn` 缓存。
