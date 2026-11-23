# 插件后台、窗口与通信

基础清单、驱动和页面见 [插件开发](plugin-development.md)，后台及窗口示例位于 `sdk/examples/`。构建、打包与运行遵循 [项目入口](../AGENTS.md) 的授权约定。

## 本版范围

同一个插件可组合驱动、主窗口页面、后台和独立窗口。页面入口与宿主页面同级显示，默认追加到左侧导航最下方，支持用户拖动排序，规则见 [左侧导航](ui-guidelines.md#左侧导航)。后台和窗口由独立的运行实例持有，切页、调整标签顺序、最小化主窗口不会销毁它们。小叽退出时全部结束，不创建脱离小叽继续运行的进程。

本版只补这三项及所需的插件内部命令和生命周期事件。全局快捷键注册、完整业务配置修改、剪贴板、联网代理服务、跨插件通信和进程沙箱不在本轮范围。

## 清单

| 字段 | 约定 |
| --- | --- |
| `background.file` | 后台 QML 相对路径，根对象使用 `QtObject`，声明 `required property QtObject pluginApi` |
| `background.native` | 为 `true` 时使用 `native` DLL 的后台启停接口，可和后台 QML 同时声明 |
| `windows` | 最多 8 项，声明 `id`、`title`、QML `file` 以及可选 `width`、`height`、`transparent`、`frameless`、`topmost` |
| `commands` | 最多 32 项，声明 `id`、`title`、`target`；目标为 `background`（默认，需后台 QML）或 `native`（需 DLL） |

后台至少声明 QML 或原生入口之一，申请 `runtime.background`；窗口申请 `ui.window`；命令和事件申请 `messaging.own`。这些权限用于声明用途和约束公开 API，不提供系统级隔离。

窗口 QML 根对象使用 `Item`，声明 `required property QtObject pluginApi` 和 `required property string windowId`；插件提供窗口内容，框架创建 `QQuickWindow`。页面和窗口内容根对象不直接使用 `Window`。

窗口初始宽高默认 320×240，声明范围为 32–2048 个逻辑像素，实际尺寸和位置受屏幕工作区约束。窗口、页面、驱动和命令 ID 分别在各自列表内唯一，使用现有本地 ID 格式。

后台和窗口 QML 入口由打包脚本自动收集；引用的其他组件、图片和资源仍须列入 `resources`。所有文件遵循原有路径包含、版本、哈希和包大小规则；目录服务同步检查新清单，GitHub / Gitee 的发现、发布和审核方式不变。

## 后台生命周期

首次安装后后台默认停止。用户在插件详情或插件自己的设置页主动启动后，框架保存 `backgroundEnabled`；下次启动小叽、重新启用插件或完成纯 QML 包更新时恢复。点击“停止后台”会取消恢复选择，并关闭该插件的托管窗口。

启用插件与运行后台是两个状态：可以只打开设置页。后台启动失败时保留页面和错误，取消自动恢复，避免启动时反复加载失败后台。恢复模式在加载前停用第三方插件。

| `pluginApi.runtime` 字段 | 内容 |
| --- | --- |
| `status` | `stopped`、`starting`、`running`、`stopping`、`error` |
| `backgroundRunning` | 是否成功进入 `running` |
| `error` | 后台或窗口操作错误 |
| `windows` | 声明的窗口及 `open`、`visible`、`clickThrough` 状态 |
| `commands` | 公开命令的 ID、标题和目标 |

通过 `runtimeChanged` 观察变化。`setBackgroundRunning(bool)` 返回值只说明请求是否被能力与参数检查接收，最终结果读取 `runtime`。

后台 QML 创建后依次发送 `lifecycleEvent("starting", reason)` 和 `lifecycleEvent("started", reason)`；后台可在 `started` 中显示窗口。停止时先发送 `stopping`，取消对应请求、关闭窗口、销毁后台 QML，原生后台结束后进入 `stopped`。后台销毁后，`stopped` 事件由仍在的设置页等接收。

常见原因是 `user`、`restore`、`disabled`、`blocked`、`updated`、`removed`、`management`、`exit`。停用、禁用、替换和退出关闭整个运行实例并使旧 API 失效；系统驱动管理工具运行前暂停实例，结束后按原选择恢复。

QML 后台在 GUI 线程运行，适合状态、短任务和普通定时器，不得阻塞事件循环。高精度调度、连发、循环和输入仲裁仍由框架负责；后台启动不代表授权自动启动按键引擎，原有启动确认继续生效。

## 独立窗口 API

| 接口 | 行为 |
| --- | --- |
| `showWindow(windowId)` | 创建或显示清单中的窗口，页面关闭后继续存在 |
| `hideWindow(windowId)` | 隐藏并保留内容，动画应根据窗口可见状态暂停 |
| `closeWindow(windowId)` | 销毁窗口内容，再打开时重新创建 |
| `configureWindow(windowId, options)` | 修改已打开窗口的 `x`、`y`、`width`、`height`、`topmost`、`clickThrough` |
| `startWindowMove(windowId)` | 在拖动手势中请求系统移动窗口，使用逻辑坐标 |

只能操作本插件声明的窗口。除拖动外，窗口操作延迟到当前 QML 回调结束后执行，避免同步销毁发起操作的对象；最终状态与错误通过 `runtime` 返回。

框架单独保存位置、尺寸、屏幕、置顶和鼠标穿透设置，不占用插件 JSON 设置额度；显示时恢复，屏幕移除、工作区或 DPI 变化后重新约束。位置保存合并写入。

窗口不会仅因上次打开过就自动出现，由用户再次打开或后台在 `started` 中显示。详情提供打开、关闭和“恢复交互”，鼠标穿透不妨碍从主界面恢复控制。

## 页面与后台通信

页面、后台和窗口共用本插件 API 上下文，使用各自的 QML 对象。通信只在本插件内路由，不接受其他插件 ID。

| 接口 / 信号 | 行为 |
| --- | --- |
| `request(command, payload = {})` | 调用清单命令，返回唯一请求 ID；无权限、参数无效或超速返回空字符串 |
| `commandReceived(requestId, command, payload)` | 后台 QML 接收发给 `background` 的命令 |
| `reply(requestId, ok, result = {}, error = "")` | 后台 QML 回复 |
| `requestFinished(requestId, ok, result, error)` | 页面或窗口按自己保存的请求 ID 匹配结果 |
| `publish(topic, payload = {})` | 发布本插件事件 |
| `eventReceived(topic, payload)` | 页面、窗口和后台观察本插件事件 |

请求异步分派，调用方能先保存 ID，再收到结果。后台 QML 命令要求后台已运行；声明 `background.native` 的原生命令也要求后台运行。没有原生后台的驱动或工具可以按需加载 DLL 处理原生命令，不自动连接输入驱动。

负载和结果都是 JSON 对象，每份最多 64 KB；每实例最多 32 个在途请求，超时 10 秒；QML 消息入口和原生回调各限制每秒 128 次。事件主题最长 80 字符，由小写字母、数字、点、下划线或连字符组成。晚到或重复回复被忽略；后台停止取消对应请求，实例关闭取消全部请求。

共享设置继续使用 `settings`、`saveSetting` 和 `settingsChanged`。不要为每帧动画或每次按键发送消息；输入仍走直接 C ABI 调用。

## 原生后台与命令

`api.h` 在 ABI 1.0 结构末尾增加可选成员，原前缀和 ABI 常量不变。宿主接受旧结构前缀；插件通过 `XJ_HAS_FIELD` 检查宿主大小再使用新回调。

| 结构 | 新成员 |
| --- | --- |
| `XjPluginApi` | `start_background`、`stop_background`、`handle_request` |
| `XjHostApi` | `reply`、`publish_event` |

原生后台须配套实现启停回调，没有驱动时 `create_driver` 可以为空。初始化、驱动实例和后台共用同一版本模块；只打开页面不会主动加载 DLL。模块和宿主回调上下文保留至进程退出。

后台和命令在插件专属工作线程串行调用；输入与钩子回调仍可能并发，插件须协调共享数据。`handle_request` 返回 `XJ_OK` 表示接收，随后通过宿主 `reply` 回复一次；其他返回值由框架转为失败。事件和回复可来自插件线程，由框架复制数据并转交 GUI 线程。

UTF-8 JSON 缓冲区仅在调用期间有效，不转移内存所有权；异步处理须复制。`stop_background` 返回前须结束任务并等待自建线程退出，不能同步等待 UI，不能误销毁驱动仍使用的资源。首版不保证强制停止进程内代码或从原生内存破坏中恢复。

新旧任一版本包含 DLL，或进程保留该插件已加载的模块时，整体等待重启，包括旧版原生、新版纯 QML；新版本页面不会提前混入旧驱动。纯 QML 版本之间的页面、后台和窗口一起替换。新增原生 QML 类型仍需框架支持，不跨 DLL 传递 Qt 对象。

激活恢复按驱动提供方、原生后台和原生通信入口分别记录。混合插件只使用后台或命令时，也能正确结束对应的进行中标记，不要求先选择它的驱动；未使用的驱动保留首次连接失败回退能力。正常退出会在后台工作线程结束后取消未完成尝试，避免误报激活中断。细则见 [安装与恢复](plugin-development.md#安装与恢复)。

## 示例与预览

- [desktop-pet](../sdk/examples/desktop-pet)：纯 QML 桌宠、设置页、后台计时、命令回复和事件、透明窗口与拖动，首次安装后主动启动后台。
- [native-service](../sdk/examples/native-service)：不提供驱动的原生后台，展示计数命令、C ABI 回复和事件，以及 QML 页面。
- 原有 [status-page](../sdk/examples/status-page) 和 [native-driver](../sdk/examples/native-driver) 继续使用。

预览加载资源内的桌宠 QML，使用正式运行时和窗口承载；启停、窗口位置和设置仅在预览内存保存，不加载 DLL、不读取用户配置、不联网或发送按键。本轮未启动预览。
