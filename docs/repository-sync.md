# Gitee 与 GitHub 同步

项目使用同一份 Git 历史和 GPL-3.0 许可证，在两个公开仓库维护源码：

- Gitee：<https://gitee.com/fourteen-fourteen/xiao-ji-auto-key>
- GitHub：<https://github.com/Lshisi/XiaoJiAutoKey>

Gitee 是框架更新检查的数据源，也是应用发行版同步的来源。GitHub 复用相同的应用 tag、发布说明和 zip 附件，不重新构建应用。仅在处理 Git 远端、tag 和两端发行版同步时读取本文；打包与更新规则见 [应用发布](release-process.md)。

## 本机代码推送

当前开发机已经配置：

- `origin` 从 Gitee 拉取，推送时依次推送到 Gitee 和 GitHub。
- `gitee` 单独访问 Gitee，用于先推送发版标签。
- `github` 单独访问 GitHub，方便查看远程分支或补推。
- `push.followTags=false`，普通分支推送不会自动携带新建的附注标签。

正常提交后执行 `git push`，会把当前跟踪分支推送到两个平台。推送发布主分支时执行：

```powershell
git push origin master
```

两个平台依次接收推送。如果其中一个失败，已经成功的一边不会回滚；修复连接后重新执行同一条命令即可。发版标签按下方顺序分别推送；不要用 `git push origin --tags`，它会把标签同时推到两边，可能在 Gitee 附件尚未准备好时触发同步。

Git 远程地址保存在本机 `.git/config`，不会随源码传播。在其他开发机首次配置时执行以下命令；如果已存在同名远程或 push URL，先用 `git remote -v` 查看，避免重复添加：

```powershell
git remote add gitee git@gitee.com:fourteen-fourteen/xiao-ji-auto-key.git
git remote add github git@github.com:Lshisi/XiaoJiAutoKey.git
git remote set-url --add --push origin git@gitee.com:fourteen-fourteen/xiao-ji-auto-key.git
git remote set-url --add --push origin git@github.com:Lshisi/XiaoJiAutoKey.git
git config --local push.followTags false
```

这组命令适用于 `origin` 已指向 Gitee、尚未配置 push URL 的现有克隆。新克隆需要先确保 `origin` 的拉取地址也是 Gitee。

## 发布新版本

1. 更新 `CMakeLists.txt` 的项目版本，按现有流程构建和打包，产出 `dist/XiaoJiAutoKey.zip`。发布包禁止包含 `config.json`。
2. 将发布代码提交到 `master`，把分支推送到两个平台，在对应提交上创建 `vx.y.z` 标签，先只推到 Gitee。以下命令中的 `vx.y.z` 替换为实际版本标签：

   ```powershell
   git push origin master
   git tag -a vx.y.z -m "发布 vx.y.z"
   git push gitee vx.y.z
   ```

3. 在 [Gitee Releases](https://gitee.com/fourteen-fourteen/xiao-ji-auto-key/releases) 创建对应标签的正式发行版，填写说明并上传 `XiaoJiAutoKey-vx.y.z.zip`。自动生成的源码压缩包不能代替应用发布包。
4. 确认 Gitee 发行版已发布且 zip 上传完成后，再将同一个标签推到 GitHub：

   ```powershell
   git push github vx.y.z
   ```

   GitHub 收到新的 `v*` 标签后自动运行 [Sync Gitee releases](https://github.com/Lshisi/XiaoJiAutoKey/actions/workflows/sync-gitee-releases.yml)，只同步该标签对应的 Gitee 发行版。标签所指的发布提交需包含本工作流与同步脚本。

5. 在 [GitHub Releases](https://github.com/Lshisi/XiaoJiAutoKey/releases) 查看相同版本的说明和下载附件。

工作流不再定时检查，也不在普通分支推送或标签删除时执行同步。需要补同步或重试时，在 Actions 页面点击 **Run workflow**，选择 `master`，填写具体 `tag`；留空则同步所有已经发布的版本。已经存在于 GitHub 的同一个标签再次推送不会产生新的推送事件，此时应手动重试工作流。

工作流只读取已经发布、带有上传 zip 附件的 Gitee 发行版。新 GitHub 发行版先作为草稿创建，附件完整上传后才公开；下载包会检查主程序是否存在，并拒绝含有 `config.json` 的包。标签缺失或提交不一致时会停止同步，避免将附件关联到错误的源码。

重复同步会更新发布说明、补充缺失附件，保留已上传的同名附件及 GitHub 额外附件。更新安装包应发布新版本和新标签；不会自动覆盖同名历史安装包。中断后可重新运行工作流，已完成的附件会被跳过。

工作流使用 GitHub 自动提供、仅在运行期间有效的 `GITHUB_TOKEN`，权限为该仓库的 `contents: write`。读取公开的 Gitee 发行版无需个人访问令牌，不需要在源码或仓库 Secrets 中保存私人 token，也无需部署 Webhook 接收服务。

## 软件内更新

框架更新仍读取 Gitee Releases，允许启动后异步及后台低频检查，并保留关于页手动检查；下载、安装和重启由用户主动触发，退出不发起新检查。具体联网约定见 [应用发布与更新](release-process.md#联网与更新交互)。插件目录和独立插件发行使用 [插件发布](plugin-release.md) 中的签名目录及专属 tag，不混入应用发行版同步。

插件与目录采用各自的双平台发布工作流：从同一产物上传 GitHub 和 Gitee，Gitee 凭据为 `XIAOJI_GITEE_TOKEN`；失败时通过 `Retry Gitee plugin mirror` 复用 GitHub 原附件补传。社区插件在任意一端公开标记和发行后自动发现，不必先向主仓库登记。具体标记、包格式及镜像身份以插件发布文档为准。
