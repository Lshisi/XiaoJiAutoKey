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
- `driver/` -- DD/HID 运行库（DD32.dll / DD64.dll / hid/ddhid.63340.dll）

不应包含：

- `config.json` -- 用户配置（运行时自动生成）

## 构建注意事项

- 当前环境里 `cmake` 可能不在 `PATH`，优先使用 VS BuildTools 自带的 CMake：
  `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
- 如果 MSBuild 报 `System.ArgumentException: 已添加项。字典中的关键字:“Path”所添加的关键字:“PATH”`，说明当前进程环境里同时存在 `Path` / `PATH`。构建前先在当前 PowerShell 进程清掉重复项并设置单一 `Path`。
- 推荐构建命令：

```powershell
[System.Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[System.Environment]::SetEnvironmentVariable('Path', 'C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem;C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin', 'Process')
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Release --target XiaoJiAutoKey
```
