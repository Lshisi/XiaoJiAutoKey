# 小叽按键

基于 Qt 5 + DD 驱动的 Windows 按键连发工具，支持键盘和鼠标按键的自动连发，提供中文界面。

## 功能说明

- 全局启停控制：配置一个全局快捷键，按一次启动，再按一次停止
- 开关模式（Toggle）：工具启动后自动开始连发，停止后统一停止
- 按压模式（Hold）：工具启动后，按住配置键才连发，松开立即停止
- 连发优先模式（可选）：开启后，按住按压模式键期间所有开关模式连发自动暂停，松开后恢复；关闭时按压和开关互不影响（默认关闭）
- 暂停按键：支持配置多个暂停键，按住暂停键时所有开关模式连发暂停，松开后恢复（独立于连发优先模式）
- 宏设置：支持录制按键序列，设置循环次数和触发模式（开关/按压），独立快捷键触发
- 按键连发与宏循环模式切换：同一页面通过标签切换，运行时互斥
- 门派主题：支持剑三门派主题切换，界面配色随门派变化
- 支持键盘和鼠标按键（左键、右键、中键、侧键）
- 每个按键可独立配置延迟间隔（ms）和启用/禁用状态
- 列表实时编辑：支持启用/禁用、删除、双击弹窗编辑
- 多配置方案：支持创建多个配置方案，可随时切换、重命名、删除，默认加载上次使用的方案
- 运行时锁定：工具运行期间自动禁用所有编辑操作（添加按键、快捷键录制、配置切换、模式切换等），防止误操作
- 弹窗屏蔽：编辑弹窗打开时自动屏蔽启停快捷键和宏触发，防止误触
- 自定义提示音：支持分别设置开启/关闭提示音，支持 mp3、wav、wma 格式
- 启停提示音：可选播放提示音
- 系统托盘：最小化到托盘，双击恢复窗口
- DD 驱动级模拟：绕过常规软件检测，适用于游戏等场景

## 技术架构

### UI 框架

使用 Qt 5 Widgets 构建现代化 Fluent Design 风格界面，侧边栏导航，支持静态链接编译为单文件 exe 分发。

页面结构：
- 按键设置页：包含「按键连发」和「宏循环」两个标签，通过顶部切换栏切换
  - 按键连发：按键列表管理、启停控制、暂停键配置、配置方案切换
  - 宏循环：宏序列编辑、触发模式、循环控制
- 全局设置页：提示音设置、门派主题、关闭行为、配置路径
- 关于页：版本信息

### 配置系统

采用统一单文件配置存储（`ConfigStore`），所有配置方案内联在同一个 JSON 文件中。每个配置方案（`ProfileConfig`）包含独立的按键列表、宏配置、启停快捷键、暂停键和连发优先设置。

### 按键引擎

- 独立工作线程（`RepeaterEngine`）以 1ms tick 驱动，按配置间隔发送按键
- 通过 DD 驱动（`ddKey_` / `ddBtn_`）进行驱动级按键模拟
- 全局低级钩子（`WH_KEYBOARD_LL` / `WH_MOUSE_LL`）追踪物理按键状态
- 支持暂停键：按住暂停键时所有 Toggle 模式连发暂停

### 宏引擎

- 独立工作线程（`MacroEngine`）驱动宏序列执行
- 支持 Toggle（按一次开始/再按停止）和 Hold（按住执行/松开停止）两种触发模式
- 每个宏有独立的触发快捷键
- 支持设置循环次数（0 = 无限循环）

### 模式互斥

按键连发和宏循环通过 `activeMode_` 原子变量实现互斥：
- `activeMode_ == 0`：连发模式，Hook 响应连发快捷键和物理按键
- `activeMode_ == 1`：宏模式，Hook 响应宏触发键

运行时不允许切换模式，UI 按钮自动禁用。

### 连发优先与暂停键的实现

连发优先（原混合模式）通过 `mixedMode_` 标志控制。暂停键通过 `pauseKeys_` + `pauseKeysDown_` 集合追踪。两者独立工作，任一条件满足时 Toggle 模式连发暂停。

由于 DD 驱动发送的按键不携带 `LLKHF_INJECTED` 标志，系统需要可靠地区分物理事件和自身注入事件。采用双轨追踪机制：

- `physicalDown_` + `ignoreCount_`：通用物理按键状态追踪，用计数器过滤 SendTap 产生的事件
- `holdPhysicalDown_` + `holdUpIgnoreCount_`：Hold 模式按键的独立物理状态追踪

## 目录结构

