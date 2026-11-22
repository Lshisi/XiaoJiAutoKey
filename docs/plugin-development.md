# 插件开发

框架保留连发与循环引擎、输入路由、热键、配置和主题。插件可组合原生驱动、QML 页面、常驻后台和独立窗口。后台生命周期、窗口及页面与后台通信见 [插件运行时](plugin-runtime.md)，按需补读。SDK、官方插件和目录服务共用本仓库；第三方可自行选择单仓库或多插件仓库。

下方示例命令供开发者自行执行。构建、打包和运行遵循 [项目入口](../AGENTS.md) 的授权约定，线上目录配置见 [插件发布](plugin-release.md#线上配置与首次初始化)。

## 包目录与清单

zip 根目录直接包含 `plugin.json`，不包额外的顶层文件夹。规范入口为 [SDK 清单 schema](../sdk/plugin.schema.json)，实际文件存在、路径包含、身份和兼容性校验由宿主完成。

| 字段 | 约定 |
| --- | --- |
| `schemaVersion` / `apiVersion` | 当前均为整数 `1`，原生 ABI 常量为 `0x00010000` |
| `id` | 3–96 个小写字母、数字、点和连字符，不含连续点；公共自动发现使用 `仓库所有者小写名称.插件名`；`xiaoji.` 为官方保留 |
| `version` | 正式三段版本，无前导零、预发行后缀；与宿主和固件版本分别管理 |
| `minHostVersion` / `maxHostVersion` | 最低版本必填，最高版本可选；首版 SDK 最低宿主为 `1.1.8` |
| `platforms` | 首版包含 `windows-x64` |
| `name` / `author` / `description` | 用户可读、非空，长度上限分别为 80、120、2000 字符 |
| `native` | 可选的 DLL 相对路径；声明驱动时必填 |
| `drivers` | 最多 16 个提供方，每项有唯一的本插件内 `id` 和名称；完整 ID 为 `插件ID/提供方ID`；可选 `diagnostics`、`selfCheck` 布尔值声明诊断和自检入口，默认不显示 |
| `pages` | 最多 8 个页面，每项有本插件内 `id`、标题、QML 相对路径和共用图标名；路由为 `plugin:插件ID/页面ID` |
| `background` | 后台 QML `file`、原生后台 `native: true`，或二者组合；首次安装不自动运行 |
| `windows` / `commands` | 最多 8 个托管窗口、32 个插件内部命令，详见运行时文档 |
| `resources` | 打包时额外收集的资源文件或目录，例如 `ui`、`driver`；QML 使用到的辅助组件和图片目录也应列入 |
| `management` | 可选 `install`、`repair`、`uninstall`，指定包内 EXE、参数数组和确认说明；可先在插件详情安装/修复，再在驱动页选择和管理 |

所有路径必须位于插件版本目录内，不接受绝对路径、`..`、链接、Windows 保留文件名或大小写冲突。包不得携带用户 `config.json`、缓存、构建目录或密钥。当前上限为 zip 64 MB、解压总量 256 MB、单文件 64 MB、4096 个条目。

审核、官方身份、上下架及禁用来自可信目录，写入插件清单不能自行获得这些身份。下载安装时核对整个 zip 的 SHA-256；审核仅绑定该包及版本。随应用构建的预装副本显示“随程序提供”，不会自动声称通过某个独立 zip 的审核。首版没有每次调用前重新校验磁盘文件或防止本机高权限修改的机制。

## 公开能力

| 权限 | API 或作用 |
| --- | --- |
| `driver.provide` | 注册 `drivers` 与原生驱动接口 |
| `device.identify` | 声明 `deviceTokens`，供设备发现与自身输出过滤；条件至少 5 字符，不能填写泛化的所有键盘/鼠标身份 |
| `device.serial` | 声明串口通信用途，通信细节由插件处理 |
| `ui.page` | 注册 QML 页面 |
| `runtime.background` | 声明常驻后台，用户启动后独立于页面运行 |
| `ui.window` | 声明独立窗口，使用框架窗口管理接口 |
| `messaging.own` | 本插件页面、窗口、QML 或原生后台间的命令、回复和事件 |
| `engine.read` | 页面读取公开运行状态 |
| `engine.control` | 页面请求连发或循环启停；宿主仍执行现有运行条件与热键保护 |
| `settings.own` | 页面保存自己名下的设置 |
| `log.write` | 使用带插件 ID 的宿主日志入口 |
| `system.driver.manage` | 在用户确认后运行包内管理工具；声明此权限不会自动信任证书或安装驱动 |

这些声明用来公开用途和约束框架 API。原生 DLL 与 QML 都在本机进程中执行，首版没有安全沙箱；应只安装用户信任的插件。

## 原生驱动

唯一跨 DLL 协议为 [api.h](../sdk/include/xiaoji/plugin/api.h)。导出 `extern "C" XiaojiPluginGetApi`，返回静态 `XjPluginApi`。所有字符串为 UTF-8，结构带 `size`，内存由其创建方释放，不跨边界传递 Qt、STL、C++ 虚表或异常。

驱动加载时宿主核对 ABI、插件 ID 与清单版本，依次调用 `initialize`、`create_driver`、`connect`。原生后台或命令也可独立初始化同一模块，无驱动时 `create_driver` 可以为空。结构末尾的后台、命令与宿主回复/事件回调为可选扩展，通过 `XJ_HAS_FIELD` 检查大小。`xiaoji_add_plugin` 从清单生成 `XIAOJI_PLUGIN_ID` 和 `XIAOJI_PLUGIN_VERSION` 宏，避免二进制与清单版本不一致。必需的驱动回调为销毁、连接、断开、就绪、能力、发送、释放全部按键和文本查询；其余回调可选。

`send` 必须处理声明支持的动作；不支持的键位应通过 `XJ_TEXT_KEY_SUPPORT` 提前说明。`XJ_RESTORE_PHYSICAL` 恢复物理状态，不应增加插件合成按键的所有权。`release_all`、`release_source` 和 `clear_states` 必须按输入来源保留现有释放语义。`get_text` 返回包括末尾 NUL 的所需字节数，支持空缓冲区查询；宿主会限制文本长度。

发送和释放可来自引擎线程，自注入识别可来自钩子线程，心跳和设备变化可来自健康检查线程。实现必须处理并发；不要在钩子回调中做 UI、联网、长等待或系统安装。管理操作前宿主停止输出，停用后不继续发送，驱动适配器和 DLL 保留至进程退出，避免释放仍被钩子引用的对象。

声明 `XJ_CAP_PAUSE_MODIFIER_RELAY` 的提供方还须实现 `supports_physical_key` 快速查询，使框架在接管物理输入前确认能重放该键。此回调直接位于钩子路径，不做 I/O、等待或引擎锁；未实现时不接管该物理按键。

`XJ_CONTROL_HEARTBEAT` 和 `XJ_CONTROL_DEVICE_CHANGE` 是可选的健康通知；`drivers[].heartbeatMs` 可设为 250–60000，缺省使用框架轮询间隔。硬件输出应声明 `XJ_CAP_HARDWARE_OUTPUT`；鼠标移动、滚轮和修饰键隔离须按真实能力声明。

驱动管理页采用框架共用布局，只显示当前已安装、启用且可选的提供方，不再固定汇总小叽、DD、USB。诊断和自检入口由 `drivers[].diagnostics` / `selfCheck` 声明；这里提供框架统一的只读诊断及引导，新增专有页面仍使用 `pages`。已加载的驱动可通过 `get_text(XJ_TEXT_DIAGNOSTIC)` 补充自己的状态说明，`NativeInput.hpp` 对应 `DiagnosticText()`，应为快速、线程安全的文本查询，不执行系统安装、联网或 UI 操作。系统驱动操作按该插件的 `management` 声明显示，旧 HID 仅在检测到真实残留时提供兼容清理。

现有小叽、DD 的系统检测及安装/修复仍保留宿主兼容服务，卸载通过统一插件管理流程调用；它们的文件载荷已属于各自插件。第三方驱动不需要这些官方 ID 分支，使用公开输入、诊断文本和 `management` 接口。插件声明的包内管理工具优先于官方兼容处理，工具应只处理自己拥有的设备、服务及驱动包，不删除 Windows 自带或共用组件。

最小示例为 [native-driver](../sdk/examples/native-driver)。示例仅支持普通 Windows 键盘键，明确拒绝组合键和鼠标动作；正式提供方应实现自己的完整语义。官方后端在 `plugins/driver-dd/src/`、`plugins/driver-xiaoji/src/`、`plugins/driver-usb/src/`，其中 `NativeInput.hpp` 只用于 DLL 内部实现。目录与标识约定见 [项目目录](project-layout.md)。

独立构建示例与打包：

```powershell
& 'D:\software\cmake-3.31.6\bin\cmake.exe' -S sdk/examples/native-driver -B build-native-example -G 'Visual Studio 17 2022' -A x64
& 'D:\software\cmake-3.31.6\bin\cmake.exe' --build build-native-example --config Release --target XiaoJiNativeExamplePackage
./scripts/package-plugin.ps1 -SourceDirectory build-native-example/Release
```

`<目标名>Package` 刷新 DLL 以外的清单、资源和许可证，即使 DLL 无需重新链接也会收集最新文件。安装后的 SDK 可通过 `find_package(XiaoJiPluginSDK CONFIG REQUIRED)` 获取 `XiaoJi::PluginSDK` 与相同 CMake 函数，不依赖主程序 Qt。

## QML 页面

参考 [status-page](../sdk/examples/status-page)，无需原生模块。根对象为可显示的 `Item` 或其派生组件，声明 `required property QtObject pluginApi`。可 `import XiaoJi.UI` 复用 `Theme`、`QuietCard`、`QuietLabel`、`ActionButton`、输入框及滚动条；首版 UI 模块仅承诺本版公开的组件和主题入口，不暴露宿主私有页面模型。

| `pluginApi` 成员 | 内容 |
| --- | --- |
| `pluginId`、`theme` | 当前插件身份与框架主题 |
| `state` | `version`、`driver`、`repeatRunning`、`macroRunning` 等公开状态；需要读取或控制引擎权限 |
| `settings` / `saveSetting(key, value)` | 自己名下的 JSON 设置；最多 128 项，总计 64 KB，键最多 64 字符 |
| `navigate(pageId)` | 框架公开页面或本插件页面；跨插件私有路由不开放 |
| `requestEngine(macro, running)` | 请求启动/停止对应引擎，需 `engine.control` |
| `log(message)` | 最多 2000 字符的框架日志，需 `log.write` |
| `showMessage(title, message)` | 框架消息界面，显示插件来源；标题最多 80 字符，正文最多 2000 字符，已有插件操作时不弹出 |
| `requestConfirmation(requestId, title, message)` | 框架确认界面，通过 `confirmationFinished(requestId, accepted)` 返回；操作繁忙时返回拒绝 |
| `runtime` / `setBackgroundRunning(running)` | 本插件后台、窗口、命令与错误状态，及后台启停请求 |
| `request` / `reply` / `publish` | 插件内部命令与事件；通过 `requestFinished` 和 `eventReceived` 观察 |
| `showWindow` / `hideWindow` / `closeWindow` / `configureWindow` / `startWindowMove` | 操作本插件清单中的独立窗口 |

插件请求启动引擎还需经过一次明确的宿主确认，防止页面加载、安装或后台回调直接开始发送按键。框架消息和确认界面沿用现有主题、模态与热键保护。

页面由单独的 `QQmlEngine` 承载，停用、卸载或升级时移除入口并销毁页面。纯 QML 版本之间可立即替换；新旧任一版本包含原生模块，或进程仍加载过该插件的 DLL 时，整体等待重启，包括原生版本改为纯 QML 的情况，不混用新页面与旧原生模块。

页面引擎销毁不影响独立的插件运行实例。后台和窗口在另一引擎运行，共享本插件 API 上下文。普通页面无需声明后台；桌宠、原生后台通信示例及生命周期约定见 [插件运行时](plugin-runtime.md)。

页面示例打包可使用 `./scripts/package-plugin.ps1 -SourceDirectory sdk/examples/status-page`。QML 附属资源通过 `resources` 收集，不能依赖开发机绝对路径。正式与预览使用同一框架控件，预览只加载示例页面，不加载原生插件或执行网络请求。

## 安装与恢复

应用目录下：`plugins/builtin/<id>/<version>/` 是预装载荷，`plugins/installed/<id>/<version>/` 是用户安装的独立版本，`plugin-data/<id>/` 为插件自有数据，`plugin-cache/` 为目录和暂存缓存。插件设置及版本选择记录在用户 `config.json`，不会随插件更新被覆盖。

应用默认只部署 `xiaoji.driver.xiaoji`，内置视图仅包含小叽驱动；DD、USB 从插件中心按需下载，默认不安装，独立安装后可卸载。历史试用包遗留在 `plugins/builtin/` 的 DD、USB 不再作为有效预装来源自动加载；真实的 `plugins/installed/` 安装和用户设置继续保留。小叽可在详情中恢复随包版本，兼容性和有效策略仍然生效，涉及原生版本的恢复等待重启。

用户安装入口为插件中心下载和“导入”本地 ZIP；地址安装和目录设置已移除。安装确认展示实际下载源或本地文件绝对路径，匹配可信目录时另列来源仓库；实际来源随安装记录保存，并在详情展示。旧安装记录没有原始来源时显示本机载荷路径。

清单无效、入口缺失、宿主版本不兼容和已知安装文件丢失时，保留 ID、版本及失败原因供管理；该记录不注册页面、后台或驱动。小叽驱动插件固定保留、始终启用，不能由用户停用或卸载；历史关闭状态会恢复为启用，但开关恢复不触发系统驱动安装或启动引擎。DD、USB 及其他非内置插件允许停用和卸载，不再施加最后一个可用驱动的数量限制。兼容性、运行时故障和维护者禁用策略仍可阻止异常版本运行，不能为了保留内置开关而绕过这些检查。

历史配置的当前驱动为空时，重新启用有效驱动会恢复可选提供方和驱动页状态；停用当前驱动且仍有其他有效驱动时选取替代项。恢复选择只写配置和刷新界面，不自动连接、安装系统驱动或启动连发、循环。

涉及原生版本的更新先安装到新目录，记录待重启版本；重启前保留完整旧版，没有旧活动版本时不提前开放新页面或后台。下次加载前按驱动提供方、原生后台、原生通信入口分别登记激活尝试。后台成功不再要求插件没有驱动，同时也不替未使用的驱动确认成功；已确认的步骤持久化，首次使用其他步骤时仍保留回退能力。只有仍在执行的尝试计入激活中断，正常停止、停用和退出在后台完成清理后取消对应尝试。

可处理的首次加载失败或激活中断恢复旧版，已确认步骤的普通运行故障不直接当作首次激活失败；命中禁用策略的旧版不会执行。首版不保证从原生内存破坏中在同一进程内恢复。

官方 DD 在运行库存在但连接失败时恢复原有的 INF 探测、刷新和设备重建流程，慢操作在工作线程执行。官方 USB 心跳失败后先停止输出，再延迟尝试重连；重连成功不会自动重启连发或循环。健康检查结果绑定当前连接批次，过期结果不影响新连接。

无法正常启动时，可由用户手动运行 `XiaoJiAutoKey.exe --disable-third-party-plugins`：停用并保存非内置插件（包括独立安装的 DD、USB）选择，忽略已安装的小叽覆盖包，并明确启用、选择随包小叽副本；仍执行缓存的禁用策略。这是用户主动请求恢复时的操作，正常启动不重开已停用的非内置插件。条目保留在插件中心，排查后可主动恢复。

停用插件保留系统驱动。卸载提供系统卸载能力的插件时，确认框默认勾选“同时卸载此插件使用的系统驱动”，用户可取消勾选只移除插件。框架先停止输入输出、释放按键、关闭插件页面/后台并断开连接，再异步清理系统驱动，完成后才标记移除插件；原生文件在下次启动清理，设置保留。DD 接入现有专用驱动清理；当前 USB 插件没有自定义系统驱动安装步骤，只移除插件。小叽插件不能删除，但可独立卸载系统驱动，之后主动重新安装。

包内管理工具使用当前进程权限，退出码 `0` 表示完成，`3010` 表示 Windows 已接受操作、需要重启电脑；工具不得自行重启电脑。失败时保留已停用的插件、管理工具及失败原因；等待电脑重启时持久化 `removalPendingRestart`，保留文件并阻止启用、更新，用户重启电脑后点击“继续卸载”重新确认清理完成。仅重启小叽不会提前删除工具。清单无效或策略禁止执行的第三方工具不会运行，这类条目仍可只移除插件包。
