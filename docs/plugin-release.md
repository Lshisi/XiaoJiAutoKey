# 插件发布与目录运营

SDK、三个官方插件和目录服务均在现有主仓库，GitHub 为 `Lshisi/XiaoJiAutoKey`，Gitee 为 `fourteen-fourteen/xiao-ji-auto-key`，无需额外创建仓库。官方插件独立发行，平台配置与目录初始化见下方操作约定。

## 官方插件独立发行

| 源码 | 独立包目标 | tag | 附件 |
| --- | --- | --- | --- |
| `plugins/driver-dd` | `XiaoJiPluginDDPackage` | `plugins/driver-dd/vx.y.z` | `XiaoJiPluginDD-vx.y.z.zip` |
| `plugins/driver-xiaoji` | `XiaoJiPluginXiaojiPackage` | `plugins/driver-xiaoji/vx.y.z` | `XiaoJiPluginXiaoji-vx.y.z.zip` |
| `plugins/driver-usb` | `XiaoJiPluginUSBPackage` | `plugins/driver-usb/vx.y.z` | `XiaoJiPluginUSB-vx.y.z.zip` |

修改对应清单 `version` 和代码，提交源文件后再发布对应 tag。协议、宿主和固件版本分开管理。已发布的同一 ID + 版本不能替换包内容，重做包应使用新版本。官方入口的版本宏由清单生成。

[plugin-release.yml](../.github/workflows/plugin-release.yml) 在推送上述 tag 后独立构建和打包，无需 Qt，不执行驱动、安装助手或输入操作，向 GitHub Release 上传 zip 及 `.sha256`；配置 Gitee 发布凭据后，将同一份文件上传到 Gitee Release。两端不分别重建 ZIP，确保包哈希一致。先同步源提交到两端，再推插件 tag。

已发布插件附件不可覆盖。若 GitHub 发布成功、Gitee 失败，用 [Retry Gitee plugin mirror](../.github/workflows/plugin-mirror.yml) 手动填写同一个 tag，直接下载 GitHub 已发布附件补镜像；不重新构建或重打包。相同内容可幂等补传，不同内容拒绝覆盖。