```
├── CMakeLists.txt          # CMake 构建配置，Qt5 依赖
├── include/                # 头文件
│   ├── AppTypes.h          # 类型定义（KeyDescriptor, KeyBinding, ProfileConfig, MacroConfig 等）
│   ├── ConfigStore.h       # JSON 配置读写（统一单文件存储）
│   ├── DdDriver.h          # DD 驱动封装
│   ├── MacroEngine.h       # 宏引擎
│   ├── MainWindow.h        # Qt 主窗口 + 全局钩子
│   └── RepeaterEngine.h    # 连发引擎（含暂停键支持）
├── src/                    # 实现代码
│   ├── ui/components/      # Fluent Design 组件
│   ├── ui/pages/           # 页面（KeyPage, MacroPage, SettingsPage, AboutPage）
│   └── ui/styles/          # 设计令牌和主题
├── assets/                 # 图标、音频、门派图标资源
├── third_party/dd/         # DD 驱动 DLL
└── scripts/                # 构建和打包脚本
```

## 构建环境

- Windows 10/11
- Visual Studio 2022（MSVC）
- CMake 3.20+
- Qt 5.15（静态编译版本位于 `C:\Qt\5.15.2-static`，或通过 Anaconda 等动态版本）

## 编译步骤

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

输出：`build/Release/XiaoJiAutoKey.exe`

## 一键打包

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-release.ps1
```

打包产物结构：

```
XiaoJiAutoKey/
├── XiaoJiAutoKey.exe   # 主程序（静态链接 Qt，无需额外 DLL）
├── audio/              # 启停提示音
├── driver/             # DD 驱动 DLL（DD32.dll / DD64.dll）
└── logs/               # 日志目录
```

## 运行准备

1. 安装 DD 驱动（运行 DD 官方安装程序）
2. 解压发布包，`driver/` 目录下已包含 DD32.dll / DD64.dll
3. 以管理员身份启动程序，配置全局控制键和按键列表

## 配置文件

程序使用统一单文件配置系统，配置存储在程序目录下的 `config.json`：

```json
{
  "beepEnabled": true,
  "closeAction": "ask",
  "faction": "none",
  "customSoundOn": "",
  "customSoundOff": "",
  "activeProfileId": 1,
  "profiles": [
    {
      "id": 1,
      "name": "默认配置",
      "controlHotkey": "...",
      "defaultDelayMs": 10,
      "mixedMode": false,
      "pauseKeys": [],
      "bindings": [...],
      "macros": [...]
    }
  ]
}
```

全局设置：
- `beepEnabled`：是否开启启停提示音
- `closeAction`：关闭窗口行为（ask/minimize/exit）
- `faction`：门派主题 ID
- `customSoundOn`：自定义开启提示音路径（空表示使用默认）
- `customSoundOff`：自定义关闭提示音路径（空表示使用默认）

每个配置方案包含：
- `controlHotkey`：全局启停键
- `defaultDelayMs`：新增按键默认延迟（默认 10ms）
- `mixedMode`：是否开启连发优先（Hold 压制 Toggle，默认 false）
- `pauseKeys`：暂停按键列表
- `bindings`：按键列表
- `macros`：宏配置列表

## 版本历史

### v1.0.6（当前版本）
- 修复首次启动无默认配置方案的问题（首次运行自动创建"默认配置"和"默认宏"）
- 移除自定义配置文件路径功能，配置文件固定存储在程序同级目录的 `config.json`
- 修复宏步骤列表排序功能：添加上移/下移按钮，支持调整步骤顺序
- 修复宏配置启停快捷键互斥逻辑：不同宏允许设置相同触发键，同一时间只有当前选中的宏响应
- 修复全局设置中「关闭窗口时」选项显示不同步的问题

### v1.0.5
- 重构配置系统：统一单文件存储，移除独立的 ProfileManager，所有配置方案内联在同一 JSON 文件中
- 重构按键设置界面：合并按键连发和宏循环到同一页面，通过顶部标签切换，运行时互斥
- 添加暂停按键功能：支持配置多个暂停键，按住时暂停所有开关模式连发
- 添加自定义提示音：支持分别设置开启/关闭提示音文件（mp3、wav、wma）
- 弹窗打开时自动屏蔽启停快捷键和宏触发，防止误触
- 侧边栏精简为三项（按键设置、全局设置、关于软件）
- 移除按键连发与宏之间的按键冲突检测（两种模式互斥，不再需要）
- 全局设置页面添加滚动支持，调整卡片顺序（提示音和门派主题前置）

### v1.0.4
- 添加宏设置功能（按键序列录制、循环执行、独立触发键）
- 添加门派主题功能，支持剑三各门派配色切换
- 添加QQ群联系方式
- 优化表格组件的文本对齐和列宽布局

### v1.0.3
- 添加混合模式（Hold 压制 Toggle）
- 添加多配置方案管理（创建、切换、重命名、删除）
- 修复发布脚本和项目配置问题

### v1.0.2
- 添加按键按下和释放分离功能
- 添加使用说明文档

### v1.0.1
- 重构用户界面为现代化 Fluent Design 风格
- 新增功能性体验优化

### v1.0.0
- 初始版本：Qt5 + DD 驱动按键连发工具
- 支持 Toggle/Hold 两种模式
- 全局快捷键启停
- 本地 JSON 配置保存

## 注意事项

- 部分杀毒软件可能误报，请添加信任
- 本工具仅供学习交流，使用后果自负
