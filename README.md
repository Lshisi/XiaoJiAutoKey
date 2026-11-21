# 小叽按键（XiaoJiAutoKey）

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](./LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D4.svg)](#从源码构建)
[![Qt](https://img.shields.io/badge/Qt-6.8%2B-41CD52.svg)](#从源码构建)
[![Release](https://img.shields.io/badge/下载-Gitee%20Releases-brightgreen.svg)](https://gitee.com/fourteen-fourteen/xiao-ji-auto-key/releases)

基于 Qt 6 + QML 的 Windows 按键连发工具。支持键盘和鼠标按键的自动连发与循环连发，提供剑三门派主题的中文界面，单文件绿色免安装，可静驻系统托盘。

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

- 支持 `Ctrl`／`Shift`／`Alt` 加键盘或鼠标主键的组合键，可用于连发按键、启停快捷键、暂停键和切换键；组合键按完整修饰键集合精确匹配，`Ctrl+1` 不会误触单独 `1`
- 组合键连发期间持续按住 `Ctrl`／`Shift`／`Alt`，仅主键按间隔重复按下、松开；修饰键保持复用长按输出，停止、暂停、切换或停用时先释放主键，再释放不再使用的修饰键。`Ctrl+1` 与 `Ctrl+Alt+1` 同为开关模式时不能同时启用；分别配置为两个独立项或两个切换段时可以互斥使用。同修饰键、不同主键可并发，共用修饰键只在最后一项结束后释放。完整规则见 [按键模式与冲突](docs/key-mode-rules.md)。
- 开关模式（Toggle）：工具启动后自动开始连发，停止后统一停止
- 长按模式（LongPress）：工具启动后持续按住配置键，停止时松开；切换前台窗口后自动恢复长按，仍遵循游戏前台限制、暂停键、按压优先和冲突规则
- 武学助手：在长按项中启用，配合按压优先和游戏内置长按连发使用。每轮固定按住 3.1 秒，计时中失焦则回到原窗口后重新计时。
- 游戏连发后，手动按配置键即交给用户控制，这一项不再自动补长按或收尾键；需要工具接管时，停止后重新启动。不识别游戏画面。
- 小叽驱动下，武学助手支持对新按下的单独 Alt／Ctrl／Shift 暂停键先完成收尾、再转交暂停键，并保持后续输入顺序；此前已按住的修饰键及绕过低级钩子的游戏输入路径仍有隔离限制。
- 按压模式（Hold）：按住配置键才连发，松开立即停止
- 独立模式：启动工具后按配置键开启连发，按其他独立模式键可切换，同一时间只连发一个。默认再按当前键停止，添加或编辑按键时开启「防误停」后，再按当前键会继续连发
- 切换模式：通过切换键轮换当前连发按键，列表下方显示当前段，停用按键会被跳过；可选择记住上次切换位置
- 按压优先：按住参与优先的按压键期间，开关、独立和切换模式连发暂停，长按键松开；松开后恢复，每个按压键可单独设置是否参与
- WASD 防卡位：按住 W/A/S/D 移动时临时降低开关连发频率，松开后恢复
- 暂停按键：支持多个暂停键，按住时暂停开关、独立和切换模式并松开长按键，按压键仍可触发；松开后恢复，独立于按压优先
- 每个连发按键可独立配置延迟间隔（ms，最低 1ms）和启用状态；长按模式无需间隔，列表显示为「—」
- 每个连发按键可设置可选名称，列表中的「按键」与「名称」分列显示，悬浮窗同时显示名称和实际键位；名称只影响展示，不参与冲突判断
- 按键／循环列表旁的「按键总览」通过完整用途名称与色块显示键盘、鼠标和滚轮配置，点击展开详情；添加或编辑时保留总览，保存后直接刷新，支持组合键与重复步骤关联，运行中仅查看
- 按键与循环列表左侧选择框支持多选和表头全选；勾选后可批量修改参数或删除，未勾选字段保留原值；批量删除需确认
- 多配置方案：创建、复制、切换、重命名、删除，默认加载上次使用的方案

### 循环连发

- 循环步骤、触发键和暂停键支持组合键；录制时先按住修饰键再按主键，输出时按修饰键、主键顺序按下并反向松开
- 支持点击、保持、按下、松开、等待和滚轮六种步骤；按下／松开可跨步骤配对，例如「按下 Ctrl → 点击 1 → 等待 → 点击 2 → 松开 Ctrl」
- 点击「录制一轮」，经过 3 秒准备后可切回游戏，记录键盘、鼠标键、滚轮和间隔；按 F10 停止并预览，确认后追加到当前循环。录制前停止两种连发，停止时补齐尚未松开的键，F10 和录制窗口本身的操作不录入
- 录制时将动作间的停顿直接保存为前一步的「间隔」，连续的同键按下／松开合并为「保持」，多键交叠保留原始顺序；结果确认中同时显示动作与间隔，独立「等待」可按需手动添加
- 可选记录鼠标左、右键的点击位置，也可在步骤编辑中拾取；中键、侧键和滚轮不记录位置，不记录连续移动轨迹。录制结果沿用步骤编辑、插入、复制、拖动或 Alt+↑/↓ 排序、停用和批量修改
- 循环列表左侧选择框支持多选和表头全选，勾选后可批量复制或删除；批量删除需确认
- 两种列表的启用开关统一放在操作列前；循环步骤可独立停用，执行时跳过该步骤和间隔，保留原有顺序、名称与点位
- 动作后间隔允许为 0：点击按下约 5ms 后松开，保持按指定时长松开，随后等待该间隔；按下、松开和滚轮在动作后等待；纯等待只有等待时长。列表下方按动作类型估算单轮与指定轮数的时长
- 循环步骤支持批量修改启用状态、动作后间隔及保持／等待时长，未勾选字段保留原值；启动前检查按下／松开配对和总时长
- 暂停时释放当前持有键并冻结计时，恢复后继续剩余保持／等待；已经发送的点击不会重发。停止、切换配置和退出时清理持有键
- 循环步骤可设置可选名称，列表中的「动作」与「名称」分列显示；悬浮窗显示当前动作及名称
- 滚轮输出支持小叽驱动及 DD 的 `DD_whl`，可选方向与 1–100 格；当前 USB 固件没有滚轮命令，含滚轮的循环在该驱动下会提示无法启动
- 每个循环最多 512 步；单次录制接近剩余容量时自动停止并进入确认，分享导入遇到超限内容会拒绝，不截断步骤
- 循环次数、触发模式（开关/按压）和宏暂停键独立配置
- 独立快捷键触发，多个宏可共用触发键，同一时间只有当前选中的宏响应
- 与按键连发可同时运行，配置相同按键不冲突，运行时逐键仲裁

### 使用统计

- 左侧「使用统计」页查看今日时长、累计时长、最近 270 天（含今天）的日历热力图和全年月度趋势；年份选择仅切换月度趋势，本月累计段用虚线显示
- 悬停日期查看当天使用时长，也可用 Tab 聚焦日历后通过方向键查看
- 只累计连发运行时间，暂停、生效范围挂起和电脑休眠期间不计入；两种连发同时运行时不重复累计
- 按本机日期记录，跨午夜自动拆分；记录保存在 `config.json`，从启用此功能起开始累计

### 界面

- Fluent Design 风格，剑三门派主题切换，门派图标 SVG 渲染
- 导航和控件图标内置 SVG，跟随主题着色，不依赖系统图标字体
- 跟随系统深色模式，适配高 DPI 与多显示器缩放切换
- 状态悬浮窗：显示连发/宏启停状态，支持拖动、锁定、透明度调节、多屏位置保持
- 系统托盘：最小化到托盘，双击恢复；重复启动程序时自动唤起已运行实例的窗口（进程保持唯一）
- 运行时自动锁定编辑操作，编辑弹窗打开时屏蔽启停快捷键，防止误触
- 首次启动提供操作引导，支持跳过、继续和在关于页重新查看；独立、切换和循环连发按需说明

### 驱动

- 小叽驱动默认为第一驱动，DD 为第二驱动，USB 为第三驱动；已接入现有连发和循环输出路径
- 驱动管理页查看状态、切换及管理小叽/DD 驱动；旧 HID 只保留卸载，检测到残留即持续显示按钮
- 小叽驱动接入 NKRO 多键保持、F1–F24、媒体/浏览器键、鼠标五键、移动和滚轮；当前面向 Windows x64，本机签名安装依赖 .NET Framework 4.8、Windows PowerShell 及系统信任策略。
- 驱动管理页可预览并复制诊断信息，报告不包含个人路径、剪贴板内容和完整配置
- 支持键盘和鼠标按键（左键、右键、中键、侧键）及 Ctrl／Shift／Alt 组合主键
- 自定义提示音：保留按键／循环整组开关，各事件可分别设置开关、音量与音频；支持整体启停、独立键启停与换键、切换模式换键、暂停／恢复和录制开始／结束（mp3、wav、wma、mid、midi）。默认音频文件名见 [提示音配置](docs/sound-cues.md)。

### 软件更新

- **只在关于页手动点击"检查更新"时联网**（读取 Gitee Releases），其余时间永不联网检查
- 发现新版本可在程序内直接下载更新包，一键重启完成更新；更新失败自动回滚
- 按键连发页显示"有新版本"小标签，点击跳转关于页
- 无强制弹窗、无强制更新逻辑

## 下载与安装

1. 前往 [Gitee Releases](https://gitee.com/fourteen-fourteen/xiao-ji-auto-key/releases) 下载最新版本的 zip 压缩包
2. 解压到任意目录（无需安装）
3. 以管理员身份运行 `XiaoJiAutoKey.exe`

> 解压后的 `driver/` 目录包含 DD 运行库（DD32.dll / DD64.dll）和小叽驱动管理/本机签名组件（xiaoji/xiaoji.exe、xiaoji/xiaoji-setup.exe），请保持在 exe 同级目录。小叽驱动已内置安装包，首次安装在单独确认证书信任后自动完成本机签名和安装；已有可信包时优先复用。`config.json` 为运行时自动生成的用户配置，升级新版本时会被自动保留。

## 快速上手

首次打开软件会显示新手引导，依次介绍驱动、添加按键、模式与启用状态、启停快捷键、生效范围以及启动/暂停/停止。驱动已就绪时直接进入按键配置，异常时先衔接驱动自检。引导中不会自动开始连发，完成后由用户主动启动。可随时跳过，之后在「关于软件 → 配置说明 → 新手引导」重新查看；已有配置的升级用户不会自动重走。

1. **安装驱动**：首次使用请在「驱动管理」页确认小叽驱动状态，点击安装/修复，按首次提示确认证书信任后自动安装，或按需切换到 DD / USB。旧 HID 不再提供安装和启用功能
2. **配置全局启停键**：在按键连发页设置一个全局快捷键，按一次启动连发，再按一次停止
3. **添加连发按键**：点击「添加按键」录制按键，选择模式（开关/按压/独立/切换/长按）和间隔；长按模式无需间隔
4. **（可选）配置循环**：在「循环连发」页点击「添加步骤」手工编排，或用「录制一轮」生成步骤，设置循环次数和触发键；首次进入有独立的简短引导
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
- .NET Framework 4.8（用于构建和运行小叽本机签名助手；无需另装驱动 WDK 或签名工具）
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

可用独立的 Qt 6.8.3 预览程序查看完整主窗口、全部主页面及常用弹窗。Mac 执行 `./scripts/preview-macos.sh`，Windows 使用 `scripts/preview-windows.ps1`；环境准备和仅编译命令见 [界面预览](docs/ui-preview.md)。

本地观察键盘、组合键、长按、鼠标五键和滚轮输出时，可直接用浏览器打开 [输入查看页](scripts/input-viewer.html)。页面离线运行，提供逐键统计、按住时长、连发间隔、事件筛选与 JSON 导出；使用方法和浏览器限制见 [本地输入查看](docs/input-viewer.md)。

主窗口标题栏、六个主页面、剪贴板分享提示条、状态悬浮窗和悬浮窗锁定按钮均使用 QML。标题栏保留原有拖动、最小化和关闭确认；分享提示条保留点击导入、关闭抑制和 8 秒自动隐藏，位置随窗口尺寸自动调整。原生窗口负责 Windows 消息、托盘、鼠标穿透和 DPI 协调。悬浮窗以单个着色器绘制通透底、弧面轮廓、交互高光和阴影；不采集桌面或游戏画面，不包含背景实时折射，也不是苹果原生 Liquid Glass 材质。调整透明度只影响玻璃底，不会使文字一起变淡。

按键录制、配置命名与确认、退出确认、悬浮窗设置、音效、进程管理、导入导出、更新、驱动自检和卸载进度等自定义弹窗的内容与控件均使用 QML；`QDialog` 仅承载原生窗口、模态和屏幕约束，系统文件选择器保留原生交互。按键引擎及按键方案格式不变。

旧 Widgets 自绘控件和菜单实现已移除。当前是 QML 自绘界面与原生宿主组合，不是纯 `QQuickWindow` 应用；系统托盘菜单、文件选择器和 Qt 初始化前的单实例提示继续使用原生实现。

### 打包发布

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-release.ps1 -QtPrefixPath "D:\Qt\6.8.3-static" -CMakePath "D:\software\cmake-3.31.6\bin\cmake.exe"
```

打包产物位于 `dist/XiaoJiAutoKey.zip`，包含主程序、离线使用说明、内置提示音、DD 运行库及小叽驱动管理、本机签名组件和许可证；打包脚本会自动排除 `config.json`。

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
  "dailyUsageMs": {},
  "activeProfileId": 1,
  "profiles": [
    {
      "id": 1,
      "name": "默认配置",
      "controlHotkey": "...",
      "defaultDelayMs": 10,
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
| `beepEnabled` | 按键连发整组提示音开关；各事件还需开启自己的单项开关 |
| `macroBeepEnabled` | 循环连发及录制整组提示音开关 |
| `closeAction` | 关闭窗口行为（`ask` / `minimize` / `exit`） |
| `driverMode` | 驱动模式（`xiaoji` = 小叽，默认；`legacy` = DD；`usb` = USB 硬件；旧 `hid` 自动迁移到小叽） |
| `faction` | 门派主题 ID |
| `latestKnownVersion` | 最近一次手动检查发现的版本号，仅用于保持"有新版本"标签显示，不触发联网 |
| `dailyUsageMs` | 本机每日连发运行时长，键为 `YYYY-MM-DD`，值为毫秒数；不随按键方案分享 |
| `onboarding` | 本机新手引导的版本、状态、步骤及已读提示；完成或跳过后不再自动展示，不随按键方案分享 |
| `sounds` | 按事件 ID 保存 `enabled`、`path` 和 `volume`（0–100）；空路径使用该事件的默认音频，详见 [提示音配置](docs/sound-cues.md) |
| `statusOverlayShowRepeat` / `statusOverlayShowMacro` / `statusOverlayOpacity` / `statusOverlayLocked` | 状态悬浮窗显示与外观 |
| `macroOverlayShowCurrentStep` | 状态悬浮窗是否显示循环当前步骤 |

### 配置方案（Profile）

每个按键配置方案包含独立的按键列表（`bindings`）、启停快捷键（`controlHotkey`）、暂停键（`pauseKeys`）、按压优先（`mixedMode`）、WASD 防卡位（`wasdProtection`）和默认间隔（`defaultDelayMs`，默认 10ms、最低 1ms）。循环配置（`macros`）在「循环连发」页独立管理。

按键绑定和循环步骤均使用 `name` 保存可选别名，保存前会去除首尾空白；别名随方案复制、分享码和文本配方导入导出，仅用于列表、分享预览和悬浮窗展示。

循环步骤的 `action` 为 `click`、`hold`、`down`、`up`、`wait` 或 `wheel`；`durationMs` 保存保持／等待时长，`wheelDelta` 用正负 120 的倍数保存方向和格数，`delayAfterMs` 保存动作后间隔。文本配方的步骤格式为 `动作/按键/时长/间隔/滚轮量`，例如 `hold/K:81/500/10/120`、`wait/-/500/0/120`，前缀 `!` 表示停用，后缀 `@x,y` 保存点位。

按键字段使用 `K:<虚拟键码>`、`M:<鼠标键码>`；组合键追加 `+Ctrl`、`+Shift`、`+Alt`，例如 `K:49+Ctrl` 表示 `Ctrl+1`，`M:4+Shift` 表示 `Shift+中键`。旧的单键格式仍按无修饰键读取。

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

按键连发和循环连发不是页面级或配置级互斥。`InputDriverRouter` 按实际运行状态为每个按键登记来源和激活顺序：

- 双方同时申请同一个按键时，先申请的一方保留所有权；互不相同的按键并行发送
- 所有权释放或转移时，路由器会先补发原来源的抬键事件，避免按键卡住
- 只是在两个页面配置相同按键不会冲突；双方实际运行且同时申请该按键时才触发仲裁

### 物理按键与注入事件的区分

小叽/DD 驱动发送的按键可能不携带 `LLKHF_INJECTED` 标志，优先通过 Raw Input 设备身份及父节点过滤自身输出，钩子同时跟踪自身报告事件；现有引擎还保留以下状态追踪：

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
│   ├── dd/                 # DD 驱动运行库（DD32.dll / DD64.dll）
│   ├── xiaoji/             # 小叽驱动传输与管理源码、许可证及来源说明
│   └── miniz/              # zip 解压库（更新功能使用）
└── scripts/                # 构建、打包脚本与本地输入查看工具
```

## 参与贡献

欢迎通过 Issue 反馈问题、提交 Pull Request 改进代码。提交前请：

- 遵循现有代码风格（分层结构、coordinator 模式、UI 规范见 [AGENTS.md](AGENTS.md)）
- 界面改动请同时验证浅色/深色模式和 100%/125%/150%/200% DPI 缩放
- 保持"软件更新仅在用户手动触发时联网"的原则

## 开源协议

- 本项目代码以 [GPL-3.0](./LICENSE) 协议开源
- `third_party/xiaoji/`：派生自 [XiaojiDriver](https://github.com/ZyphrZero/XiaojiDriver) 与其上游 [HIDMaestro](https://github.com/hifihedgehog/HIDMaestro)，保留 [AGPL-3.0](third_party/xiaoji/LICENSE-AGPL-3.0) 与 [MIT 许可证及版权声明](third_party/xiaoji/LICENSE-HIDMaestro-MIT)；组合发布须提供相应源码并遵守许可证要求。
- `third_party/miniz/`：miniz，公有领域 / MIT
- `third_party/dd/` 下的 DD 驱动运行库为第三方闭源组件，版权归其原作者所有，仅作为运行时依赖分发

## 免责声明

1. 本软件仅供学习和交流使用
2. 使用本软件产生的一切后果由用户自行承担
3. 作者不对软件使用造成的任何损失负责
4. 禁止使用本软件进行任何违法活动
5. 部分杀毒软件可能误报，请自行添加信任
