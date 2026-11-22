# Gitee 与 GitHub 同步

项目使用同一份 Git 历史和 GPL-3.0 许可证，在两个公开仓库维护源码：

- Gitee：<https://gitee.com/fourteen-fourteen/xiao-ji-auto-key>
- GitHub：<https://github.com/Lshisi/XiaoJiAutoKey>

Gitee 是软件内手动更新检查的数据源。发行版由维护者在两个平台分别手动发布，使用相同的 tag、发布说明和同一份 zip 附件。

## 本机代码推送

当前开发机已经配置：

- `origin` 从 Gitee 拉取，推送时依次推送到 Gitee 和 GitHub。
- `github` 单独访问 GitHub，方便查看远程分支或补推。

正常提交后执行 `git push`，会把当前跟踪分支推送到两个平台。发布主分支与标签时执行：

```powershell
git push origin master
git push origin --tags
```

两个平台依次接收推送。如果其中一个失败，已经成功的一边不会回滚；修复连接后重新执行同一条命令即可。推送标签只同步 Git 标签，不会自动创建发行版或上传附件。

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
4. 在 [GitHub Releases](https://github.com/Lshisi/XiaoJiAutoKey/releases) 点击 **Draft a new release**，选择已经推送的同一个 tag，填写相同标题和发布说明，上传与 Gitee 完全相同的 zip，然后手动点击 **Publish release**。
5. 两个平台各自确认发行版的 tag、发布说明和下载附件。已有 tag 时直接选择，不另建指向不同提交的同名版本。

不配置发行版同步工作流或自动发布脚本，也不需要为手动发布设置访问令牌。GitHub 页面提供的自动生成源码包不等同于应用安装包，仍需手动上传打包得到的 zip。

## 软件内更新

双平台发布不改变程序的联网时机和更新来源。程序仍仅在用户于关于页点击“检查更新”时读取 Gitee Releases；启动、运行和关机不会触发更新检查。
