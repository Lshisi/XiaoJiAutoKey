# DD 驱动插件

插件 ID：`xiaoji.driver.dd`。通过小叽 Plugin API 1.x 提供默认输入提供方。驱动实现来自小叽原有 `DdDriver`，保留现有输出和自身注入识别行为。

SDK 默认位于主仓库 `sdk/`，可通过 `XIAOJI_PLUGIN_SDK` 指定。`driver/` 保存现有 DD 运行库；运行库为第三方闭源组件，权利归原作者。本模块适配代码保留原项目 GPL-3.0 许可证。

编译产物为 `xiaoji-plugin-dd.dll`、`plugin.json`、`driver/` 与许可证，作为一个固定版本 zip 分发。系统驱动安装与插件包安装分别进行。

本模块与宿主同仓库维护，分别构建和打包；源码已写入，不代表已编译或运行确认。
