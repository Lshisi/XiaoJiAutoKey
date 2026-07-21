# 小叽 USB 硬件桥接固件

这是小叽当前使用的 USB 硬件固件，目标芯片为 CH552G。原 Pico 固件继续保留在
`firmware/pico`，但桌面程序目前只连接这套 USB 固件。

## USB 结构

- 设备标识：`VID_1209 / PID_584A`
- EP1 / EP2：CDC 串口命令通道
- EP3：复合 HID，包括键盘、五键鼠标和媒体控制
- USB 序列号：`XJAK-USB2-<芯片唯一 ID>`
- 固件握手：`XiaoJi USB V1.1.2`

键盘、鼠标、媒体键、按键连发和宏循环均由现有小叽桌面程序控制。固件兼容以下
命令：

```text
PING
VER
R
D 0 65
U 0 65
D 1 1
U 1 1
UPD 714021
```

桌面程序自动更新时使用 USB CDC 的 1200 波特率触发机制进入工厂更新程序，这是
CH55xduino 官方支持的更新入口。`UPD` 命令保留为备用入口，会先释放所有输入状态、
关闭 USB 并等待总线完成断开，然后跳转到 `0x3800`。

## 编译环境

已经验证的工具版本：

- Arduino CLI 1.5.1
- CH55xduino 0.0.26
- SDCC build.13407_4
- 内部 24 MHz 时钟
- 266 字节 USB 端点内存布局
- P3.6 下载配置

Arduino CLI 放在：

```text
tools/arduino-cli/arduino-cli.exe
```

CH55xduino 安装到：

```text
tools/ch552-arduino-data
```

开发板索引地址：

```text
https://raw.githubusercontent.com/DeqingSun/ch55xduino/ch55xduino/package_ch55xduino_mcs51_index.json
```

执行编译：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-ch552-firmware.ps1
```

生成的 WCHISPStudio 烧录文件：

```text
firmware/ch552/build/xiaoji_ch552.hex
```

## 板载指示灯

这块板的一颗灯直接连接电源，通电后常亮；另一颗由芯片自带下载程序用于显示更新
活动。它们不作为小叽运行状态灯，桌面程序和应用固件均不会驱动相关 GPIO。

## 首次使用 WCHISPStudio 烧录

1. 芯片系列选择 `CH55x`。
2. 芯片型号选择 `CH552`。
3. 下载接口选择 `USB`。
4. 在“目标程序文件1”一行点击 `...`，选择
   `firmware/ch552/build/xiaoji_ch552.hex`，并确认该行右侧复选框已勾选。
5. 下载配置脚保持 `P3.6`。
6. “下载完成后运行目标程序”保持勾选。
7. 首次调试建议关闭代码和数据保护；功能验证完成后再决定是否启用。
8. 点击“下载”。

如果搜索不到设备，需要完全断开开发板电源，通过板上的 BOOT/DOWNLOAD 按键或
下载焊盘给 D+/P3.6 接入上拉，然后重新连接 USB。空白芯片通常会自动进入
Bootloader。

烧录完成后，WCHISPStudio 中的下载设备会断开，Windows 将重新枚举小叽 USB
复合设备和一个 COM 端口。此时在小叽的驱动页面选择“USB”即可。

第一次烧录成功后，后续更新由小叽自动完成。桌面程序检测到内置 USB 驱动版本
高于设备版本时会提示更新，并在驱动页面显示“更新驱动”按钮。点击更新后，程序
会自动停止当前任务、切换设备状态、写入内置版本并等待设备重新连接，不需要再
手动操作 P3.6，也不需要打开 WCHISPStudio。

## 第三方代码

USB 底层基于 CH55xduino 的 `CdcHidCombo` 示例修改，许可证保存在
`LICENSE.ch55xduino`。
