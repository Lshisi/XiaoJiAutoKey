# 小叽按键

基于 Qt 5 + DD 驱动的 Windows 按键连发工具，支持键盘和鼠标按键的自动连发，提供中文界面。

## 功能说明

- 全局启停控制：配置一个全局快捷键，按一次启动，再按一次停止
- 开关模式（Toggle）：工具启动后自动开始连发，停止后统一停止
- 按压模式（Hold）：工具启动后，按住配置键才连发，松开立即停止
- 混合模式（可选）：开启后，按住按压模式键期间所有开关模式连发自动暂停，松开后恢复；关闭时按压和开关互不影响（默认关闭）
- 支持键盘和鼠标按键（左键、右键、中键、侧键）
- 每个按键可独立配置延迟间隔（ms）和启用/禁用状态
- 列表实时编辑：支持启用/禁用、删除、双击弹窗编辑
- 本地配置：自动读取和保存 `config.json`
- 多配置方案：支持创建多个配置方案，可随时切换、重命名、删除，默认加载上次使用的方案
- 运行时锁定：工具运行期间自动禁用所有编辑操作（添加按键、快捷键录制、配置切换等），防止误操作
- 启停提示音：可选播放 `audio/按键开启.mp3` 与 `audio/按键关闭.mp3`
- DD 驱动级模拟：绕过常规软件检测，适用于游戏等场景

## 技术架构

### UI 框架

使用 Qt 5 Widgets 构建界面，支持静态链接编译为单文件 exe 分发。

### 按键引擎

- 独立工作线程（`RepeaterEngine`）以 1ms tick 驱动，按配置间隔发送按键
- 通过 DD 驱动（`ddKey_` / `ddBtn_`）进行驱动级按键模拟
- 全局低级钩子（`WH_KEYBOARD_LL` / `WH_MOUSE_LL`）追踪物理按键状态

### Hold 压制 Toggle 的实现（混合模式）

混合模式是一个可选功能（默认关闭），开启后按住 Hold 键会压制所有 Toggle 连发。通过 `mixedMode_` 标志控制是否启用此行为。

由于 DD 驱动发送的按键不携带 `LLKHF_INJECTED` 标志，系统需要可靠地区分物理事件和自身注入事件。采用双轨追踪机制：

- `physicalDown_` + `ignoreCount_`：通用物理按键状态追踪，用计数器过滤 SendTap 产生的事件
- `holdPhysicalDown_` + `holdUpIgnoreCount_`：Hold 模式按键的独立物理状态追踪

Hold 模式按键的状态追踪逻辑：
- KEYDOWN：首次按下时加入集合；已在集合中时忽略（键盘重复和 SendTap 的 KEYDOWN 均无害）
- KEYUP：通过独立计数器区分 SendTap 产生的 KEYUP 和真实物理松开（每次 SendTap 只给 KEYUP 计数器 +1，键盘重复不产生 KEYUP 因此不会消耗计数器）

这种设计确保：
1. Windows 键盘重复不会干扰状态判断
2. SendTap 产生的 KEYUP 不会误移除物理按住状态
3. 真实松开时能可靠清除状态，让 Toggle 模式恢复连发

## 目录结构

```
├── CMakeLists.txt          # CMake 构建配置，Qt5 依赖
├── include/                # 头文件
│   ├── AppTypes.h          # 类型定义（KeyDescriptor, KeyBinding 等）
│   ├── ConfigStore.h       # JSON 配置读写
│   ├── DdDriver.h          # DD 驱动封装
│   ├── MainWindow.h        # Qt 主窗口 + 全局钩子
│   ├── ProfileManager.h    # 多配置方案管理
│   └── RepeaterEngine.h    # 连发引擎
├── src/                    # 实现代码
├── assets/                 # 图标、音频资源
├── third_party/dd/         # DD 驱动 DLL
└── scripts/                # 构建和打包脚本
```

## 构建环境

- Windows 10/11
- Visual Studio 2022（MSVC）
- CMake 3.20+
- Qt 5.15（静态编译版本位于 `C:\Qt\5.15.2-static`，或通过 Anaconda 等动态版本）

## 编译步骤

在 `CMakeLists.txt` 中确认 `CMAKE_PREFIX_PATH` 指向正确的 Qt 路径，然后：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

输出：`build/Release/XiaoJiAutoKey.exe`

已启用 MSVC 静态运行时链接（`/MT`），配合 Qt 静态编译可生成单文件分发。

## 构建 Qt 静态版本（可选）

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-static-qt.ps1
```

## 一键打包

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-release.ps1
```

脚本会自动完成：
1. 递增版本号（patch +1）
2. CMake 配置 + 编译（静态 Qt，单文件 exe）
3. 组装发布目录并打包为 `dist/XiaoJiAutoKey.zip`

打包产物结构：

```
XiaoJiAutoKey/
├── XiaoJiAutoKey.exe   # 主程序（静态链接 Qt，无需额外 DLL）
├── audio/              # 启停提示音
│   ├── 按键开启.mp3
│   └── 按键关闭.mp3
├── driver/             # DD 驱动 DLL
│   ├── DD32.dll
│   └── DD64.dll
└── logs/               # 日志目录
```

## 运行准备

1. 安装 DD 驱动（运行 DD 官方安装程序）
2. 解压发布包，`driver/` 目录下已包含 DD32.dll / DD64.dll
3. 以管理员身份启动程序，配置全局控制键和按键列表

## 配置文件

程序使用多配置方案系统，配置存储在 `%APPDATA%/XiaoJiAutoKey/` 目录下：

- `profiles.json`：配置方案列表（名称、路径、当前激活方案）
- `config.json` / `profile_N.json`：各方案的具体配置

每个配置文件包含：

- `controlHotkey`：全局启停键
- `defaultDelayMs`：新增按键默认延迟（默认 10ms）
- `beepEnabled`：是否开启启停提示音
- `mixedMode`：是否开启混合模式（Hold 压制 Toggle，默认 false）
- `bindings`：按键列表，每项包含键值、模式、延迟、启用状态
