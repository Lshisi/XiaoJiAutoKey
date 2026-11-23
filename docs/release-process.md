# 应用打包、发布与更新

仅在处理应用包、应用版本、框架更新或发行流程时读取。操作命令是流程说明，不自动授权构建、打包、推送或发布；当前工作约束见 [项目入口](../AGENTS.md)。插件独立发行使用 [插件发布](plugin-release.md)。

## 发布包内容

`scripts/package-release.ps1` 生成 `dist/XiaoJiAutoKey.zip`，zip 内包含顶层 `XiaoJiAutoKey/`。

| 允许内容 | 约定 |
| --- | --- |
| `XiaoJiAutoKey.exe` | 主程序 |
| `使用说明.mhtml` | 离线使用说明 |
| `audio/` | 内置提示音 |
| `plugins/builtin/xiaoji.driver.xiaoji/<插件版本>/` | 仅小叽驱动插件的清单、DLL、声明载荷和许可证，版本取自其 `plugin.json` |

DD、USB 是默认不安装的官方插件，由用户从插件中心按需下载，不进入完整应用包。DD 运行库 `DD32.dll` / `DD64.dll` 和 INF 仅随 DD 独立插件包分发。小叽驱动管理工具、本机签名组件及许可证在小叽插件的 `driver/xiaoji/`，包括 `xiaoji.exe`、`xiaoji-setup.exe`；可选已签名包在该目录的 `package/`。旧 HID 输入文件不再发布，仅保留必要的历史安装清理能力。

发布包必须排除：

- exe 同级运行时生成的 `config.json`，任何打包路径都不能将用户配置收进去。
- `plugins/installed/`、`plugin-data/`、`plugin-cache/`，即用户安装插件、自有数据与缓存。
- 日志、构建中间文件及本机私钥。

脚本按清单收集指定版本载荷，不直接压缩用户运行目录。框架更新同样不得覆盖用户配置、插件数据、用户安装版本及本机选择。

## 构建与打包入口

发布使用静态 Qt，环境和已知问题见 [本机构建环境](build-environment.md)，重建 Qt 的细节见 [静态 Qt](build-qt6-static.md)。用户自行执行的打包入口：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-release.ps1 -QtPrefixPath D:\Qt\6.8.3-static -CMakePath D:\software\cmake-3.31.6\bin\cmake.exe -PluginCatalogPublicKey ((Get-Content -LiteralPath .\registry\public-key.txt -Raw).Trim())
```

`-XiaojiSignedDriverDir` 可指定完整的可信 `xiaoji.inf`、`xiaoji.dll`、`xiaoji.cat`；`-PluginCatalogPublicKey` 只传目录公钥，不传私钥。显式读取仓库公钥可避免旧构建缓存仍保留空值；目录公钥配置见 [插件发布](plugin-release.md#线上配置与首次初始化)。

## 应用版本与双平台发行

项目以同一份 Git 历史和 GPL-3.0 许可证开源在 Gitee 与 GitHub。框架更新读取 Gitee Releases；双平台代码、tag 与发行版同步的具体操作以 [仓库同步](repository-sync.md) 为准。

1. 更新 `CMakeLists.txt` 的 `project(XiaoJiAutoKey VERSION x.y.z)`，`APP_VERSION` 由它生成。README 不写固定应用版本和版本历史；版本历史放在 tag 与 Release 发布说明中。
2. 打包得到 `dist/XiaoJiAutoKey.zip`。应用 tag 必须为 `vx.y.z`，与 CMake 版本一致。
3. 发布分支同步两端后，先仅向 Gitee 推送应用 tag；创建对应 Release，并上传 `XiaoJiAutoKey-vx.y.z.zip`，也接受附件名 `XiaoJiAutoKey.zip`。
4. Gitee 发行版与附件完整后，再向 GitHub 推送同一个 tag，触发 `Sync Gitee releases`，复用相同说明和 zip。不要提前使用 `git push origin --tags` 同时推送到两端。
5. 自动生成的源码包不能代替应用包。同名历史附件不覆盖，更换内容需发布新版本。

应用更新只接受应用 `vx.y.z` tag，以及上述主程序 zip 附件名；地址须为 `/releases/download/` 发行附件。`/archive/refs/tags/` 源码包、`plugins/driver-…/v…` 插件 tag 和插件 zip 都不作为框架更新。

更新检查分页扫描发行版，每页 100 条、最多 50 页，跨页选择最高的应用版本；每页响应最多 8 MB。翻页失败、格式错误、重复分页或达到上限而未读完时，作为检查失败保留已有版本提示，不将部分插件发行列表误报成“无更新”。分页参数和附件下载查询参数都通过 WinHTTP 请求路径完整传递。

## 联网与更新交互

- 允许正常联网：启动后异步检查框架与插件更新，后台低频刷新插件目录和策略，默认周期为 6 小时；保留手动检查和关闭后台检查的设置。
- 每次进入插件中心自动异步请求最新签名目录，右上角可手动刷新列表和更新信息。这是正常页面读取，不额外确认联网，也不受“后台检查更新”开关控制；该开关只控制后台调度。
- 页面先显示本机插件及上次有效目录，请求完成后一起更新发现列表、版本、审核信息和更新数量；目录请求进行中再次进入时复用同一次请求，安装、管理或宿主操作繁忙时合并为一次待刷新请求，操作结束后继续。切换内置／发现／已安装、搜索和筛选只处理当前列表，不重复联网。
- 后台检查不能阻塞界面、按键引擎或离线启动，退出时不发起新检查。绘制与普通切页不触发目录请求，插件中心的进入刷新由页面生命周期触发。
- 通过首页标签、插件中心标记、更新说明提示新版本，不做强制弹窗或强制更新。普通版本更新不打断当前运行。
- 包下载、安装、重启由用户主动触发，不因后台发现新版本自动执行。维护者禁用策略与普通更新分别处理，详见 [插件发布](plugin-release.md)。
- 检查失败保留本地有效结果，以低频调度或用户手动刷新重试。插件中心明确显示加载或失败状态；无历史目录且联网失败时，不将失败显示为“没有插件”。目录、签名、公钥和离线策略的具体约束见插件文档。

## 应用内安装过程

下载 zip 到 `%TEMP%\XiaoJiAutoKeyUpdate`，由 miniz 解压和校验；若有顶层包目录会自动剥掉。解压兼容 PowerShell 5.1 的反斜杠条目与 GBK 文件名，`config.json` 永不覆盖。

用户点击“立即重启更新”后，停止引擎、释放按键、断开驱动；运行中的 exe 改名为 `XiaoJiAutoKey.old.exe`，覆盖安装目录，以 `--restart-wait` 拉起新实例并退出。进程内原生插件仍被引用的 DLL 保留到退出，文件共享冲突可暂存为 `.update-pending`，由新实例启动早期换入。

新实例清理 `.old.exe` 和更新暂存目录。不得借框架更新替换 `plugins/installed/`、插件用户数据或重新启用用户已停用的插件。插件版本激活、回退与恢复模式见 [插件开发](plugin-development.md#安装与恢复)。
