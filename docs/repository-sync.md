# Gitee 与 GitHub 同步

项目使用同一份 Git 历史和 GPL-3.0 许可证，在两个公开仓库维护源码：

- Gitee：<https://gitee.com/fourteen-fourteen/xiao-ji-auto-key>
- GitHub：<https://github.com/Lshisi/XiaoJiAutoKey>

Gitee 是软件内手动更新检查的数据源，也是发行版同步的来源。GitHub 复用相同的 tag、发布说明和 zip 附件，不重新构建应用。

## 本机代码推送

当前开发机已经配置：

- `origin` 从 Gitee 拉取，推送时依次推送到 Gitee 和 GitHub。
- `github` 单独访问 GitHub，方便查看远程分支或补推。

正常提交后执行 `git push`，会把当前跟踪分支推送到两个平台。发布主分支与标签时执行：

```powershell
git push origin master
git push origin --tags
```

两个平台依次接收推送。如果其中一个失败，已经成功的一边不会回滚；修复连接后重新执行同一条命令即可。推送标签只同步 Git 标签，发行版与附件由下方的工作流同步。

Git 远程地址保存在本机 `.git/config`，不会随源码传播。在其他开发机首次配置时执行以下命令；如果已存在同名远程或 push URL，先用 `git remote -v` 查看，避免重复添加：

```powershell
git remote add github git@github.com:Lshisi/XiaoJiAutoKey.git
git remote set-url --add --push origin git@gitee.com:fourteen-fourteen/xiao-ji-auto-key.git
git remote set-url --add --push origin git@github.com:Lshisi/XiaoJiAutoKey.git
```

这组命令适用于 `origin` 已指向 Gitee、尚未配置 push URL 的现有克隆。新克隆需要先确保 `origin` 的拉取地址也是 Gitee。

## 发布新版本

1. 更新 `CMakeLists.txt` 的项目版本，按现有流程构建和打包，产出 `dist/XiaoJiAutoKey.zip`。发布包禁止包含 `config.json`。
2. 提交发布代码，在对应提交上创建 `vx.y.z` 标签，并将 `master` 和标签推送到两个平台。
3. 在 [Gitee Releases](https://gitee.com/fourteen-fourteen/xiao-ji-auto-key/releases) 创建对应标签的正式发行版，填写说明并上传 `XiaoJiAutoKey-vx.y.z.zip`。自动生成的源码压缩包不能代替应用发布包。
4. GitHub 的 [Sync Gitee releases](https://github.com/Lshisi/XiaoJiAutoKey/actions/workflows/sync-gitee-releases.yml) 每天 09:17（UTC+8）计划执行一次，自动补齐发行版。GitHub 调度可能延后；需要立即同步时，在该页面点击 **Run workflow**，选择 `master`，`tag` 留空同步全部版本，或填写一个具体标签。
5. 在 [GitHub Releases](https://github.com/Lshisi/XiaoJiAutoKey/releases) 查看相同版本的说明和下载附件。

工作流只读取已经发布、带有上传 zip 附件的 Gitee 发行版。新 GitHub 发行版先作为草稿创建，附件完整上传后才公开；下载包会检查主程序是否存在，并拒绝含有 `config.json` 的包。标签缺失或提交不一致时会停止同步，避免将附件关联到错误的源码。

重复同步会更新发布说明、补充缺失附件，保留已上传的同名附件及 GitHub 额外附件。更新安装包应发布新版本和新标签；不会自动覆盖同名历史安装包。中断后可重新运行工作流，已完成的附件会被跳过。

工作流使用 GitHub 自动提供、仅在运行期间有效的 `GITHUB_TOKEN`，权限为该仓库的 `contents: write`。读取公开的 Gitee 发行版无需个人访问令牌，不需要在源码或仓库 Secrets 中保存私人 token。定时执行遵循 GitHub Actions 的调度和活跃度规则；若 GitHub 因仓库长期不活跃暂停定时工作流，可在 Actions 页面重新启用。

## 软件内更新

双平台发布不改变程序的联网时机和更新来源。程序仍仅在用户于关于页点击“检查更新”时读取 Gitee Releases；启动、运行和关机不会触发更新检查。
