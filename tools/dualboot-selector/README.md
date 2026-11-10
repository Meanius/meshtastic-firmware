# T-Deck dual boot: Meshtastic + WADAMESH

A selector in the factory partition chooses at power-on which firmware starts. Only one runs at a time;
each keeps its own data partition, so switching does not touch the other's settings.

## Parts

| Image | Partition | Offset | Built from |
|---|---|---|---|
| bootloader | - | 0x0 | Meshtastic `t-deck-dualboot` (IDF 5.5, the newer of the two) |
| partition table | - | 0x8000 | `dualboot_16MB.csv` (identical in all three builds) |
| selector | factory (1 MB) | 0x10000 | this folder, `pio run` |
| Meshtastic | ota_0 `mt_app` (3 MB) | 0x110000 | Meanius/meshtastic-firmware, branch `dual-boot`, env `t-deck-dualboot` |
| WADAMESH | ota_1 `mc_app` (4 MB) | 0x410000 | Meanius/wadamesh, branch `dual-boot`, env `LilyGo_TDeck_companion_radio_touch_dualboot` |

Data: Meshtastic LittleFS in `mt_fs` (0x810000), WADAMESH SPIFFS in `mc_fs` (0xC10000); both format themselves
on first start. `otadata` is left erased, so the first boot goes to the selector.

The first version of this table had MeshCore in a 3 MB `mc_app` at 0x510000 (Meanius/MeshCore, branch
`dual-boot`); WADAMESH does not fit 3 MB, so `mt_app` gave up 1 MB. The data partitions did not move.

## How it switches

- Both firmwares, built with `DUAL_BOOT_SELECTOR`, set the next boot back to the factory partition at start.
- Power-on, crash or brownout: the selector shows a 5 s menu (1/m Meshtastic, 2/w WADAMESH, Enter or
  trackball press, trackball up/down), the last choice preselected, then boots it.
- Software restart or deep-sleep wake (a firmware rebooting itself): straight on to the last choice, no menu.
- Do not hold the trackball while powering on: it is GPIO0, the strapping pin (download mode).
- WADAMESH's own over-the-air update is off in this build (it would write ota_0, Meshtastic): update it
  over USB by writing its app image to 0x410000. Do not use the WADAMESH web flasher on this device.

## Flashing

Clean device (all settings are lost):

    esptool --chip esp32s3 erase-flash
    esptool --chip esp32s3 write-flash 0x0 tdeck-dualboot-wm-full.bin

The full image ends below `mt_fs`, so writing it WITHOUT erase-flash keeps both data partitions; it does
overwrite `nvs` (0x9000) with its padding. Writing the parts one by one keeps `nvs` too:

    esptool --chip esp32s3 write-flash 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 selector.bin 0x110000 meshtastic.bin 0x410000 wadamesh.bin

WADAMESH only (same table already on the device):

    esptool --chip esp32s3 write-flash 0x410000 wadamesh.bin

`merge-image.ps1` builds `tdeck-dualboot-wm-full.bin` from the three builds on the build machine.
