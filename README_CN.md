<div align="center">

<img src="docs/linux.png" alt="ameba-linux development" width="800">

# development

**ameba-linux IoT SDK 中 Linux 侧的用户态应用、守护进程、工具集与预编译资产。**

[![SDK](https://badgen.net/badge/SDK/ameba--linux/blue)](https://aiot.realmcu.com/zh/latest/linux/index.html)
[![Language](https://badgen.net/badge/language/C%2FC%2B%2B%2FSh/blue)](#)
[![License](https://badgen.net/badge/License/Apache%202.0%20%26%20GPL/lightgrey)](#)

[English](README.md) · [中文版](README_CN.md) · [Linux SDK 文档](https://aiot.realmcu.com/zh/latest/linux/index.html)

</div>

`development/` 汇集了 Realtek 自研的、跑在 Ameba Linux 内核之上的**用户态软件**：命令行工具、守护进程、控制面工具、连接性辅助、AI 运行时、MP（量产测试）工具及预编译资产。每个子目录都是一个独立的软件包，自带 `Makefile`，并通过 [`meta-realtek/meta-realtek-bsp/recipes-development/`](../yocto/meta-realtek/meta-realtek-bsp/recipes-development) 或 [`meta-realtek/meta-sdk/recipes-rtk/`](../yocto/meta-realtek/meta-sdk/recipes-rtk) 下的 Yocto recipe 被打包进最终镜像。

## 🧭 在 SDK 中的位置

```text
┌──────────────────────────────────────────────────────────────┐
│  development/  ← 用户态应用、守护进程、MP 工具               │
│      adbd  rtwpriv  hciattach  recoveryd  aivoice  tflite …  │
├──────────────────────────────────────────────────────────────┤
│  Linux 6.18 内核 + 驱动  (sources/kernel/)                   │
├──────────────────────────────────────────────────────────────┤
│  协处理器固件  (sources/firmware/)                           │
├──────────────────────────────────────────────────────────────┤
│  Realtek Ameba SoC                                           │
└──────────────────────────────────────────────────────────────┘
```

## 🏗️ 目录结构

```text
development/
├── adb/            # Android Debug Bridge — host 端 adb + 设备端 adbd（USB / TCP 调试）
│   ├── adb/            #   adb 客户端（宿主机）
│   ├── adbd/           #   adbd 设备守护进程
│   ├── libcutils/      #   Android bionic cutils 移植
│   ├── libmincrypt/    #   RSA 签名校验
│   └── adb_usb.sh      #   USB gadget 拉起脚本
│
├── apps/           # 上层应用
│   ├── aivoice/        #   AI 语音应用（KWS、ASR、AFE），Realtek 离线语音栈
│   └── pangu_app/      #   Pangu 预编译应用包
│
├── bluetooth/      # 蓝牙用户态
│   ├── bt_fw/          #   BT 固件与配置（rtl8730_fw、mp_fw、LE-Audio 禁用、BR/EDR 禁用 …）
│   ├── hciattach/      #   HCI attach 守护进程（H4、Realtek 私有下载 v2/v3）
│   └── rtlbtmp/        #   Realtek 蓝牙量产 / 射频测试工具
│
├── cmds/           # 硬件控制小工具
│   ├── captouch/       #   电容触摸控制器 CLI
│   ├── efuse/          #   eFuse 读 / 烧写 CLI
│   ├── getevent/       #   Android 风格 input 事件抓取器
│   ├── km4_console/    #   经 IPC 进入 KM4 协处理器的 console
│   └── otp-ipc/        #   经 IPC 访问 OTP
│
├── openthread/     # Thread / 802.15.4 支持
│   ├── 8771HTV/                                #   RTL8771 的 RCP / NCP 固件
│   └── RTKRCP_Config_CFU_Tool_V1.02_20241218/  #   配置 & CFU 工具
│
├── recovery/       # Recovery / OTA 升级基础设施
│   ├── recovery.c              #   Recovery 主程序
│   ├── recoveryd/              #   常驻 recovery 守护进程
│   ├── install.c               #   升级包安装逻辑
│   ├── config_parser.c         #   flash_image.cfg 解析
│   ├── flash_image.cfg         #   分区 / 镜像布局配置
│   └── storage_mount/          #   存储挂载辅助
│
├── tflite/         # TensorFlow Lite (LiteRT) Ameba 运行时
│   ├── lib/            #   aarch64 预编译 TFLite 库
│   ├── inc/            #   公共头文件
│   ├── examples/       #   `minimal` 与 `label_image` demo
│   ├── bin/            #   参考二进制
│   ├── patch/          #   TF v2.18.0 之上的本地 patch
│   └── build/          #   构建辅助
│
└── wifi/           # Wi-Fi 用户态
    ├── ATWZ/               #   AT 命令风格 Wi-Fi 测试工具
    ├── rtwperf/            #   Realtek Wi-Fi 吞吐 / 性能测试
    ├── rtwpriv/            #   Realtek 私有 ioctl CLI（`iwpriv` 风格）
    └── wireless-regdb/     #   无线管制数据库
```

## ✨ 亮点

- **adb** — 完整 ADB 调试通道（宿主 `adb` + 设备 `adbd`），可跑在 USB gadget 或 TCP 之上。
- **AI Voice** — Realtek 离线语音栈（AFE + KWS + VAD + ASR），详见 [`apps/aivoice/README.md`](apps/aivoice/README.md)。
- **TFLite** — 面向 Ameba 的 LiteRT v2.18.0 移植，附最小 demo 和图像分类 demo，详见 [`tflite/README.md`](tflite/README.md)。
- **蓝牙 MP** — 固件二进制 + 支持 Realtek HCI 下载 v2/v3 的 `hciattach`，包含 LE-Audio、BR/EDR 禁用变体以及 `rtlbtmp` 射频测试工具。
- **Wi-Fi 工具链** — `rtwpriv`（私有 ioctl）、`rtwperf`（吞吐）、`ATWZ`（AT 风格 Wi-Fi 测试）以及无线管制数据库。
- **Recovery** — 自包含的 recovery + OTA 安装器，带常驻 `recoveryd` 守护进程和可配置分区布局。
- **协处理器桥梁** — `km4_console` 与 `otp-ipc` 通过 IPC 把 KM4 侧（跑 [`firmware/`](../firmware) 镜像）暴露给 Linux 用户态。
- **OpenThread** — RTL8771 802.15.4 combo 的 RCP 固件与配置工具。

## 🚀 构建

每个子目录都是普通 `Makefile` 工程，由 Yocto 常规方式编译。在 SDK 根目录（`6.18.y/`）下：

```bash
# 进入构建环境
source envsetup.sh -m rtl8730elm-va8 -d ameba-full

# 构建完整镜像（含 development/ 下全部）
m

# 只编单个包（如 adbd）
bitbake -c cleanall adbd
bitbake adbd
```

单独脱环境交叉编译某个包时：

```bash
cd sources/development/<package>
make CROSS_COMPILE=<your-cross-prefix>-
```

包与 recipe 的对应关系位于 [`sources/yocto/meta-realtek/meta-realtek-bsp/recipes-development/`](../yocto/meta-realtek/meta-realtek-bsp/recipes-development)（BSP 组件）以及 [`meta-sdk/recipes-rtk/`](../yocto/meta-realtek/meta-sdk/recipes-rtk)（`aivoice`、`tflite`、`recoveryd` 等上层应用）。

## 📚 文档

- **Ameba Linux SDK 用户指南** — https://aiot.realmcu.com/zh/latest/linux/index.html
- **AIVoice** — https://aiot.realmcu.com/zh/latest/rtos/ai/aivoice/aivoice_overview/index.html
- **Ameba 上的 TFLite** — https://aiot.realmcu.com/zh/latest/linux/ai/tfl/index.html

## 💬 反馈

社区：[Real-AIOT 论坛](https://forum.real-aiot.com/)
