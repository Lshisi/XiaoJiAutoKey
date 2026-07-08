# 小叽 Pico C++ 固件

这是给 Raspberry Pi Pico / RP2040 开发板用的小叽硬件桥接固件，基于 Pico SDK 和 TinyUSB。

烧录后，Pico 会枚举成一个小叽 USB 复合设备：

- 厂商名：`XiaoJi`
- 产品名：`XiaoJi MX Keyboard`
- 序列号：`XJAK-<板载 Flash 唯一 ID>`
- USB 接口：
  - CDC 串口命令通道
  - HID 键盘
  - HID 鼠标
  - HID 媒体/浏览器控制

## 离线工具包

仓库的 `tools/` 目录可以保留这些 zip，方便换电脑后直接恢复编译环境：

- `pico-sdk-2.1.1.zip`
- `tinyusb-master.zip`
- `arm-gnu-toolchain-12.2-mpacbti-rel1.zip`

换电脑后按下面方式解压：

1. 把 `pico-sdk-2.1.1.zip` 解压到 `tools/pico-sdk`
2. 把 `tinyusb-master.zip` 解压/复制到 `tools/pico-sdk/lib/tinyusb`
3. 把 `arm-gnu-toolchain-12.2-mpacbti-rel1.zip` 解压到 `tools/`

解压后设置环境变量：

```powershell
$env:PICO_SDK_PATH = (Resolve-Path tools/pico-sdk).Path
$env:Path = "$(Resolve-Path 'tools/12.2 mpacbti-rel1/bin');$env:Path"
```

## 编译固件

```powershell
cmake -S firmware/pico -B firmware/pico/build -G Ninja
cmake --build firmware/pico/build
```

编译成功后会生成：

```text
firmware/pico/build/xiaoji_pico.uf2
```

## 烧录固件

1. 按住 Pico 板子上的 `BOOTSEL`
2. 插入 USB
3. 电脑会出现 `RPI-RP2` U 盘
4. 把 `firmware/pico/build/xiaoji_pico.uf2` 复制到这个盘根目录
5. Pico 会自动重启并运行小叽固件

后续如果固件已经在运行，可以通过串口发送 `B`，让 Pico 自动重启进 BOOTSEL。

## 串口协议

串口波特率：`115200`

每条命令用 ASCII 文本发送，并以 `\n` 结尾。

示例：

```text
PING
VER
R
B
D 0 65
U 0 65
D 1 1
U 1 1
D 0 175
U 0 175
```

命令说明：

- `PING`：测试连接，返回 `OK PONG`
- `VER`：读取固件版本
- `D`：按下
- `U`：松开
- `R`：释放所有键盘、鼠标和媒体控制状态
- `B`：释放所有输入，并重启进 BOOTSEL，方便升级固件

参数说明：

- 设备 `0`：键盘，按键码使用 Windows virtual-key code
- 设备 `1`：鼠标，按键码使用 Windows 鼠标 VK
- 鼠标按键：
  - `1`：左键
  - `2`：右键
  - `4`：中键
  - `5`：侧键 1
  - `6`：侧键 2
- 音量、播放/暂停、下一曲、浏览器后退等媒体/浏览器键仍然使用设备 `0`，固件内部会转成 HID consumer report

USB 序列号由 Pico SDK 的 `pico_unique_id` 从板载 Flash 唯一 ID 生成。同一块板子会保持同一个序列号，不同板子会枚举成不同 Windows 设备实例。

响应格式：

```text
OK ...
ERR ...
```

常见例子：

```text
D 0 65   # 按下 A
U 0 65   # 松开 A
D 1 1    # 按下鼠标左键
U 1 1    # 松开鼠标左键
D 0 175  # 按下音量加
U 0 175  # 松开音量加
```
