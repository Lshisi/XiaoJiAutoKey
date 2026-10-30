# 小叽按键（XiaoJiAutoKey）

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](./LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D4.svg)](#从源码构建)
[![Qt](https://img.shields.io/badge/Qt-6.8%2B-41CD52.svg)](#从源码构建)
[![Release](https://img.shields.io/badge/下载-Gitee%20Releases-brightgreen.svg)](https://gitee.com/fourteen-fourteen/xiao-ji-auto-key/releases)

基于 Qt 6 + QML 的 Windows 按键连发工具。支持键盘和鼠标按键的自动连发与宏循环，提供剑三门派主题的中文界面，单文件绿色免安装，可静驻系统托盘。

## 目录

- [功能特性](#功能特性)
- [下载与安装](#下载与安装)
- [快速上手](#快速上手)
- [软件更新](#软件更新)
- [从源码构建](#从源码构建)
- [配置说明](#配置说明)
- [技术架构](#技术架构)
- [目录结构](#目录结构)
- [参与贡献](#参与贡献)
- [开源协议](#开源协议)
- [免责声明](#免责声明)

## 功能特性

### 按键连发

- 开关模式（Toggle）：工具启动后自动开始连发，停止后统一停止
- 按压模式（Hold）：按住配置键才连发，松开立即停止
- 连发优先：按住参与优先的按压键期间，所有开关连发自动暂停，松开后恢复；每个按压键可单独设置是否参与
- WASD 防卡位：按住 W/A/S/D 移动时临时降低开关连发频率，松开后恢复
- 暂停按键：支持配置多个暂停键，按住时暂停所有开关模式连发（独立于连发优先）
- 每个按键可独立配置延迟间隔（ms，最低 1ms）和启用状态，列表实时编辑
- 多配置方案：创建、切换、重命名、删除，默认加载上次使用的方案

### 宏循环

- 按键序列录制与编辑，支持上移/下移调整步骤顺序
- 循环次数、触发模式（开关/按压）和宏暂停键独立配置
- 独立快捷键触发，多个宏可共用触发键，同一时间只有当前选中的宏响应
- 与按键连发可同时运行，配置相同按键不冲突，运行时逐键仲裁

### 界面

- Fluent Design 风格，剑三门派主题切换，门派图标 SVG 渲染
- 跟随系统深色模式，适配高 DPI 与多显示器缩放切换
- 状态悬浮窗：显示连发/宏启停状态，支持拖动、锁定、透明度调节、多屏位置保持
- 系统托盘：最小化到托盘，双击恢复；重复启动程序时自动唤起已运行实例的窗口（进程保持唯一）
- 运行时自动锁定编辑操作，编辑弹窗打开时屏蔽启停快捷键，防止误触

### 驱动

- DD/HID 驱动级按键模拟，适用于游戏等场景
- 驱动管理页查看 DD/HID 状态、切换驱动模式、卸载 HID
- 支持键盘和鼠标按键（左键、右键、中键、侧键）
- 自定义提示音：连发和宏可分别设置提示音与音量（mp3、wav、wma、mid）

### 软件更新

- **只在关于页手动点击"检查更新"时联网**（读取 Gitee Releases），其余时间永不联网检查
- 发现新版本可在程序内直接下载更新包，一键重启完成更新；更新失败自动回滚
- 按键连发页显示"有新版本"小标签，点击跳转关于页
- 无强制弹窗、无强制更新逻辑

## 下载与安装

1. 前往 [Gitee Releases](https://gitee.com/fourteen-fourteen/xiao-ji-auto-key/releases) 下载最新版本的 zip 压缩包
2. 解压到任意目录（无需安装）
3. 以管理员身份运行 `XiaoJiAutoKey.exe`

> 解压后的 `driver/` 目录包含 DD/HID 运行库（DD32.dll / DD64.dll / hid/ddhid.63340.dll），请保持在 exe 同级目录。`config.json` 为运行时自动生成的用户配置，升级新版本时会被自动保留。

## 快速上手

1. **安装驱动**：首次使用请在「驱动管理」页确认 DD/HID 驱动状态，必要时切换驱动模式（默认 DD，也可切换 HID）
2. **配置全局启停键**：在按键连发页设置一个全局快捷键，按一次启动连发，再按一次停止
3. **添加连发按键**：点击「添加按键」录制按键，选择模式（开关/按压）和延迟间隔
4. **（可选）配置宏**：在宏循环页录制按键序列，设置循环次数和触发键
5. 具体操作说明见发布包内的《使用说明.mhtml》（可在关于页直接打开）

## 软件更新

版本发布基于 [Gitee Releases](https://gitee.com/fourteen-fourteen/xiao-ji-auto-key/releases)（对应仓库 tag `vx.y.z`）：

- 关于页 → 软件更新 → 「检查更新」，手动检查新版本
- 发现新版本后可「下载并安装」，下载完成后「立即重启更新」，程序会自动停止连发/宏、替换文件并重启
- 也可以点击「前往下载」在浏览器打开发布页手动下载
- 程序永远不会在启动、运行或关机时自动联网检查更新

## 从源码构建

### 环境要求

- Windows 10/11
- Visual Studio 2022（MSVC v143 x64 和 Windows SDK）
- 官方独立版 CMake 3.30+（不要使用 VS BuildTools 自带 CMake）
- Qt 6.8 或更新版本，包含 Qt QML、Qt Quick、Qt Quick Controls、Qt Quick Widgets、Qt SVG、Qt Shader Tools
- 发布包使用静态 Qt，安装路径由 `CMAKE_PREFIX_PATH` 指定，不再使用旧 Qt 构建目录

### 编译

```powershell
& "D:\software\cmake-3.31.6\bin\cmake.exe" -S . -B build-qt6 -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="D:/Qt/6.8.3-static"
& "D:\software\cmake-3.31.6\bin\cmake.exe" --build build-qt6 --config Release --target XiaoJiAutoKey
```

输出：`build-qt6/Release`（与旧版结构一致，包含 exe、使用说明、`audio/` 和 `driver/`；QML 组件编译并嵌入 exe，不需要额外分发 QML 文件）。

需要重新编译静态 Qt 时执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-qt-build.ps1 -QtVersion 6.8.3 -QtRoot D:\Qt -CMakePath D:\software\cmake-3.31.6\bin\cmake.exe -RemoveIntermediates
```

脚本会自动下载并校验 Qt 源码、导入 MSVC x64 环境、下载 Ninja、编译并安装到 `D:\Qt\6.8.3-static`。完整说明见 [Qt 6 静态构建规范](docs/build-qt6-static.md)。

主窗口标题栏、五个页面、剪贴板分享提示条、状态悬浮窗和悬浮窗锁定按钮均使用 QML。标题栏保留原有拖动、最小化和关闭确认；分享提示条保留点击导入、关闭抑制和 8 秒自动隐藏，位置随窗口尺寸自动调整。原生窗口负责 Windows 消息、托盘、鼠标穿透和 DPI 协调。悬浮窗以单个着色器绘制通透底、弧面轮廓、交互高光和阴影；不采集桌面或游戏画面，不包含背景实时折射，也不是苹果原生 Liquid Glass 材质。调整透明度只影响玻璃底，不会使文字一起变淡。

按键录制、配置命名与确认、退出确认、悬浮窗设置、音效、进程管理、导入导出、更新、驱动自检和卸载进度等自定义弹窗的内容与控件均使用 QML；`QDialog` 仅承载原生窗口、模态和屏幕约束，系统文件选择器保留原生交互。按键引擎及配置格式不变。

旧 Widgets 自绘控件和菜单实现已移除。当前是 QML 自绘界面与原生宿主组合，不是纯 `QQuickWindow` 应用；系统托盘菜单、文件选择器和 Qt 初始化前的单实例提示继续使用原生实现。

### 打包发布

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-release.ps1 -QtPrefixPath "D:\Qt\6.8.3-static" -CMakePath "D:\software\cmake-3.31.6\bin\cmake.exe"
```

打包产物位于 `dist/XiaoJiAutoKey.zip`，包含主程序、离线使用说明、内置提示音和 DD/HID 运行库；打包脚本会自动排除 `config.json`。

更详细的构建注意事项见 [AGENTS.md](AGENTS.md)。

## 配置说明

程序使用统一单文件配置，存储在 exe 同级目录的 `config.json`：

```json
{
  "beepEnabled": true,
  "closeAction": "ask",
  "driverMode": "legacy",
  "faction": "cangjian",
  "latestKnownVersion": "",
  "activeProfileId": 1,
  "profiles": [
    {
      "id": 1,
      "name": "默认配置",
      "controlHotkey": "...",
      "defaultDelayMs": 30,
      "mixedMode": false,
      "wasdProtection": false,
      "pauseKeys": [],
      "bindings": [],
      "macros": []
    }
  ]
}
```

### 全局设置

| 字段 | 说明 |
| --- | --- |
| `beepEnabled` | 是否开启按键连发启停提示音 |
| `closeAction` | 关闭窗口行为（`ask` / `minimize` / `exit`） |
| `driverMode` | 驱动模式（`legacy` = DD，`hid` = HID，`usb` = USB 硬件） |
| `faction` | 门派主题 ID |
| `latestKnownVersion` | 最近一次手动检查发现的版本号，仅用于保持"有新版本"标签显示，不触发联网 |
| `customSoundOn` / `customSoundOff` | 自定义连发开启/关闭提示音路径（空 = 内置音） |
| `soundVolume` | 连发提示音音量（0-100） |
| `macroBeepEnabled` / `macroSoundOn` / `macroSoundOff` / `macroSoundVolume` | 宏循环对应提示音设置 |
| `statusOverlayShowRepeat` / `statusOverlayShowMacro` / `statusOverlayOpacity` / `statusOverlayLocked` | 状态悬浮窗显示与外观 |

### 配置方案（Profile）

每个配置方案包含独立的按键列表（`bindings`）、宏配置（`macros`）、启停快捷键（`controlHotkey`）、暂停键（`pauseKeys`）、连发优先（`mixedMode`）、WASD 防卡位（`wasdProtection`）和新增按键默认间隔（`defaultDelayMs`，默认 30ms、最低 1ms）。

## 技术架构

### 分层结构

项目按职责拆成三层：

- **core**：纯业务和底层能力——`AppTypes`、`ConfigStore`、`DdDriver`、`InputDriverRouter`、`RepeaterEngine`、`MacroEngine`
- **qml**：标题栏、主页面、分享提示条、悬浮窗、自定义弹窗及共用控件，统一布局、门派强调色和克制的玻璃材质
- **ui**：C++ 界面模型、设计令牌与取点窗口的原生宿主
- **app**：应用层装配和协调——主窗口、全局输入 hook、controller、coordinator、dialog、service

`MainWindow` 只负责应用启动、UI 接线、hook 启动和关闭事件协调，具体功能拆分到独立模块：

- `GlobalInputHook`：管理 `WH_KEYBOARD_LL` / `WH_MOUSE_LL` 全局钩子线程，通过 Qt signal 发出捕获键、启停热键、物理按键变化和宏触发变化
- `RepeaterEngine` / `MacroEngine`：独立工作线程驱动连发与宏序列，高精度定时
- `InputDriverRouter`：按键级所有权仲裁（见下）
- `ProfileController` / `MacroController`：配置修改与业务校验
- `*Coordinator`：连接页面 signal、controller、engine、config 保存和页面刷新
- `UpdateService` / `UpdateCoordinator`：软件更新（仅手动触发联网）

### 运行时按键互斥

按键连发和宏循环不是页面级或配置级互斥。`InputDriverRouter` 按实际运行状态为每个按键登记来源和激活顺序：

- 双方同时申请同一个按键时，先申请的一方保留所有权；互不相同的按键并行发送
- 所有权释放或转移时，路由器会先补发原来源的抬键事件，避免按键卡住
- 只是在两个页面配置相同按键不会冲突；双方实际运行且同时申请该按键时才触发仲裁

### 物理按键与注入事件的区分

DD 驱动发送的按键不携带 `LLKHF_INJECTED` 标志，采用双轨追踪机制区分物理事件和自身注入事件：

- `physicalDown_` + `ignoreCount_`：通用物理按键状态追踪，用计数器过滤 SendTap 产生的事件
- `holdPhysicalDown_` + `holdUpIgnoreCount_`：Hold 模式按键的独立物理状态追踪

## 目录结构

```
├── CMakeLists.txt          # CMake 构建配置，Qt 6 / QML 模块
├── AGENTS.md               # 构建/发布流程约束说明
├── include/
│   ├── app/                # 应用层：主窗口、coordinator、dialog、service、输入 hook
│   └── core/               # 核心层：类型、配置、驱动、引擎、路由
├── src/
│   ├── app/                # 应用层实现（main.cpp、main_window/、coordinators/ 等）
│   ├── core/               # 核心层实现
│   ├── qml/                # QML 页面与共用控件
│   └── ui/                 # C++ 页面模型、原生组件、设计令牌
├── assets/                 # 图标、音频、门派图标、使用说明
├── firmware/               # 硬件桥接固件（RP2040 Pico / CH552）
│   ├── pico/               # 基于 Pico SDK + TinyUSB 的 USB 复合设备固件
│   └── ch552/              # CH552 固件
├── third_party/
│   ├── dd/                 # DD 驱动运行库（DD32.dll / DD64.dll / hid）
│   └── miniz/              # zip 解压库（更新功能使用）
└── scripts/                # 构建和打包脚本
```

## 参与贡献

欢迎通过 Issue 反馈问题、提交 Pull Request 改进代码。提交前请：

- 遵循现有代码风格（分层结构、coordinator 模式、UI 规范见 [AGENTS.md](AGENTS.md)）
- 界面改动请同时验证浅色/深色模式和 100%/125%/150%/200% DPI 缩放
- 保持"软件更新仅在用户手动触发时联网"的原则

## 开源协议

- 本项目代码以 [GPL-3.0](./LICENSE) 协议开源
- `third_party/miniz/`：miniz，公有领域 / MIT
- `third_party/dd/` 下的 DD 驱动运行库为第三方闭源组件，版权归其原作者所有，仅作为运行时依赖分发

## 免责声明

1. 本软件仅供学习和交流使用
2. 使用本软件产生的一切后果由用户自行承担
3. 作者不对软件使用造成的任何损失负责
4. 禁止使用本软件进行任何违法活动
5. 部分杀毒软件可能误报，请自行添加信任
