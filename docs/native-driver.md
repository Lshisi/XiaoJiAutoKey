# 小叽 HID 输入驱动

## 目标

小叽 HID 是与 DD、HID、USB 并存的第四套输入后端，不删除或修改现有驱动文件。
桌面程序通过设备接口和 IOCTL 控制自有 KMDF 驱动，驱动使用 Windows Virtual HID
Framework 创建虚拟键盘、五键鼠标和 Consumer Control 设备。

## 组成

- `driver/xiaoji-vhid`：独立 WDK 工程、INF 和内核实现。
- `driver/shared/XiaoJiVhidProtocol.h`：内核与桌面程序共享的固定宽度 ABI。
- `NativeHidDriver`：桌面程序输入后端，负责独占租约、心跳、键码映射和按键状态。
- `NativeHidRuntimeService`：负责根设备创建、驱动包安装、状态检查和卸载。

## 输入能力

- NKRO 键盘位图，支持标准键、修饰键、F1-F24 和数字键盘。
- 五键相对鼠标，协议保留 X/Y、垂直滚轮和水平滚轮。
- 音量、播放控制、浏览器和应用启动 Consumer Usage。
- 进程句柄关闭、主动释放、设备掉电和看门狗超时都会释放全部输入状态。

## 并发和故障恢复

同一时间只允许一个用户态文件句柄持有驱动租约。桌面程序每 500 ms 发送心跳，内核在
2500 ms 内未收到活动时撤销租约并发送全释放报告。被系统长时间挂起后，桌面程序会重新
申请租约，避免旧会话继续占用设备。

## 构建和签名

安装 Visual Studio 2022、Windows Driver Kit 和 WDK Visual Studio 集成后执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-native-driver.ps1 -Configuration Release
```

开发包可以使用测试证书；正式发布必须使用 Microsoft 接受的生产签名流程。发布脚本要求
`XiaoJiVhid.sys`、`XiaoJiVhid.inf` 和 `XiaoJiVhid.cat` 全部存在，并将它们放到
`driver/native/`。生产签名前不要发布测试证书或私钥。

## 版本规则

共享协议使用 `XIAOJI_VHID_PROTOCOL_VERSION`，驱动实现使用
`XIAOJI_VHID_DRIVER_VERSION`。破坏 ABI 兼容时必须提升协议主版本；仅修复内核实现时提升
驱动版本，并同步更新 INF 的 `DriverVer`。
