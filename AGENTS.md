# 打包规则

## 配置文件

- `config.json` 是运行时生成的用户配置文件，存放在 exe 同级目录。
- 打包 zip 时绝对不能包含 `config.json`。
- 打包脚本 `scripts/package-release.ps1` 中已有清理逻辑，确保 `config.json` 不会被打入发布包。

## 发布包内容

发布包只应包含：

- `XiaoJiAutoKey.exe` -- 主程序
- `使用说明.mhtml` -- 离线使用说明
- `audio/` -- 内置提示音文件
- `driver/` -- DD/HID 运行库（DD32.dll / DD64.dll / hid/ddhid.63340.dll）

不应包含：

- `config.json` -- 用户配置（运行时自动生成）

## 构建注意事项

- 当前环境里 `cmake` 可能不在 `PATH`，优先使用 VS BuildTools 自带的 CMake：
  `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
- 如果 MSBuild 报 `System.ArgumentException: 已添加项。字典中的关键字:“Path”所添加的关键字:“PATH”`，说明当前进程环境里同时存在 `Path` / `PATH`。构建前先在当前 PowerShell 进程清掉重复项并设置单一 `Path`。
- 现使用 Qt 6.8+（QML / Quick / Quick Controls / Quick Widgets / SVG / Shader Tools）。指定实际静态 Qt 安装目录，使用独立的 `build-qt6`，不要复用旧 Qt 构建缓存。推荐构建命令：

```powershell
[System.Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[System.Environment]::SetEnvironmentVariable('Path', 'C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem;C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin', 'Process')
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -S . -B build-qt6 -G 'Visual Studio 17 2022' -A x64 -DCMAKE_PREFIX_PATH='C:\Qt\6-static'
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-qt6 --config Release --target XiaoJiAutoKey
```

# 发布流程（线上更新）

项目开源在 `gitee.com/fourteen-fourteen/xiao-ji-auto-key`（GPL-3.0）。程序内"软件更新"功能的数据源是 Gitee Releases（基于 git tag），更新逻辑见 `src/app/services/UpdateService.cpp`。发布新版本时：

1. 提升 `CMakeLists.txt` 里 `project(XiaoJiAutoKey VERSION x.y.z)` 的版本号（`APP_VERSION` 宏由它生成）。README 不写版本号和版本历史，版本信息只存在于 git tag 和 Gitee Release 的发布说明里。
2. 用 `scripts/package-release.ps1` 打包（产物为 `dist/XiaoJiAutoKey.zip`，内部有顶层 `XiaoJiAutoKey/` 目录）。
3. 在仓库打 tag `vx.y.z`（如 `v1.1.6`，必须带 `v` 前缀，与 CMake 版本号一致）。
4. 在 Gitee 上基于该 tag 创建"发行版（Release）"，并把 `XiaoJiAutoKey.zip` 重命名后（建议 `XiaoJiAutoKey-vx.y.z.zip`）作为附件上传。附件的下载地址形如 `.../releases/download/...`，程序只把这种附件当作可用的更新包；Gitee 自动生成的源码包（`/archive/refs/tags/...`）会被忽略。
5. 更新包约束：必须是 zip；顶层目录（如有）会被自动剥掉；`config.json` 永远不会被更新包覆盖；PowerShell 5.1 生成的反斜杠路径条目和 GBK 文件名条目都能被解压逻辑兼容。

注意事项：

- 更新检查**只在用户于关于页手动点击"检查更新"时发生**，启动、运行、关机都不能触发联网检查；首页"有新版本"标签只读取 `config.json` 里的 `latestKnownVersion`，不允许为了它发请求。
- 不做强制弹窗、不做强制更新；所有更新动作（检查、下载、重启安装）都由用户主动点击触发。
- 程序内更新流程：下载 zip 到 `%TEMP%\XiaoJiAutoKeyUpdate` → miniz 解压校验 → 用户点"立即重启更新" → 停引擎、卸载驱动 → 运行中的 exe 改名为 `XiaoJiAutoKey.old.exe` → 覆盖安装目录 → 以 `--restart-wait` 拉起新实例并退出。新实例启动时会自动清理 `.old.exe` 和暂存目录。

# UI 规则

## 已确认的视觉与交互方向

- 在原有界面上精修，不重排用户熟悉的操作顺序：左侧功能导航，中间配置与按键列表，右侧快捷控制、启动触发和设置；启动/停止始终保留在右侧。
- 整体以安静、清晰、耐看为目标。使用中性背景和明确的信息层级，不增加银杏叶背景、门派纹样铺底、粒子、光晕、大徽记或无功能的装饰文案。
- 门派身份只通过小尺寸徽记、导航选中态和主要操作的强调色表达，不对整个页面、表格和所有卡片进行门派色染色。
- 玻璃材质只克制地用于侧栏和右侧快捷控制区，使用细边界、弱高光与轻微通透；按键列表、输入框和参数内容区保持实底。不要堆叠模糊层、强阴影或持续运行的装饰动画。
- 保留合理留白，不为填满页面添加卡片、统计数字或重复状态。标题、正文和辅助说明保持清晰的三级层次，辅助文字仍需满足深浅色模式下的可读性。

## 按钮与控件

- 使用统一的组件和设计 token，不在各个页面分别手写一套按钮样式。普通按钮建议最小高度 36、右侧启动/停止按钮最小高度 40、图标按钮最小触控区域 32，均为逻辑像素；文字或 DPI 需要更多空间时允许增高，不固定裁切内容。
- 按钮与输入框圆角统一以 8 个逻辑像素为起点，卡片圆角以 13 个逻辑像素为起点；调整时修改统一 token，不将所有控件做成胶囊形，也不随意混用不同圆角。
- 区分主要操作、次要操作和图标操作：右侧启动/停止为主要操作，新增和保存等动作按当前任务的重要性设置层级，不让一组按钮中的所有动作都使用高饱和填充。
- 运行中的停止操作仍位于原启动位置，不移动、不隐藏。hover、pressed、disabled 和键盘焦点必须清晰且克制，不能仅用颜色变化表达运行状态。
- 图标按钮必须提供明确的 tooltip 和可访问名称；删除等危险操作不要紧贴主要操作，沿用现有确认与安全措施。
- 按钮组、表单行和卡片遵循下方已有间距与对齐规则，避免为装饰增加嵌套容器。

## QML 技能

- 项目级 `qt-qml` 技能入口为 `.agents/skills/qt-qml/SKILL.md`，来源为 `https://github.com/theqtcompanyrnd/agent-skills`。进行 QML 源码编写或修改前先阅读该技能，并以用户要求和本项目规则为优先。
- 安装技能不代表已经迁移 Qt 版本或 UI 架构；不要为安装技能附带启动应用、预览服务或修改业务与按键引擎。

## 深色模式

- 所有页面、滚动区域、滚动视口、卡片、输入框、表格、弹窗和状态区都必须兼容系统深色模式。
- 页面背景必须使用 `Fluent::Colors` / `FactionTheme::palette()` 里的深色模式感知颜色；QML 通过 `MainWindowView::theme` / `Theme.qml` 读取这些颜色，不能硬编码白色、浅灰或只适合浅色模式的背景。
- `QScrollArea` 不只设置自身透明，还要确认 `viewport()` 和内部 content widget 不露出默认 `QPalette::Base` 白底。
- 新增页面或主题刷新入口时，必须接入 `MainWindowView::updateFactionVisuals()` / `refreshRuntimeVisuals()`，确保切换门派、系统主题或运行时刷新后不会残留旧颜色。
- 原生滚动容器（弹窗、文本框等）的滚动条必须使用 `Fluent::Styles::scrollBarQss()` 统一样式；QML 滚动容器统一使用 `QuietScrollBar.qml`。两者配色均来自 `Fluent::Styles::scrollBarHandle()` / `scrollBarHandleHover()`，禁止各处手写独立滚动条或露出系统原生滚动条。
- 文本、边框、分割线、hover、selected、disabled 状态必须同时检查浅色和深色模式的对比度。

## 布局和间距

- 页面外边距统一优先使用 `Fluent::Spacing::pageMargin` 和 `Fluent::Spacing::cardGap`。
- 卡片内边距统一优先使用 `Fluent::Spacing::cardPadding`，不要在同一页面里随意混用不同 padding。
- 卡片之间的间距、表单行间距、按钮组间距要使用已有 spacing token 或同页面已有模式，避免视觉跳动。
- 卡片边缘必须对齐：同级卡片左/右边界、标题、主要输入控件和操作按钮要按同一网格对齐。
- 卡片内不要嵌套无必要的卡片；确实需要内层容器时，必须保证层级、边框和背景在深浅色模式下都清晰。
- 文本必须能在中文、长配置名和窄窗口下换行或省略，不允许压到按钮、滚动条或相邻控件上。

## DPI 和窗口缩放

- 固定尺寸只用于图标按钮、标题栏、状态栏等稳定控件；原生内容区域使用 `QSizePolicy`、stretch 和布局管理器，QML 内容区域使用 `Layout.*`、implicit size 与最小/最大尺寸自适应。
- 新增或修改布局后，必须考虑 100%、125%、150%、200% DPI 缩放，以及窗口在不同屏幕间移动后的布局刷新。
- 不要用裸像素假设屏幕大小。窗口尺寸、最小尺寸和位置约束应走现有 `MainWindowView` 的屏幕/DPI 处理逻辑。
- 表格列宽、卡片高度、文本编辑区高度等固定值要确认高 DPI 下不会裁切文字、按钮或滚动条。
- 主题刷新或 DPI 变化后，相关控件应调用已有 refresh/update 方法，必要时 invalidate/activate layout，避免旧尺寸缓存导致错位。
