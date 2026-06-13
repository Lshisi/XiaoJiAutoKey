# 打包规则

## 配置文件

- `config.json` 是运行时生成的用户配置文件，存放在 exe 同级目录。
- 打包 zip 时绝对不能包含 `config.json`。
- 打包脚本 `scripts/package-release.ps1` 中已有清理逻辑，确保 `config.json` 不会被打入发布包。

## 发布包内容

发布包只应包含：

- `XiaoJiAutoKey.exe` -- 主程序
- `使用说明.url` -- 使用说明入口链接
- `audio/` -- 内置提示音文件
- `driver/` -- DD 驱动 DLL（DD32.dll / DD64.dll）

不应包含：

- `config.json` -- 用户配置（运行时自动生成）
- `logs/` -- 日志目录（运行时自动创建）
