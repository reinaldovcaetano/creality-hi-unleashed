<h1 align="center">🖨️ Creality Hi Unleashed</h1>
<p align="center"><b>Community firmware for the Creality Hi (F018) 3D printer: 5 GHz Wi-Fi 6, a modern kernel stack, a modern Moonraker/Fluidd setup and a safe one-shot installer.</b></p>
<p align="center">
<img alt="status" src="https://img.shields.io/badge/status-ALPHA-red">
<img alt="printer" src="https://img.shields.io/badge/printer-Creality%20Hi%20(F018)-blue">
<img alt="soc" src="https://img.shields.io/badge/SoC-Allwinner%20T113--i-lightgrey">
<img alt="kernel" src="https://img.shields.io/badge/kernel-5.4.61%20%2B%20backports%206.1-green">
</p>
<p align="center">
<b>English</b> · <a href="README.pt-BR.md">Português</a> · <a href="README.es.md">Español</a> · <a href="README.zh-CN.md">中文</a>
</p>

> [!WARNING]
> **ALPHA. Use at your own risk.**
> - This firmware has been tested on **one** Creality Hi, on the main board `CR4NU200360C20`.
> - It replaces the kernel, and it is **not** affiliated with or endorsed by Creality.
> - The installer falls back to your previous system automatically if the boot test fails, and the board can always be re-flashed over USB (see [Recovery](#recovery)).
> - Even so, only install it if you accept the risk.

---

## Download (alpha)

A ready-to-install image is available in **[Releases → v1.2.0.0-alpha](https://github.com/reinaldovcaetano/creality-hi-unleashed/releases/tag/v1.2.0.0-alpha)**. It has no SSH key and no personal calibration. Read the warning above before installing; to add your own SSH key, build the image yourself.

## What you get

| | Stock Creality 1.1.0.74 | Creality Hi Unleashed (alpha, OTA 1.2.1.0) |
|---|---|---|
| Wi-Fi | Internal AIC8800DC, **2.4 GHz only** | **5 GHz Wi-Fi 6** with a MediaTek **MT7921AU** USB dongle (tested: 80 MHz, 2×2, 1200 Mbit/s link). The internal chip stays as a fallback. |
| Wireless stack | cfg80211/mac80211 built into the 5.4 kernel | **linux-backports 6.1.145** (cfg80211, mac80211, mt76, mt7921u) + AIC8800 driver ported to it |
| Kernel | Creality binary (no source published) | Rebuilt from the Allwinner Tina 5.4.61 BSP, using Creality's config and Creality's **original device tree** |
| Wired network | none | USB Ethernet **RTL8153** (r8152), DHCP on hot-plug |
| Memory | 488 MB RAM, no swap | **256 MB zram** swap (lzo) |
| Moonraker / Fluidd | old Moonraker, Fluidd 1.27 | **Moonraker (API 1.5) on the printer's Python 3.9**, **Fluidd 1.38**, **camera inside Fluidd** |
| Log spam | `set enabled 1` written every second | logged only when the state changes |
| Creality cloud, ADB | on | **off** (cloud domains blocked, ADB disabled) |
| SSH | port 22 | extra **dropbear on port 2222** with key login and a detailed log |
| Install safety | A/B switch | **One-shot boot test**: the new half is booted once and only becomes official after it passes the test |
| Touch screen | Creality's closed UI | **Hi Painel**: our own modern UI (LVGL 8.3) with live data, editable calibrations (PID, input shaper with graph, bed mesh, Z tilt, PA, ideal speed), print with thumbnails, live tuning, filament manager, Wi-Fi with keyboard, maintenance reminders and power-loss resume. Creality's UI stays one tap away. |

## How it was done (short story)

1. **Firmware anatomy.**
   - The OTA image is an unsigned SWUpdate cpio ("070702") containing `sw-description`, `uboot`, `kernel`, a gzip squashfs `rootfs` and an MD5 list.
   - The eMMC has A/B halves: `bootA/B` on p4/p5 and `rootfsA/B` on p6/p7.
   - The USB-stick installer is `upgrade-server`, which works fully offline.
2. **Kernel source.**
   - Creality never published it. A public Allwinner T113 Tina 5.4.61 tree turned out to match it: of Creality's 3619 config options, only 6 are missing, and **98.97 %** of Creality's real symbols exist in our build.
   - The missing pieces were rebuilt:
     - **GC9307 display driver:** recreated from the disassembly of Creality's kernel. Its init sequence is identical, 100 of 100 operations.
     - **Inverted USB-A ID pin:** `USB_SUNXI_USB_ID_GPIO_ACTIVE_HIGH`, which Creality uses so the USB-A port works as host.
3. **5 GHz.**
   - The MT7921 driver does not exist for 5.4, so the kernel was rebuilt with `cfg80211` as modules and **linux-backports 6.1** was added.
   - The internal **AIC8800DC** driver (walnutpi 2024_1024, close to Creality's version) was ported to the 6.1 API and built for the Allwinner platform. A build for a generic "Ubuntu" platform would never power the chip on.
4. **One-shot installer.**
   - The package does **not** switch the official half. It sets `bootcmd=run hi_tenta`, and U-Boot boots the new half **once**.
   - `hi-confirma` only makes that half official after these checks pass:
     - 90 s of uptime;
     - **60 s of Klipper "ready" with no interruption**;
     - a **real EEPROM read through Klipper**;
     - display, touch and USB-A in host mode.
   - Otherwise it reboots back to the old half.
   - Lesson learned: U-Boot computes `root=` **before** `bootcmd`, so `hi_tenta` also has to set `mmc_root`/`nand_root`.
5. **The I2C bug, found live on the printer.**
   - On the new kernel, Klipper crashed with `MCU 'rpi' shutdown: Unable write i2c device`.
   - Cause: the **public BSP pin table lists PG9 function 3 as `twi0`; the correct value is `twi1`**. Creality fixed this privately.
   - Effect: only SCL was muxed and SDA (PG9) stayed unclaimed.
   - Fixed in the kernel, and `kernel/tools/conferir_pinos.py` now checks **every pin group of the device tree against the kernel table** (328/328).
   - A retry was also added to the I2C driver. A busy EEPROM NACKs on **both** drivers, ours and Creality's.
6. **Persistent overlay pitfalls.**
   - `rootfs_data` survives OTAs, so an old file in the overlay can hide the new one from the package.
   - `hi-limpa-overlay` removes stale overlay copies of package files once per version. It keeps user settings and backs up everything it removes.

The full lab notes, in Portuguese, are in [`docs/tecnico-pt/`](docs/tecnico-pt/).

## Build it yourself

**Requirements:**
- Docker, with Docker Compose v2;
- about 10 GB of free disk space;
- `curl`, `unzip`, `cpio`;
- the **official Creality Hi OTA 1.1.0.74**, downloaded from Creality.

```bash
git clone https://github.com/reinaldovcaetano/creality-hi-unleashed && cd creality-hi-unleashed
# 1. put the official OTA here: insumos/CR4NU200360C20_ota_img_V1.1.0.74.img
./scripts/obter-insumos.sh            # downloads + verifies SHA-256 of every input (pinned commits)
# 2. optional, never committed:
#    pacote/local/authorized_keys     -> your SSH public key (login on port 2222)
#    pacote/local/save_config.txt     -> your printer's SAVE_CONFIG block (restored after a reset)
docker compose run --rm kbuild        # kernel + backports + AIC8800 -> pacote/p2/   (~10-15 min)
docker compose run --rm painel        # Hi Painel (on-screen UI) -> pacote/painel/   (~1 min)
docker compose run --rm pack          # OTA -> saida/CR4NU200360C20_ota_img_V1.2.1.0.img
./tests/rodar-testes.sh               # tests the boot scripts inside an emulated ARM (busybox of the image)
```

The kernel build prints the pin check (`328 ok, 0 com erro`) and checks the `vermagic` of every module. It also rebuilds the boot image with **Creality's original device tree**.

### Prebuilt build containers

The build containers are published on GitHub Packages, so you can skip building them locally:

```bash
docker compose pull      # ghcr.io/reinaldovcaetano/creality-hi-unleashed-{kbuild,pack}:alpha
```

They only contain the tools: Debian bookworm, the ARM cross compiler and squashfs-tools. No Creality files are included.

## Install

1. Use a USB stick formatted as **FAT32**, with only `CR4NU200360C20_ota_img_V1.2.1.0.img` in its root. Do not rename the file.
2. With the printer idle, plug in the stick and confirm the update on the screen.
3. **Wait about 7 minutes.** The printer reboots once into the new system, runs the boot test, and then:
   - **passes:** the new half becomes official (`logs/hi-confirma.log` says `CONFIRMADO`);
   - **fails:** it reboots back to your previous system by itself (`logs/hi-confirma-falhou.log`).
4. For 5 GHz, plug in the MT7921AU dongle and reboot.
   - The dongle becomes `wlan0` with the **same MAC as the internal Wi-Fi**, so the printer keeps its IP.
   - Without the dongle, the internal Wi-Fi is used as before.

After installing:
- **Fluidd:** `http://<printer-ip>:4408`, with the camera.
- **Moonraker:** port `7125`.
- **SSH:** port `2222`.
- **Diagnostics:** `logs/hi-diag.txt` and `logs/hi-limpa-overlay.log`.

> [!IMPORTANT]
> Do **not** install official Creality updates on top of this firmware. They would replace the kernel and the 5 GHz drivers. Use builds from this project.

## Hi Painel (the new touch screen)

- **Default screen at boot.** Creality's screen starts first; as soon as it reaches its home page, Hi Painel takes over. It does **not** take over while printing or when a USB update is plugged in, so Creality's installer works as before.
- **Power loss:** if a print was interrupted, Hi Painel shows *Continue / Discard*. It uses the same Klipper commands as Creality: `SDCARD_PRINT_FILE ... ISCONTINUEPRINT=True` and `pause_resume/cancel_continue_print`. You can also choose to decide on Creality's screen.
- **Go to Creality's screen:** the *Creality* button. **Come back:** on Creality's screen, tap the robot, then *Help*, then *Wiki*.
- **Calibrations:** each one shows the default value, lets you edit it, runs live and compares *current × new* before saving. Saving always backs up `printer.cfg` first. *Ideal speed* also updates the acceleration cap in the macros.
- **Disable it:** run `touch /mnt/UDISK/hi-painel/desligado` and reboot. A test build in `/mnt/UDISK/hi-painel/hi-painel` takes priority over the packaged one.
- **Screens preview without hardware:** `hi-painel/mock` (`make -f Makefile.mock`, `HI_MOONRAKER=<printer-ip>` for live data).

## Recovery

- **Automatic:** a failed boot test returns you to the previous half.
- **Manual switch of the boot half:**

  ```sh
  fw_setenv boot_partition bootB; fw_setenv root_partition rootfsB; reboot
  ```
- **Factory reset:** create a USB stick with an empty `wipe_all` file, as described in Creality's wiki.
- **Full re-flash over USB:**
  1. Hold the **LOAD/FEL** button on the board.
  2. Connect the Micro USB cable to a Windows PC.
  3. Flash Creality's factory image with **PhoenixSuit**.

## Known limitations (alpha)

- Tested on one printer only.
- Only the `gc9307_9bit_spi_tjc_240_320` panel variant (the one in the tested device tree) has been rebuilt.
- The `tlsc6x` touch module is disabled, because the tested unit uses a Goodix touch. To re-enable it:

  ```sh
  echo tlsc6x > /etc/modules.d/touchscreen-tlsc6x
  ```
- Only the MT7921AU (`0e8d:7961`) dongle has been tested.
- The Creality cloud is disabled on purpose.
- The bundled printer configs (`pacote/overlay/usr/share/klipper/config/...`) are the author's tuned configs, with 4300 mm/s² acceleration and macro fixes. Your own configs are **kept** if you changed them; factory configs are replaced.

## Repository layout

```
compose.yaml, docker/        build containers (kbuild = kernel, pack = OTA)
scripts/obter-insumos.sh     downloads/verifies all inputs (pinned)
kernel/patches/              linux-hi.patch (display, USB-ID, PG9 pin, I2C retry), aic8800-walnutpi-hi.patch
kernel/config/               kernel-b.config, backports.config, Creality's original config
kernel/tools/                bootimg.py (Android boot v2), conferir_pinos.py (DT x pinctrl check)
kernel/build-kernel.sh       full kernel + Wi-Fi build
pacote/build/                dentro.sh (assembles the OTA), tentativa.py (one-shot boot)
pacote/overlay*/             files added to the rootfs (init scripts, Moonraker, configs)
tests/                       ARM-emulated tests, I2C bench tools
docs/tecnico-pt/             full lab notes (Portuguese)
```

## Credits and licenses

- **Built with [Claude](https://claude.ai) (Anthropic), through Claude Code.** Claude worked alongside the author on the firmware teardown, the kernel and Wi-Fi porting, the live debugging on the printer (including the PG9/I2C bug), the build pipeline, the tests and this documentation in four languages. 🤖🤝

- **Creality:** Hi_Klipper (GPL-3.0); the OTA is Creality's and is **not** redistributed here.
- **Allwinner Tina Linux 5.4 BSP:** mirror by `bazzonrobocon` (GPL-2.0). Our kernel patches are GPL-2.0.
- **linux-backports**, **linux-firmware** and **wireless-regdb:** kernel.org.
- **walnutpi/aic8800_sdio:** GPL-2.0.
- **Moonraker** (GPL-3.0) and **Fluidd** (GPL-3.0).
- Project scripts and documentation: GPL-3.0 (see `LICENSE`).

*Creality and Creality Hi are trademarks of their owners. This is an independent community project.*
