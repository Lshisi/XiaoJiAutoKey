# XiaoJiPluginSDK

小叽按键插件协议与开发工具。`include/xiaoji/plugin/api.h` 是原生插件和宿主之间唯一的二进制协议，使用 C ABI、UTF-8 和调用方拥有的内存。

原生提供方不链接宿主的 Qt，也不跨 DLL 传递 C++ 虚表、STL 或 Qt 对象。插件 API 为 1.x，插件自己的版本与小叽应用版本分别管理。

SDK 位于主仓库 `sdk/`，官方驱动位于 `plugins/driver-xiaoji/`、`plugins/driver-dd/`、`plugins/driver-usb/`。各模块使用同一 SDK，不复制 ABI 头文件。源码同仓库维护，各插件分别构建和分发；外部开发者也可以单独获取 SDK 安装包。

应用默认只随包提供小叽驱动，该插件固定保留、始终启用；DD、USB 是可自由停用和卸载的可选下载。客户端保留插件中心安装和“导入”本地 ZIP，不提供地址安装或目录设置入口。

## 清单

插件包根目录必须有 `plugin.json`。`schemaVersion` 为 1，`id` 唯一，`version` 为正式语义版本，`apiVersion` 为 1，`minHostVersion` 声明最低宿主版本。`native` 是相对包目录的 DLL 路径，`drivers` 声明提供方，`pages` 声明页面；`background` 声明 QML / 原生后台，`windows` 声明托管窗口，`commands` 声明插件内部命令。

仅页面入口支持 `pages[].icon`，可填写宿主共用图标名，或包内 SVG / PNG 相对路径，例如 `icons/status.svg`。宿主按透明度轮廓统一着色，打包脚本自动收集引用文件；声明和素材要求见 [QML 页面](../docs/plugin-development.md#qml-页面)。

审核、官方身份、上下架和禁用策略来自平台目录，不能写进插件清单自行授予。审核绑定版本与包哈希，新版本默认为未审核。

完整规范见 [plugin.schema.json](plugin.schema.json)、[开发文档](../docs/plugin-development.md)、[后台、窗口与通信](../docs/plugin-runtime.md) 和 [发布文档](../docs/plugin-release.md)。示例包括 `examples/status-page/`、`native-driver/`、`desktop-pet/` 和 `native-service/`。

## 生命周期

宿主校验清单后加载导出 `XiaojiPluginGetApi`，核对插件身份、版本和 ABI，再调用 `initialize`、`create_driver`。驱动通过 `connect` 激活；停用先停止输出、释放按键、断开连接，等待工作线程与钩子调用结束后销毁。原生模块替换在重启时进行。

输入调用可能来自工作线程，自注入识别可能来自钩子线程。不得从这些回调访问 UI、执行联网或等待宿主事件循环。所有导出回调不得向宿主抛出异常。

后台默认停止，用户启动后记住恢复选择；切页或最小化不停止，停用、更新或退出统一清理。无驱动的原生后台允许 `create_driver` 为空，使用可选 `start_background` / `stop_background`。

ABI 1.0 保持原结构前缀；新增 `handle_request` 与宿主 `reply` / `publish_event`，通过 `XJ_HAS_FIELD` 检查后使用。后台和命令在专属工作线程串行调用，不同步等待 UI；输入/钩子回调仍可能并发。详见运行时文档。

`NativeInput.hpp` 是内置驱动迁移所用的 C++ 实现辅助，不是跨 DLL 的 C++ ABI。`get_text` 返回包含结尾 NUL 的 UTF-8 所需字节数，宿主先查询长度，再提供缓冲区。

驱动页只显示当前插件。`drivers[].diagnostics`、`selfCheck` 声明框架诊断/自检入口；已加载驱动使用 `XJ_TEXT_DIAGNOSTIC` 补充只读说明。包内 `management` 声明安装、修复、卸载工具；卸载插件可默认联动该工具，失败或返回 `3010` 时保留插件及工具并显示继续处理入口，工具不得自行重启电脑。具体约定见开发文档“安装与恢复”。

## 打包与分发

固定版本 zip 根目录放置清单、DLL、QML 与必要资源，不含用户配置和缓存。用户可直接本地安装；在 GitHub 添加 `xiaoji-plugin` topic，或在 Gitee 简介添加同名标记，公开仓库声明并发布 Release ZIP 后，目录任务自动发现并收录为未审核插件，无需首次登记。单插件声明使用根 `plugin.json`，多插件使用 `xiaoji-plugins.json`，详见发布文档。包安装与 Windows 系统驱动安装是不同动作。

构建和运行由开发者按各自工程确认。官方插件的独立发行、镜像补传和签名目录配置见 [插件发布](../docs/plugin-release.md)，不要覆盖已发布的同版本包。
