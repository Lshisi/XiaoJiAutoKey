# 小叽插件目录

维护公开插件来源、自动收录、按版本审核、下架与禁用策略。源码收录任务不安装或执行插件。

运营配置、签名密钥管理和目录初始化以 [插件发布](../docs/plugin-release.md#线上配置与首次初始化) 为准。仓库仅保存公开公钥和运营规则，本机目录历史、私钥、凭据及生成结果不进入版本控制。

## 自由发布

采用公开标记自动发现：GitHub 添加 `xiaoji-plugin` topic；Gitee 在仓库简介中放置 `xiaoji-plugin`。默认分支公开根 `plugin.json` 或多插件索引 `xiaoji-plugins.json`，Release 上传固定版本 ZIP，目录任务自动收录为未审核状态；不需要首次登记或等待人工审批。

第三方 ID 使用 `仓库所有者小写名称.插件名`。`xiaoji.` 命名空间保留给 `registry.json` 中的官方发布者；首次有效收录后不通过普通声明转移所有权。搜索漏收录时可选用补充来源 Issue。不同仓库同 ID 仅可提供已知版本同哈希包的备用下载地址。

`scripts/sources.py` 负责两平台发现、声明读取与公共网络请求。Gitee 使用其网页实际采用的公开搜索服务，不能照搬 GitHub topic API；搜索错误、分页上限和单仓库读取失败保留此前有效来源并记录原因。`scripts/catalog.py` 负责包校验、版本聚合、审核、策略和签名；从已发布且验签通过的目录恢复历史，避免依赖单份 Actions 缓存。

签名版本记录同时包含 ZIP 的 `sha256` 和真实解压文件的 `contentsSha256`，发布账号由仓库平台及所有者生成，作者仅是插件自述。内容摘要算法、历史版本回填和新客户端上线顺序见[目录内容摘要与升级](../docs/plugin-release.md#目录内容摘要与升级)。只补充旧 ZIP 的摘要，不替换历史包或复用旧版本发布不同内容；包内不得携带宿主保留的 `.installation.json`。

## 审核和管理

`reviews.json` 的记录为 `{ "id": "author.plugin", "version": "1.0.0", "sha256": "包的 SHA-256", "status": "approved" }`，仅精确匹配的版本获得审核标记。删去记录即可撤销，新版默认未审核。

`policy.json` 中 `delisted` 是停止展示和新安装的插件 ID 列表。`blocked` 使用 `{ "id": "author.plugin", "version": "1.0.0", "sha256": "可选精确哈希", "reason": "原因" }`；版本可用 `*` 表示整个插件。审核、下架、禁用分别管理。

## 发布配置

使用现有 `Lshisi/XiaoJiAutoKey` 主仓库及根目录 `.github/workflows/plugin-registry.yml`，无需新建远端。维护者启用 Actions，并提供 ECDSA P-256 私钥作为 `XIAOJI_REGISTRY_SIGNING_KEY_PEM` 仓库 secret。私钥不写入源码。任务生成签名 `catalog.json`，签名格式为 SHA-256 + P-256，签名采用 32 字节 r 与 32 字节 s 拼接；公钥采用 65 字节非压缩点的 Base64。

目录默认发布到 GitHub 与 Gitee 两端的 `catalog` Release。配置 `XIAOJI_GITEE_TOKEN` secret 后，`scripts/publish_gitee.py` 通过官方 OpenAPI 上传相同文件；官方插件包同样复用 GitHub 的原始 ZIP。可信公钥在发行客户端前固化，两端共用一个签名。缺少凭据会明确跳过相应发布。正式配置和补传方式见 [插件发布](../docs/plugin-release.md)。

自动发现、可选补充来源、审核及策略通过同一发布队列更新。维护者保护主分支，不能把未审核投稿的任意脚本纳入发布任务。

无有效缓存且两端历史目录均不可恢复时，任务停止收录及发布，不生成新的签名目录。首次发布需在手动工作流中显式勾选 `bootstrap_catalog`（本地为 `--collect --bootstrap-catalog`），且必须没有缓存、全部目录地址明确返回 404；网络故障、验签失败或损坏缓存均不能触发初始化。已有有效历史时，即使指定初始化也继续沿用历史。

三个官方模块使用现有主仓库，通过 `tagPrefix` / `assetPrefix` 分开收录；DD 首选 Gitee 来源，USB 首选 GitHub 来源，并保留维护者显式绑定的可信镜像。第三方也可以使用多插件仓库。