也可以自行构建后使用 `scripts/package-plugin.ps1`。例如，显式构建 `XiaoJiPluginDDPackage` 后，从 `build-qt6/plugins/driver-dd/Release/` 打包，`-PackageName XiaoJiPluginDD`。主程序默认构建不再依赖 DD、USB；两者保留独立构建和发行目标。脚本只收集清单、原生模块、页面及其图标文件、后台 QML、窗口入口、声明的资源、管理工具和许可证，不重新编译或执行插件。页面图标由 `pages[].icon` 声明；收录时与宿主一致校验内置名称或包内 SVG / PNG 文件，具体规则见 [插件页面](plugin-development.md#qml-页面)。

后台和独立窗口仍使用同一套发布与审核流程，不单独建发布平台。清单声明和 SDK 桌宠/原生后台示例见 [插件运行时](plugin-runtime.md)；纯 QML 版本之间一起替换页面、后台和窗口，新旧任一版本包含原生模块或进程仍保留已加载模块时整体等待重启。

完整应用仍由 `scripts/package-release.ps1` 构建和打包，只携带小叽驱动的清单版本。DD、USB 默认不安装，通过插件中心按需下载；独立包仍可经“导入”安装，官方身份须匹配可信目录。用户安装版本、`plugin-data`、缓存及 `config.json` 不进入发布包。框架更新只接受 `vx.y.z` 和 `XiaoJiAutoKey.zip` / `XiaoJiAutoKey-vx.y.z.zip`，与插件 tag 及附件分开。

驱动 DLL、INF/SYS/CAT、安装工具及许可证属于对应插件，小叽以完整内置插件随应用发布。小叽插件固定启用、不能删除，其他插件允许停用及卸载。驱动页的诊断/自检入口由清单声明，专有说明通过原生诊断文本提供；系统驱动卸载可与插件移除联动，失败及待电脑重启时保留文件和工具。管理工具的退出码、权限及继续卸载约定见 [插件开发](plugin-development.md#安装与恢复)。

## 社区发布与收录

按 Magpie 的公开标记自动发现方式处理，首次发布不要求提交登记 Issue，也不等待人工审核。GitHub 和 Gitee 均可作为独立发布源，选择任意一端即可。

| 平台 | 开发者公开标记 | 发现方式 |
| --- | --- | --- |
| GitHub | 给仓库添加 `xiaoji-plugin` topic | GitHub 仓库主题搜索，排除归档仓库和 fork |
| Gitee | 在仓库简介中放入独立词 `xiaoji-plugin`，例如“小叽插件 xiaoji-plugin” | 使用 Gitee 网页实际采用的公开仓库搜索，再读取仓库信息确认标记 |

Gitee 的公开搜索和 GitHub topic API 不同，不能假设 `topic:` 查询语法通用，也不能仅凭仓库标签保证其搜索结果包含该项目。原 OpenAPI 仓库搜索可能对已知仓库返回空列表，因此自动任务使用 Gitee Search 的公开仓库索引。

开发者流程：

1. 在公开仓库默认分支放置声明：单插件可直接放根目录 `plugin.json`；多插件使用下方的 `xiaoji-plugins.json`。第三方 ID 为 `仓库所有者小写名称.插件名`，`xiaoji.` 为官方保留。
2. 添加对应平台的公开标记，发布 Release，并上传固定版本 ZIP，结构遵循 [插件开发](plugin-development.md)。声明和 ZIP 内的 ID 必须一致，ZIP 内清单才是实际安装内容。
3. 目录任务每两小时搜索两平台，读取声明和发行包，检查元数据、路径、版本、兼容性及哈希，自动收录为“未审核”。后续版本同样自动收录。
4. 客户端后台或手动刷新签名目录后展示插件；用户主动选择安装。审核申请可随后进行，新版不继承旧版审核。

多插件声明示例：

```json
{
  "format": 1,
  "plugins": [
    {"id": "yourname.my-tool", "tagPrefix": "plugins/my-tool/v", "assetPrefix": "MyTool-"}
  ]
}
```

单插件仓库也可使用此声明以限定 tag 或附件前缀。`tagPrefix`、`assetPrefix` 可选；不指定时从公开 Release ZIP 中匹配清单 ID。声明不含本机路径和密钥，也不会执行开发者的构建脚本。主仓库的 [xiaoji-plugins.json](../xiaoji-plugins.json) 声明三个官方模块。

搜索有索引延迟和 API 配额，发布后不保证即时出现。搜索故障或达到分页上限时保留此前有效结果；完整搜索确认撤去标记后，自动来源退出公开展示，已有安装和策略记录保留。单仓库读取最近 30 个 Release，每插件最多保留 128 个版本；本版 GitHub 最多搜索 500 个仓库，Gitee 最多 250 个，达到上限会在 `collection-errors.json` 说明。

“补充插件来源”Issue 只用于搜索漏收录或索引延迟的可选补充，由自动任务处理，不是发布或审核门槛。首次有效收录绑定来源；不同仓库同 ID 不改写发布者，只能把已知版本、哈希一致的包作为备用下载地址。维护者可在 `registry.json` 显式绑定可信镜像，允许镜像独立提供新版本。官方 GitHub / Gitee 仓库已声明此绑定。

自动任务不安装、不加载社区插件，不执行包内代码；格式和网络错误写入 `collection-errors.json`。Actions 缓存失效时，任务先验证已发布目录的签名，再恢复来源绑定和包哈希历史。没有有效缓存、两端目录又都不可恢复时，停止本轮收录和发布，保留线上目录。不能把历史读取失败当成空历史继续签名。

## 线上配置与首次初始化

此前试用插件及测试目录已撤回，正式运营需要配置工作流凭据并重新发布。沿用已配置的客户端公钥、受保护签名密钥及可信目录历史，不应为恢复发布清空版本历史或复用已试发版本的不同包内容。访问令牌与目录签名密钥分别管理；本机凭据、私钥和缓存不提交 Git。

1. 在现有 GitHub 仓库启用 Actions，保护默认分支并限制 `registry/` 和工作流的修改权限；保持源码提交同步到现有 Gitee 仓库。
2. 首次部署时生成 ECDSA P-256 密钥，将 PEM 私钥放入 Actions secret `XIAOJI_REGISTRY_SIGNING_KEY_PEM`；已有可信公钥和签名密钥时沿用原密钥。私钥不得放进源码、客户端、Issue 或插件包。
3. 配置 `XIAOJI_GITEE_TOKEN` Actions secret，授予现有 Gitee 仓库的 Release 与附件写入权限。任务通过官方 OpenAPI 发布相同目录和官方插件包；该 token 不进入客户端、源码或日志。
4. 首次手动运行 `Publish plugin catalog` 时勾选 `bootstrap_catalog`；仅在没有本地缓存且全部历史目录地址明确返回 404 时允许创建首份目录。网络、验签或格式失败不会触发初始化。已有历史时始终沿用已恢复的历史，后续正常运行保持该选项关闭。等效本地命令为 `python registry/scripts/catalog.py --collect --bootstrap-catalog`，由维护者自行执行。任务向两端 `catalog` Release 发布相同的签名元数据；公钥为 `public-key.txt` 中 65 字节非压缩点的 Base64，通过可信渠道核对后再分发客户端。
5. 发行客户端时将公钥通过 CMake `XIAOJI_PLUGIN_CATALOG_PUBLIC_KEY` 固化，或给打包脚本传 `-PluginCatalogPublicKey '<Base64公钥>'`。主目录由 `XIAOJI_PLUGIN_CATALOG_URL` 指定，备用由 `XIAOJI_PLUGIN_CATALOG_FALLBACK_URL` 指定；默认分别为下表两端。插件中心不提供地址安装或目录设置入口；自建发行在构建配置或本机配置中指定自己的目录和备用源。
6. 由维护者实际确认两平台自动发现、收录、包更新、审核、禁用和镜像回退。源码实现完成不代表此链路已经运行通过。

| 目录源 | 默认 HTTPS 地址 |
| --- | --- |
| GitHub | `https://github.com/Lshisi/XiaoJiAutoKey/releases/download/catalog/catalog.json` |
| Gitee | `https://gitee.com/fourteen-fourteen/xiao-ji-auto-key/releases/download/catalog/catalog.json` |

主源联网、签名或修订号校验失败时，客户端尝试备用源；两端验证同一把可信公钥，并拒绝回退到已缓存修订号之前。包镜像同样逐个尝试，只有与签名目录 SHA-256 一致的文件才能进入安装流程。全部失败保留本地目录、策略和安装状态。

缺少签名 secret 时，可选补充来源记录仍可保存，工作流跳过线上目录发布。缺少 Gitee token 时明确跳过 Gitee 镜像，GitHub 产物保留；配置后通过镜像补传工作流补齐。未配置可信公钥的客户端不读取未签名目录；已有预装插件和普通本地开发包仍可使用。`xiaoji.` 官方命名空间的独立安装包需要匹配可信目录，不能仅凭包内名称自称官方。

签名封装为 `format: 1`、`algorithm: ecdsa-p256-sha256`、Base64 `payload` 和 Base64 `signature`；签名为固定 32 字节 r 与 32 字节 s 拼接。客户端按目录来源及公钥分别缓存，拒绝同一缓存内的修订号倒退；网络失败保留本地有效策略。

## 审核、上下架和禁用

维护者可编辑 `registry/reviews.json`、`registry/policy.json`，也可自行执行 `registry/scripts/moderate.py` 修改本地记录，提交后由目录任务签名发布。没有专用网页管理后台。

| 操作 | 文件或命令 | 效果 |
| --- | --- | --- |
| 审核版本 | `review <id> --version x.y.z --sha256 <完整哈希>` | 仅该版本及 zip 标记审核通过 |
| 撤销审核 | `revoke-review`，参数同上 | 移除审核标记，已有插件仍可使用 |
| 下架 / 上架 | `delist <id>` / `relist <id>` | 隐藏或恢复发现与新安装，保留旧版本识别 |
| 禁用 | `block <id> --version x.y.z --reason '<原因>'` | 阻止安装和启用，客户端刷新到有效策略后停止该版本的框架调用 |
| 整插件禁用 | `block <id> --reason '<原因>'` | 默认版本 `*`，覆盖所有版本 |
| 限定包禁用 | 禁用命令附加 `--sha256 <哈希>` | 仅匹配下载 zip；对随应用重建的预装副本应使用 ID + 版本规则 |
| 撤销禁用 | `unblock`，ID、版本和可选哈希与原规则一致 | 新修订移除该策略，用户按本机选择恢复 |

新版本不会继承旧版审核。未审核插件仍可公开安装；目录签名只证明元数据来源。离线客户端不能实时获知新策略，已缓存的禁用继续生效；删除目录条目不能代替发布禁用规则。

## 用户运行核对事项

源码完成不代表可发布。用户自行构建与确认：旧配置迁移、三种官方输出与原有按键语义、新驱动和页面接入、同级导航与拖动排序保存、未审核安装、独立更新与回退、离线缓存、策略变更、深浅色及各 DPI、完整应用升级后配置与插件数据保留。开发过程中不编写测试类或自行运行验证。
