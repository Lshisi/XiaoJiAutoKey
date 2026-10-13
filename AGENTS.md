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
- 推荐构建命令：

```powershell
[System.Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[System.Environment]::SetEnvironmentVariable('Path', 'C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem;C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin', 'Process')
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Release --target XiaoJiAutoKey
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

## 深色模式

- 所有页面、滚动区域、滚动视口、卡片、输入框、表格、弹窗和状态区都必须兼容系统深色模式。
- 页面背景必须使用 `Fluent::Colors` / `FactionTheme::palette()` 里的深色模式感知颜色，不能硬编码白色、浅灰或只适合浅色模式的背景。
- `QScrollArea` 不只设置自身透明，还要确认 `viewport()` 和内部 content widget 不露出默认 `QPalette::Base` 白底。
- 新增页面或主题刷新入口时，必须接入 `MainWindowView::updateFactionVisuals()` / `refreshRuntimeVisuals()`，确保切换门派、系统主题或运行时刷新后不会残留旧颜色。
- 所有滚动容器（页面、弹窗、文本框、表格、列表）的滚动条必须使用 `Fluent::Styles::scrollBarQss()` 统一样式（`src/ui/styles/DesignTokens.h`，配色用 `Fluent::Styles::scrollBarHandle()` / `scrollBarHandleHover()`），禁止各处手写滚动条 QSS 或露出系统原生滚动条。
- 文本、边框、分割线、hover、selected、disabled 状态必须同时检查浅色和深色模式的对比度。

## 布局和间距

- 页面外边距统一优先使用 `Fluent::Spacing::pageMargin` 和 `Fluent::Spacing::cardGap`。
- 卡片内边距统一优先使用 `Fluent::Spacing::cardPadding`，不要在同一页面里随意混用不同 padding。
- 卡片之间的间距、表单行间距、按钮组间距要使用已有 spacing token 或同页面已有模式，避免视觉跳动。
- 卡片边缘必须对齐：同级卡片左/右边界、标题、主要输入控件和操作按钮要按同一网格对齐。
- 卡片内不要嵌套无必要的卡片；确实需要内层容器时，必须保证层级、边框和背景在深浅色模式下都清晰。
- 文本必须能在中文、长配置名和窄窗口下换行或省略，不允许压到按钮、滚动条或相邻控件上。

## DPI 和窗口缩放

- 固定尺寸只用于图标按钮、标题栏、状态栏等稳定控件；内容区域应使用 `QSizePolicy`、stretch、最小/最大尺寸和布局管理器自适应。
- 新增或修改布局后，必须考虑 100%、125%、150%、200% DPI 缩放，以及窗口在不同屏幕间移动后的布局刷新。
- 不要用裸像素假设屏幕大小。窗口尺寸、最小尺寸和位置约束应走现有 `MainWindowView` 的屏幕/DPI 处理逻辑。
- 表格列宽、卡片高度、文本编辑区高度等固定值要确认高 DPI 下不会裁切文字、按钮或滚动条。
- 主题刷新或 DPI 变化后，相关控件应调用已有 refresh/update 方法，必要时 invalidate/activate layout，避免旧尺寸缓存导致错位。
