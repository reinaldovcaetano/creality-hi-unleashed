<h1 align="center">🖨️ Creality Hi Unleashed</h1>
<p align="center"><b>创想三维 Creality Hi (F018) 3D 打印机的社区固件：5 GHz Wi-Fi 6、现代内核栈、新版 Moonraker/Fluidd，以及带"一次性启动测试"的安全安装器。</b></p>
<p align="center">
<img alt="状态" src="https://img.shields.io/badge/状态-ALPHA%20测试版-red">
<img alt="打印机" src="https://img.shields.io/badge/打印机-Creality%20Hi%20(F018)-blue">
<img alt="soc" src="https://img.shields.io/badge/SoC-Allwinner%20T113--i-lightgrey">
<img alt="kernel" src="https://img.shields.io/badge/内核-5.4.61%20%2B%20backports%206.1-green">
</p>
<p align="center">
<a href="README.md">English</a> · <a href="README.pt-BR.md">Português</a> · <a href="README.es.md">Español</a> · <b>中文</b>
</p>

> [!WARNING]
> **ALPHA 测试版，风险自负。**
> - 本固件仅在**一台** Creality Hi（主板 `CR4NU200360C20`）上测试过。
> - 它会替换内核，与创想三维（Creality）**无任何关联**，也未获其认可。
> - 启动测试失败时，安装器会自动回到原系统；主板也始终可以通过 USB 重新刷机（见[恢复](#恢复)）。
> - 尽管如此，请在接受风险后再安装。

---

## 你能得到什么

| | Creality 官方 1.1.0.74 | Creality Hi Unleashed（alpha，OTA 1.2.0.0） |
|---|---|---|
| Wi-Fi | 内置 AIC8800DC，**仅 2.4 GHz** | 使用联发科 **MT7921AU** USB 网卡实现 **5 GHz Wi-Fi 6**（实测：80 MHz、2×2、连接速率 1200 Mbit/s）。内置芯片保留作备用。 |
| 无线协议栈 | cfg80211/mac80211 编译进 5.4 内核 | **linux-backports 6.1.145**（cfg80211、mac80211、mt76、mt7921u），并将 AIC8800 驱动移植到其上 |
| 内核 | Creality 二进制（未公开源码） | 基于全志 Tina 5.4.61 BSP 重新编译，使用 Creality 的配置和 Creality **原版设备树** |
| 有线网络 | 无 | USB 网卡 **RTL8153**（r8152），插入后自动 DHCP |
| 内存 | 488 MB RAM，无 swap | **256 MB zram** 交换（lzo） |
| Moonraker / Fluidd | 旧版 Moonraker、Fluidd 1.27 | 运行在打印机自带 Python 3.9 上的 **Moonraker（API 1.5）**、**Fluidd 1.38**、**Fluidd 内置摄像头画面** |
| 日志刷屏 | 每秒写入一次 `set enabled 1` | 仅在状态变化时记录 |
| Creality 云、ADB | 开启 | **关闭**（屏蔽云端域名，禁用 ADB） |
| SSH | 22 端口 | 额外的 **2222 端口 dropbear**，支持密钥登录和详细日志 |
| 安装安全性 | A/B 切换 | **一次性启动测试**：新分区只启动一次，通过测试后才成为正式分区 |

## 实现过程（简述）

1. **固件结构。**
   - OTA 是**无签名**的 SWUpdate cpio（"070702"），包含 `sw-description`、`uboot`、`kernel`、gzip 压缩的 squashfs `rootfs` 和 MD5 列表。
   - eMMC 有 A/B 两半：`bootA/B` 在 p4/p5，`rootfsA/B` 在 p6/p7。
   - U 盘安装由 `upgrade-server` 完成，它完全离线工作。
2. **内核源码。**
   - Creality 从未公开源码。一份公开的 T113 Tina 5.4.61 源码与其吻合：Creality 的 3619 个配置项中只缺 6 个，其真实符号的 **98.97%** 都存在于我们的编译结果中。
   - 缺失的部分已重建：
     - **GC9307 屏幕驱动：**根据 Creality 内核的反汇编重写，初始化序列完全一致（100/100 项操作）；
     - **USB-A 的 ID 引脚反相：**`USB_SUNXI_USB_ID_GPIO_ACTIVE_HIGH`，Creality 用它让 USB-A 口工作在主机模式。
3. **5 GHz。**
   - 5.4 内核没有 MT7921 驱动，因此把 `cfg80211` 改为模块重新编译内核，并加入 **linux-backports 6.1**。
   - 内置 **AIC8800DC** 的驱动（walnutpi 2024_1024，接近 Creality 的版本）已移植到 6.1 API，并按全志平台编译。若按通用 "Ubuntu" 平台编译，芯片永远不会上电。
4. **一次性安装器。**
   - 安装包**不会**切换正式分区。它写入 `bootcmd=run hi_tenta`，U-Boot 只启动新分区**一次**。
   - `hi-confirma` 只有在以下检查全部通过后，才把新分区设为正式分区：
     - 启动满 90 秒；
     - **Klipper 连续 60 秒保持 "ready"**；
     - **通过 Klipper 真实读取 EEPROM**；
     - 屏幕、触摸以及 USB-A 主机模式正常。
   - 任一项失败就重启，回到旧分区。
   - 经验：U-Boot 在 `bootcmd` **之前**就计算好 `root=`，所以 `hi_tenta` 还必须设置 `mmc_root`/`nand_root`。
5. **在打印机上现场发现的 I2C 缺陷。**
   - 换上新内核后，Klipper 报错 `MCU 'rpi' shutdown: Unable write i2c device` 并停止。
   - 原因：**公开 BSP 的引脚表中，PG9 的功能 3 写成了 `twi0`，正确应为 `twi1`**。Creality 私下修复了这一点。
   - 后果：只有 SCL 被复用为 I2C，SDA（PG9）无人占用。
   - 已在内核中修复。现在 `kernel/tools/conferir_pinos.py` 会把**设备树中的每个引脚组与内核引脚表逐一核对**（328/328）。
   - I2C 驱动还加入了重试：EEPROM 忙时，我们的驱动和 Creality 的驱动**都会**收到 NACK。
6. **可写层（overlay）的陷阱。**
   - `rootfs_data` 在升级后依然保留，因此 overlay 中的旧文件可能遮住安装包里的新文件。
   - `hi-limpa-overlay` 每个版本运行一次，清除这些旧副本，保留用户设置，并备份所有被删除的文件。

完整的实验记录（葡萄牙语）位于 [`docs/tecnico-pt/`](docs/tecnico-pt/)。

## 自行编译

**环境要求：**
- Docker 及 Docker Compose v2；
- 约 10 GB 可用空间；
- `curl`、`unzip`、`cpio`；
- 从 Creality 官网下载的 **Creality Hi 官方 OTA 1.1.0.74**。

```bash
git clone https://github.com/reinaldovcaetano/creality-hi-unleashed && cd creality-hi-unleashed
# 1. 把官方 OTA 放到：insumos/CR4NU200360C20_ota_img_V1.1.0.74.img
./scripts/obter-insumos.sh            # 下载并校验所有输入文件的 SHA-256（固定提交版本）
# 2. 可选，永远不会提交到 git：
#    pacote/local/authorized_keys     -> 你的 SSH 公钥（用于 2222 端口登录）
#    pacote/local/save_config.txt     -> 你这台打印机的 SAVE_CONFIG 块（重置后自动恢复）
docker compose run --rm kbuild        # 内核 + backports + AIC8800 -> pacote/p2/  （约 10-15 分钟）
docker compose run --rm pack          # 生成 OTA -> saida/CR4NU200360C20_ota_img_V1.2.0.0.img
./tests/rodar-testes.sh               # 在模拟的 ARM 环境中（使用镜像自带的 busybox）测试启动脚本
```

内核编译过程会输出引脚核对结果（`328 ok, 0 com erro`），并检查每个模块的 `vermagic`。它还会用 **Creality 原版设备树**生成启动镜像。

### 预构建的编译容器

编译容器已发布到 GitHub Packages，无需在本地构建：

```bash
docker compose pull      # ghcr.io/reinaldovcaetano/creality-hi-unleashed-{kbuild,pack}:alpha
```

容器里只有工具：Debian bookworm、ARM 交叉编译器和 squashfs-tools，不包含任何 Creality 文件。

## 安装

1. 准备一个 **FAT32** 格式的 U 盘，根目录只放 `CR4NU200360C20_ota_img_V1.2.0.0.img`，不要改文件名。
2. 打印机空闲时插入 U 盘，在屏幕上确认升级。
3. **等待约 7 分钟。**打印机会重启进入新系统一次，执行启动测试，然后：
   - **通过：**新分区成为正式分区（`logs/hi-confirma.log` 显示 `CONFIRMADO`）；
   - **失败：**自动重启回到原系统（`logs/hi-confirma-falhou.log`）。
4. 如需 5 GHz，插入 MT7921AU 网卡并重启。
   - 网卡会成为 `wlan0`，并使用与**内置 Wi-Fi 相同的 MAC 地址**，所以 IP 不变。
   - 不插网卡时，打印机照常使用内置 Wi-Fi。

安装后：
- **Fluidd：**`http://<打印机IP>:4408`，带摄像头画面。
- **Moonraker：**端口 `7125`。
- **SSH：**端口 `2222`。
- **诊断：**`logs/hi-diag.txt` 和 `logs/hi-limpa-overlay.log`。

> [!IMPORTANT]
> **不要**在本固件之上安装 Creality 官方更新。官方更新会替换内核，5 GHz 驱动也会随之丢失。请使用本项目的编译版本。

## 恢复

- **自动：**启动测试失败时，打印机会回到之前的分区。
- **手动切换启动分区：**

  ```sh
  fw_setenv boot_partition bootB; fw_setenv root_partition rootfsB; reboot
  ```
- **恢复出厂：**按 Creality wiki 的说明，在 U 盘中放一个名为 `wipe_all` 的空文件。
- **通过 USB 完整重刷：**
  1. 按住主板上的 **LOAD/FEL** 键。
  2. 用 Micro USB 线连接 Windows 电脑。
  3. 用 **PhoenixSuit** 刷入 Creality 出厂镜像。

## 已知限制（alpha）

- 仅在一台打印机上测试过。
- 只重建了 `gc9307_9bit_spi_tjc_240_320` 这一种屏幕型号，即受测设备树中的型号。
- `tlsc6x` 触摸模块默认关闭，因为受测机器使用 Goodix 触摸。如需重新启用：

  ```sh
  echo tlsc6x > /etc/modules.d/touchscreen-tlsc6x
  ```
- 只测试过 MT7921AU（`0e8d:7961`）网卡。
- Creality 云功能被有意关闭。
- 安装包自带的打印机配置是作者调校过的配置：加速度 4300 mm/s²，并修正了宏。如果你改过自己的配置，它们会被**保留**；出厂配置则会被替换。

## 仓库结构

```
compose.yaml, docker/        编译容器（kbuild = 内核，pack = OTA）
scripts/obter-insumos.sh     下载并校验所有输入（固定版本）
kernel/patches/              linux-hi.patch（屏幕、USB-ID、PG9 引脚、I2C 重试）、aic8800-walnutpi-hi.patch
kernel/config/               kernel-b.config、backports.config、Creality 原版配置
kernel/tools/                bootimg.py（Android boot v2）、conferir_pinos.py（设备树与 pinctrl 核对）
kernel/build-kernel.sh       内核与 Wi-Fi 完整编译
pacote/build/                dentro.sh（组装 OTA）、tentativa.py（一次性启动）
pacote/overlay*/             加入 rootfs 的文件（启动脚本、Moonraker、配置）
tests/                       ARM 模拟测试、I2C 台架工具
docs/tecnico-pt/             完整实验记录（葡萄牙语）
```

## 致谢与许可

- **借助 Anthropic 的 [Claude](https://claude.ai)（通过 Claude Code）完成。**Claude 与作者一起完成了固件拆解、内核与 Wi-Fi 移植、在打印机上的现场调试（包括 PG9/I2C 缺陷）、编译流程、测试，以及这份四种语言的文档。🤖🤝

- **Creality：**Hi_Klipper（GPL-3.0）。OTA 属于 Creality，本仓库**不**分发。
- **全志 Tina Linux 5.4 BSP：**`bazzonrobocon` 的镜像（GPL-2.0）。我们的内核补丁为 GPL-2.0。
- **linux-backports**、**linux-firmware**、**wireless-regdb：**kernel.org。
- **walnutpi/aic8800_sdio：**GPL-2.0。
- **Moonraker**（GPL-3.0）、**Fluidd**（GPL-3.0）。
- 本项目的脚本与文档：GPL-3.0（见 `LICENSE`）。

*Creality 与 Creality Hi 是其各自所有者的商标。本项目为独立社区项目。*
