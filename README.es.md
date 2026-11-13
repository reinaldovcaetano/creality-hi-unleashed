<h1 align="center">🖨️ Creality Hi Unleashed</h1>
<p align="center"><b>Firmware comunitario para la impresora 3D Creality Hi (F018): Wi-Fi 6 en 5 GHz, pila de kernel moderna, Moonraker y Fluidd nuevos y un instalador seguro con prueba de arranque única.</b></p>
<p align="center">
<img alt="estado" src="https://img.shields.io/badge/estado-ALFA-red">
<img alt="impresora" src="https://img.shields.io/badge/impresora-Creality%20Hi%20(F018)-blue">
<img alt="soc" src="https://img.shields.io/badge/SoC-Allwinner%20T113--i-lightgrey">
<img alt="kernel" src="https://img.shields.io/badge/kernel-5.4.61%20%2B%20backports%206.1-green">
</p>
<p align="center">
<a href="README.md">English</a> · <a href="README.pt-BR.md">Português</a> · <b>Español</b> · <a href="README.zh-CN.md">中文</a>
</p>

> [!WARNING]
> **ALFA. Úselo bajo su propio riesgo.**
> - Este firmware se ha probado en **una** Creality Hi, con la placa base `CR4NU200360C20`.
> - Reemplaza el kernel y **no** está afiliado a Creality ni avalado por ella.
> - Si la prueba de arranque falla, el instalador vuelve solo al sistema anterior, y la placa siempre se puede regrabar por USB (ver [Recuperación](#recuperación)).
> - Aun así, instálelo solo si acepta el riesgo.

---

## Qué obtiene

| | Creality de fábrica 1.1.0.74 | Creality Hi Unleashed (alfa, OTA 1.2.0.0) |
|---|---|---|
| Wi-Fi | AIC8800DC interno, **solo 2,4 GHz** | **Wi-Fi 6 en 5 GHz** con el adaptador USB MediaTek **MT7921AU** (probado: 80 MHz, 2×2, enlace de 1200 Mbit/s). El chip interno queda de respaldo. |
| Pila inalámbrica | cfg80211/mac80211 integrados en el kernel 5.4 | **linux-backports 6.1.145** (cfg80211, mac80211, mt76, mt7921u) + driver AIC8800 portado a ella |
| Kernel | binario de Creality (código no publicado) | recompilado a partir del BSP Allwinner Tina 5.4.61, con la config y el **device tree original** de Creality |
| Red cableada | no tiene | adaptador USB **RTL8153** (r8152), con DHCP al conectarlo |
| Memoria | 488 MB de RAM, sin swap | swap de **256 MB en zram** (lzo) |
| Moonraker / Fluidd | Moonraker antiguo, Fluidd 1.27 | **Moonraker (API 1.5) sobre el Python 3.9 de la impresora**, **Fluidd 1.38**, **cámara dentro de Fluidd** |
| Spam en el log | `set enabled 1` escrito cada segundo | solo se registra cuando cambia el estado |
| Nube de Creality, ADB | activados | **desactivados** (dominios de la nube bloqueados, ADB desactivado) |
| SSH | puerto 22 | **dropbear adicional en el puerto 2222**, con inicio de sesión por clave y log detallado |
| Seguridad de instalación | cambio A/B | **prueba de arranque única**: la mitad nueva arranca una vez y solo pasa a ser la oficial después de superar la prueba |

## Cómo se hizo (resumen)

1. **Anatomía del firmware.**
   - La OTA es un cpio SWUpdate ("070702") **sin firma**, con `sw-description`, `uboot`, `kernel`, un `rootfs` squashfs gzip y una lista MD5.
   - El eMMC tiene mitades A/B: `bootA/B` en p4/p5 y `rootfsA/B` en p6/p7.
   - Quien instala desde el pendrive es `upgrade-server`, que funciona sin internet.
2. **Código del kernel.**
   - Creality nunca lo publicó. Un árbol público Tina 5.4.61 del T113 coincidió con el suyo: de las 3619 opciones de config de Creality, solo faltan 6, y el **98,97 %** de sus símbolos reales existe en nuestro build.
   - Las piezas que faltaban se reconstruyeron:
     - **driver de pantalla GC9307:** recreado a partir del desensamblado del kernel de Creality, con la secuencia de inicialización idéntica (100 de 100 operaciones);
     - **pin ID del USB-A invertido:** `USB_SUNXI_USB_ID_GPIO_ACTIVE_HIGH`, que Creality usa para que el puerto USB-A funcione como host.
3. **5 GHz.**
   - El driver MT7921 no existe para la 5.4. Por eso el kernel se recompiló con `cfg80211` como módulos y con **linux-backports 6.1**.
   - El driver del **AIC8800DC** interno (walnutpi 2024_1024, cercano a la versión de Creality) se portó a la API 6.1 y se compiló para la plataforma Allwinner. Compilado para la plataforma genérica "Ubuntu", nunca encendería el chip.
4. **Instalador de intento único.**
   - El paquete **no** cambia la mitad oficial. Escribe `bootcmd=run hi_tenta`, y U-Boot arranca la mitad nueva **una sola vez**.
   - `hi-confirma` solo vuelve oficial esa mitad después de superar:
     - 90 s de arranque;
     - **60 s de Klipper "ready" sin interrupciones**;
     - una **lectura real de la EEPROM a través de Klipper**;
     - pantalla, táctil y USB-A en modo host.
   - Si algo falla, reinicia y la impresora vuelve a la mitad anterior.
   - Lección aprendida: U-Boot calcula `root=` **antes** de `bootcmd`, así que `hi_tenta` también debe definir `mmc_root`/`nand_root`.
5. **El fallo del I2C, encontrado en vivo en la impresora.**
   - Con el kernel nuevo, Klipper se detenía con `MCU 'rpi' shutdown: Unable write i2c device`.
   - Causa: **en la tabla de pines del BSP público, PG9 función 3 figura como `twi0`; lo correcto es `twi1`**. Creality lo corrigió sin publicarlo.
   - Efecto: solo SCL se asignaba al I2C y SDA (PG9) quedaba sin asignar.
   - Corregido en el kernel. Ahora `kernel/tools/conferir_pinos.py` verifica **todos los grupos de pines del device tree contra la tabla del kernel** (328/328).
   - El driver I2C también recibió reintentos: una EEPROM ocupada rechaza la lectura (NACK) con **ambos** drivers, el nuestro y el de Creality.
6. **La trampa de la capa escribible.**
   - `rootfs_data` sobrevive a las actualizaciones, así que un archivo viejo en esa capa puede ocultar el archivo nuevo del paquete.
   - `hi-limpa-overlay` elimina esas copias viejas una vez por versión, conserva los ajustes del usuario y hace copia de seguridad de todo lo que elimina.

Las notas completas del laboratorio, en portugués, están en [`docs/tecnico-pt/`](docs/tecnico-pt/).

## Cómo replicarlo (compílelo usted mismo)

**Requisitos:**
- Docker con Docker Compose v2;
- unos 10 GB libres;
- `curl`, `unzip`, `cpio`;
- la **OTA oficial 1.1.0.74 de la Creality Hi**, descargada del sitio de Creality.

```bash
git clone https://github.com/reinaldovcaetano/creality-hi-unleashed && cd creality-hi-unleashed
# 1. coloque la OTA oficial en: insumos/CR4NU200360C20_ota_img_V1.1.0.74.img
./scripts/obter-insumos.sh            # descarga y verifica el SHA-256 de todos los insumos (commits fijos)
# 2. opcional, nunca se sube a git:
#    pacote/local/authorized_keys     -> su clave pública SSH (acceso en el puerto 2222)
#    pacote/local/save_config.txt     -> el bloque SAVE_CONFIG de SU impresora (se restaura tras un reset)
docker compose run --rm kbuild        # kernel + backports + AIC8800 -> pacote/p2/   (~10-15 min)
docker compose run --rm pack          # OTA -> saida/CR4NU200360C20_ota_img_V1.2.0.0.img
./tests/rodar-testes.sh               # prueba los scripts de arranque en un ARM emulado (busybox de la imagen)
```

El build del kernel muestra la verificación de pines (`328 ok, 0 com erro`) y comprueba el `vermagic` de cada módulo. También arma la imagen de arranque con el **device tree original de Creality**.

### Contenedores de build listos

Los contenedores de build están publicados en GitHub Packages, así que no hace falta construirlos localmente:

```bash
docker compose pull      # ghcr.io/reinaldovcaetano/creality-hi-unleashed-{kbuild,pack}:alpha
```

Solo contienen las herramientas: Debian bookworm, el compilador cruzado ARM y squashfs-tools. No incluyen ningún archivo de Creality.

## Instalación

1. Use un pendrive en **FAT32**, solo con `CR4NU200360C20_ota_img_V1.2.0.0.img` en la raíz. No cambie el nombre del archivo.
2. Con la impresora detenida, conecte el pendrive y confirme la actualización en la pantalla.
3. **Espere unos 7 minutos.** La impresora reinicia una vez en el sistema nuevo, hace la prueba de arranque y entonces:
   - **si la supera:** la mitad nueva pasa a ser la oficial (`logs/hi-confirma.log` dice `CONFIRMADO`);
   - **si falla:** reinicia sola en el sistema anterior (`logs/hi-confirma-falhou.log`).
4. Para 5 GHz, conecte el adaptador MT7921AU y reinicie.
   - El adaptador pasa a ser `wlan0` con la **misma MAC que el Wi-Fi interno**, así que la IP no cambia.
   - Sin el adaptador, la impresora usa el Wi-Fi interno como antes.

Después de instalar:
- **Fluidd:** `http://<ip-de-la-impresora>:4408`, con la cámara.
- **Moonraker:** puerto `7125`.
- **SSH:** puerto `2222`.
- **Diagnóstico:** `logs/hi-diag.txt` y `logs/hi-limpa-overlay.log`.

> [!IMPORTANT]
> **No** instale actualizaciones oficiales de Creality encima de este firmware. Reemplazarían el kernel y se perderían los drivers de 5 GHz. Use builds de este proyecto.

## Recuperación

- **Automática:** si la prueba de arranque falla, la impresora vuelve a la mitad anterior.
- **Cambio manual de la mitad de arranque:**

  ```sh
  fw_setenv boot_partition bootB; fw_setenv root_partition rootfsB; reboot
  ```
- **Reset de fábrica:** prepare un pendrive con un archivo vacío llamado `wipe_all`, según la wiki de Creality.
- **Regrabación completa por USB:**
  1. Mantenga pulsado el botón **LOAD/FEL** de la placa.
  2. Conecte el cable Micro USB a un PC con Windows.
  3. Grabe la imagen de fábrica de Creality con **PhoenixSuit**.

## Limitaciones conocidas (alfa)

- Probado en una sola impresora.
- Solo se reconstruyó la variante de pantalla `gc9307_9bit_spi_tjc_240_320`, la del device tree probado.
- El módulo táctil `tlsc6x` viene desactivado, porque la unidad probada usa táctil Goodix. Para reactivarlo:

  ```sh
  echo tlsc6x > /etc/modules.d/touchscreen-tlsc6x
  ```
- Solo se probó el adaptador MT7921AU (`0e8d:7961`).
- La nube de Creality está desactivada a propósito.
- Las configs de impresora incluidas en el paquete son las configs ajustadas del autor: aceleración de 4300 mm/s² y correcciones de macros. Sus configs se **conservan** si las modificó; las de fábrica se reemplazan.

## Estructura del repositorio

```
compose.yaml, docker/        contenedores de build (kbuild = kernel, pack = OTA)
scripts/obter-insumos.sh     descarga y verifica todos los insumos (versiones fijas)
kernel/patches/              linux-hi.patch (pantalla, USB-ID, pin PG9, reintento I2C), aic8800-walnutpi-hi.patch
kernel/config/               kernel-b.config, backports.config, config original de Creality
kernel/tools/                bootimg.py (Android boot v2), conferir_pinos.py (device tree x pinctrl)
kernel/build-kernel.sh       build completo del kernel y del Wi-Fi
pacote/build/                dentro.sh (arma la OTA), tentativa.py (arranque de intento único)
pacote/overlay*/             archivos añadidos al rootfs (scripts de arranque, Moonraker, configs)
tests/                       pruebas en ARM emulado, herramientas de banco del I2C
docs/tecnico-pt/             notas completas del laboratorio (portugués)
```

## Créditos y licencias

- **Creality:** Hi_Klipper (GPL-3.0). La OTA es de Creality y **no** se redistribuye aquí.
- **BSP Allwinner Tina Linux 5.4:** espejo de `bazzonrobocon` (GPL-2.0). Nuestros parches de kernel son GPL-2.0.
- **linux-backports**, **linux-firmware** y **wireless-regdb:** kernel.org.
- **walnutpi/aic8800_sdio:** GPL-2.0.
- **Moonraker** (GPL-3.0) y **Fluidd** (GPL-3.0).
- Scripts y documentación del proyecto: GPL-3.0 (ver `LICENSE`).

*Creality y Creality Hi son marcas de sus respectivos dueños. Este es un proyecto comunitario independiente.*
