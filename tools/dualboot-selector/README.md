# T-Deck dual boot: Meshtastic + MeshCore

A selector in the factory partition chooses at power-on which firmware starts. Only one runs at a time;
each keeps its own data partition, so switching does not touch the other's settings.

## Parts

| Image | Partition | Offset | Built from |
|---|---|---|---|
| bootloader | - | 0x0 | Meshtastic `t-deck-dualboot` (IDF 5.5, the newer of the two) |
| partition table | - | 0x8000 | `dualboot_16MB.csv` (identical in all three builds) |
| selector | factory | 0x10000 | this folder, `pio run` |
| Meshtastic | ota_0 `mt_app` | 0x110000 | Meanius/meshtastic-firmware, branch `dual-boot`, env `t-deck-dualboot` |
| MeshCore | ota_1 `mc_app` | 0x510000 | Meanius/MeshCore, branch `dual-boot`, env `LilyGo_TDeck_companion_radio_ble_dualboot` |

Data: Meshtastic LittleFS in `mt_fs` (0x810000), MeshCore SPIFFS in `mc_fs` (0xC10000); both format themselves
on first start. `otadata` is left erased, so the first boot goes to the selector.

## How it switches

- Both firmwares, built with `DUAL_BOOT_SELECTOR`, set the next boot back to the factory partition at start.
- Power-on, crash or brownout: the selector shows a 5 s menu (1/m Meshtastic, 2/c MeshCore, Enter or
  trackball press, trackball up/down), the last choice preselected, then boots it.
- Software restart or deep-sleep wake (a firmware rebooting itself): straight on to the last choice, no menu.
- Do not hold the trackball while powering on: it is GPIO0, the strapping pin (download mode).

## Flashing (whole device, settings are lost)

    esptool --chip esp32s3 erase-flash
    esptool --chip esp32s3 write-flash 0x0 tdeck-dualboot-full.bin

`merge-image.ps1` builds `tdeck-dualboot-full.bin` from the three builds on the build machine.

## Status (2026-11-04)

All three images build; not yet tried on a device.
