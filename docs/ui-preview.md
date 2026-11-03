# 界面预览

预览程序独立于 Windows 按键引擎，可在 Mac 和 Windows 上使用 Qt 6.8.3 构建。
直接加载正式程序的 `AppShell.qml`、全部页面和通用控件，并使用原有图标、门派素材和主题。
不同平台使用各自的 Qt SDK 和启动脚本。

## 可以查看的内容

- 左侧导航中的按键连发、循环连发、使用统计、驱动管理、全局设置、关于软件六个主页面。
- 使用统计中的最近 270 天热力图和全年月度趋势；趋势支持独立切换年份及悬停查看时长，已完成月份用实线连接，本月累计段用虚线显示，未来月份留空。
- 门派切换、深浅色、示例数据和空数据，以及启动/停止、控件禁用等展示状态。
- 添加/编辑按键、按键别名、批量修改参数、配置新增/复制/重命名/删除、快捷键、提示音、悬浮窗设置、游戏进程、自检结果、诊断信息和分享、消息弹窗。
- 关于页中的新手引导；窗口下方的「其它窗口预览」可打开关闭确认、导入配置、更新下载、分享提示和坐标拾取界面。
- 在右侧开启「显示到浮窗」后可查看悬浮窗、拖动及锁定状态。
- 按键和循环步骤示例包含别名；编辑弹窗可修改或清空名称，列表将实际键位与名称分列显示，未命名显示「—」，长名称省略并可悬停查看全文；悬浮窗显示「名称（实际键位）」。
- 循环步骤可试用插入、单步复制、拖动排序、勾选多选、表头全选、批量修改、批量复制和批量删除。
- 两种列表以方框选择批量操作的行，以操作列前的“启用”开关控制启用或停用；均支持批量修改与删除，循环列表另支持批量复制。行内删除使用垃圾桶图标，批量删除使用文字按钮并显示数量确认。
- 循环列表下方可查看单轮与指定轮数的预计时长。
- 驱动管理页的“诊断信息”展示示例报告；正式程序的复制内容由用户主动点击触发。

所有演示状态只保存在预览进程内存中，不读写 `config.json`。
启动/停止仅切换界面状态，驱动、自启、音效、分享及更新操作使用示例数据，不执行系统操作或发起网络请求。
快捷键录制只读取预览窗口内的输入，不安装全局钩子。
预览弹窗与正式程序保持应用模态，并直接使用 `DialogShell.qml` 的背景和主题边框，避免 Mac 将弹窗作为原生 sheet 绘制额外外观。

页面、弹窗、控件和图标均在正式程序的共用文件中维护，预览不保留另一套样式。
修改模型字段、页面入口或交互时，需要同步更新 `preview/` 中的示例适配，维护规则见 `AGENTS.md`。

## Mac

需要 Xcode 开发工具、CMake，以及 Qt 6.8.3 的 Core、Gui、QML/Quick、Quick Controls、SVG 和 Shader Tools。
本机 Qt 安装目录是 `~/Qt/6.8.3/macos`。

安装 Qt 的命令：

```bash
python3 -m venv ~/.local/share/xiao-ji-auto-key/qt-installer
~/.local/share/xiao-ji-auto-key/qt-installer/bin/python -m pip install aqtinstall
~/.local/share/xiao-ji-auto-key/qt-installer/bin/python -m aqt install-qt \
    mac desktop 6.8.3 clang_64 --outputdir ~/Qt \
    --archives qtbase qtdeclarative qtsvg --modules qtshadertools
```

在仓库目录执行：

```bash
./scripts/preview-macos.sh
```

首次执行会编译预览程序，产物为 `build-qml-preview/XiaoJiPreview.app`。
仅编译而不打开窗口：

```bash
./scripts/preview-macos.sh --build-only
```

脚本默认使用本机已安装的 `/Library/Developer/CommandLineTools/SDKs/MacOSX14.4.sdk`。
Qt 6.8.3 的 AGL 链接依赖需要使用保留该框架的 SDK。
Qt 或 SDK 位于其他位置时，分别通过 `QT_PREFIX_PATH`、`MACOS_SDK_PATH` 指定。

## Windows

使用项目已有的动态 Qt 6.8.3 和官方独立版 CMake：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\preview-windows.ps1
```

脚本默认使用 `D:\Qt\6.8.3\msvc2022_64` 和 `D:\software\cmake-3.31.6\bin\cmake.exe`。
可用 `-QtPrefixPath`、`-CMakePath` 指定实际安装位置；`-BuildOnly` 表示仅编译。
产物为 `build-qml-preview-windows\Release\XiaoJiPreview.exe`。

控件与导航图标使用项目自带 SVG，正式程序和预览使用同一套资源。
Mac 与 Windows 的正文字体和系统材质仍存在差异；正式按键、驱动与更新流程在 Windows 程序中运行。
