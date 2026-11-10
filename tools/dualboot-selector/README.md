# T-Deck dual boot: Meshtastic + WADAMESH

A selector in the factory partition chooses at power-on which firmware starts. Only one runs at a time;
each keeps its own data partition, so switching does not touch the other's settings.

## Parts

| Image | Partition | Offset | Built from |
|---|---|---|---|
| bootloader | - | 0x0 | Meshtastic `t-deck-dualboot` (IDF 5.5, the newer of the two) |
| partition table | - | 0x8000 | `dualboot_16MB.csv` (identical in all three builds) |
| selector | factory (384 KB) | 0x10000 | this folder, `pio run` |
| Meshtastic | ota_0 `mt_app` (4.5 MB) | 0x70000 | Meanius/meshtastic-firmware, branch `dual-boot`, env `t-deck-tft-dualboot` (touch UI; `t-deck-dualboot` is the plain one) |
| WADAMESH | ota_1 `mc_app` (4.25 MB) | 0x4F0000 | Meanius/wadamesh, branch `dual-boot`, env `LilyGo_TDeck_companion_radio_touch_dualboot` |

Data: Meshtastic LittleFS in `mt_fs` (0x930000, 3.375 MB), WADAMESH SPIFFS in `mc_fs` (0xC90000, 3.375 MB); both format themselves
on first start. `otadata` is left erased, so the first boot goes to the selector.

Slot sizes (measured 2026-11-10, each slot at least the image + 10%): selector 301 360 B (23% free),
Meshtastic MUI 3 966 736 B (16% free), WADAMESH 3 737 424 B (16% free). The data partitions have the size
both firmwares ship with. The first version of this table (MeshCore in a 3 MB slot at 0x510000, Meanius/MeshCore
branch `dual-boot`) is not compatible with this one: changing over needs a full erase.

## How it switches

- Both firmwares, built with `DUAL_BOOT_SELECTOR`, set the next boot back to the factory partition at start.
- Power-on, crash or brownout: the selector shows a 5 s menu (1/m Meshtastic, 2/w WADAMESH, Enter or
  trackball press, trackball up/down), the last choice preselected, then boots it.
- Software restart or deep-sleep wake (a firmware rebooting itself): straight on to the last choice, no menu.
- Do not hold the trackball while powering on: it is GPIO0, the strapping pin (download mode).
- WADAMESH's own over-the-air update is off in this build (it would write ota_0, Meshtastic): update it
  over USB by writing its app image to 0x4F0000. Do not use the WADAMESH web flasher on this device.

## Flashing

Clean device (all settings are lost):

    esptool --chip esp32s3 erase-flash
    esptool --chip esp32s3 write-flash 0x0 tdeck-dualboot-wm-full.bin

The same with the parts one by one:

    esptool --chip esp32s3 erase-flash
    esptool --chip esp32s3 write-flash 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 selector.bin 0x70000 meshtastic.bin 0x4F0000 wadamesh.bin

WADAMESH only (same table already on the device):

    esptool --chip esp32s3 write-flash 0x4F0000 wadamesh.bin

`merge-image.ps1` builds `tdeck-dualboot-wm-full.bin` from the three builds on the build machine.
