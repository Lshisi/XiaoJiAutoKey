# 项目目录与命名

官方插件集中在主仓库 `plugins/`，按能力划分模块，沿用 Magpie 将插件作为仓库内独立模块维护的组织方式。小叽的模块仍使用 Qt/CMake 构建，源码目录、插件身份和构建目标分别命名。

```text
xiao-ji-auto-key/
├── xiaoji-plugins.json         公开的多插件发现声明
├── src/                       宿主与引擎实现、QML、界面模型
├── include/                   宿主与引擎头文件
├── sdk/                       对外插件协议、CMake 支持、规范和示例
├── plugins/
│   ├── driver-xiaoji/          小叽驱动插件、管理及签名组件
│   ├── driver-dd/              DD 驱动插件、运行库与 INF
│   └── driver-usb/             USB 驱动插件与固件资料
├── registry/                  双平台自动发现、审核、上下架、禁用、签名及镜像发布
├── cmake/                     工程共用的 CMake 模块
├── scripts/                   构建、打包及维护脚本
├── preview/                   复用正式页面的界面预览承载
├── assets/                    图标、音频与离线说明
├── third_party/               宿主共用的第三方依赖（miniz）
├── firmware/                  现有硬件固件资料
└── docs/                      按需读取的需求、开发规范与操作说明
```

## 源码命名

- 目录使用小写英文职责名。官方插件统一使用 `类型-名称`，当前为 `driver-xiaoji`、`driver-dd`、`driver-usb`；后续页面模块可使用 `page-名称`。
- `sdk` 表示公开协议，`registry` 表示目录运营，两者独立于具体插件实现。目录不再使用 `XiaoJiPluginDD` 等独立工程名称。
- 原有 C++ 分层沿用 `src/`、`include/` 及其内部职责目录；根目录 `third_party/` 只维护宿主共用依赖。驱动专有源码、运行库、安装组件和许可证集中在所属插件目录，不在根目录保留副本。QML 组件和 C++ 类型文件继续遵循其现有类型命名规则。
- CMake 插件装配集中在 `cmake/Plugins.cmake`，发布工作流集中在仓库根 `.github/`。插件、SDK 和目录服务不嵌套各自的 `.git` 或发布工作流副本。

## 文档组织

[AGENTS.md](../AGENTS.md) 是默认读取的轻量入口，只保留全局约束、任务对应的文档索引和维护规则。`docs/` 按界面、构建、发布、插件与业务规则分专题存放细则；开始任务时按领域读取相应文件或章节，不默认全文加载所有文档和历史计划。

细则在对应专题中维护一份，入口通过链接引用。新增约定时同步必要的索引与受影响文档，计划只登记实际完成的内容；命令示例不代表已执行或获得执行授权。

## 目录、身份与产物

| 源码目录 | 稳定插件 ID | CMake 包目标 | DLL |
| --- | --- | --- | --- |
| `plugins/driver-xiaoji/` | `xiaoji.driver.xiaoji` | `XiaoJiPluginXiaojiPackage` | `xiaoji-plugin-xiaoji.dll` |
| `plugins/driver-dd/` | `xiaoji.driver.dd` | `XiaoJiPluginDDPackage` | `xiaoji-plugin-dd.dll` |
| `plugins/driver-usb/` | `xiaoji.driver.usb` | `XiaoJiPluginUSBPackage` | `xiaoji-plugin-usb.dll` |

源码文件夹改名不改变插件 ID、提供方 ID、DLL、zip 附件名或用户配置字段。独立发行 tag 与源码模块一致，例如 `plugins/driver-dd/v1.0.0`；发布与收录配置必须同时修改，规则见 [插件发布](plugin-release.md)。

运行时继续通过稳定 ID 定位：`plugins/builtin/<id>/<version>/`、`plugins/installed/<id>/<version>/`、`plugin-data/<id>/`。这些目录由插件管理器维护，不能用源码目录名替换其中的 ID。

随应用部署的 `plugins/builtin/` 只包含小叽驱动；DD、USB 保留同仓库独立模块及发行目标，默认不构建、不预装，用户从插件中心按需下载到 `plugins/installed/`。源码同仓库不代表所有模块随应用分发。

小叽驱动依赖维护在 `plugins/driver-xiaoji/third_party/xiaoji/`，安装组件构建脚本维护在该插件的 `scripts/`；DD 运行库与 INF 维护在 `plugins/driver-dd/driver/`。根目录 `third_party/` 当前仅保留 `miniz/`，供宿主的更新解压、插件安装和分享数据使用。

## 草稿清理

此前在项目同级创建的 `XiaoJiPluginSDK`、`XiaoJiPluginRegistry`、`XiaoJiPluginDD`、`XiaoJiPluginXiaoji`、`XiaoJiPluginUSB` 已完成归仓，并按用户授权移入 Windows 回收站。它们不是当前开发入口，不再保留路径依赖。

本次目录整理同步了 CMake、打包收集、Git 忽略例外、发行 tag、目录收录和开发文档；没有编译、运行程序或执行测试。
