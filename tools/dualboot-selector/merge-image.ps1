# One flash image for the T-Deck dual boot: bootloader (Meshtastic's, IDF 5.5), the shared partition table,
# the selector (factory, 0x10000), Meshtastic with the touch UI (ota_0, 0x70000) and WADAMESH (ota_1, 0x4F0000).
# otadata stays erased, so the first boot goes to the selector. Write it to an ERASED flash (erase-flash first).
$py = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
$t = 'C:\bokibot\tdeck'
$out = "$t\out-wm"
New-Item -ItemType Directory -Force $out | Out-Null
$mtDir = "$t\meshtastic\.pio\build\t-deck-tft-dualboot"
$mt = Get-ChildItem "$mtDir\firmware-t-deck-tft-dualboot-*.bin" | Where-Object { $_.Name -notlike '*factory*' } | Select-Object -First 1
$wm = "$t\wadamesh\.pio\build\LilyGo_TDeck_companion_radio_touch_dualboot\firmware.bin"
$sel = "$t\selector\.pio\build\tdeck-selector\firmware.bin"
Copy-Item "$mtDir\bootloader.bin" "$out\bootloader.bin" -Force
Copy-Item "$mtDir\partitions.bin" "$out\partitions.bin" -Force
Copy-Item $mt.FullName "$out\meshtastic.bin" -Force
Copy-Item $wm "$out\wadamesh.bin" -Force
Copy-Item $sel "$out\selector.bin" -Force
& $py -m esptool --chip esp32s3 merge-bin -o "$out\tdeck-dualboot-wm-full.bin" --flash-mode keep --flash-freq keep --flash-size 16MB `
  0x0 "$out\bootloader.bin" 0x8000 "$out\partitions.bin" 0x10000 "$out\selector.bin" `
  0x70000 "$out\meshtastic.bin" 0x4F0000 "$out\wadamesh.bin" 2>&1 | Select-Object -Last 4
Get-ChildItem $out | ForEach-Object { "{0} {1} {2}" -f $_.Name, $_.Length, (Get-FileHash -Algorithm MD5 $_.FullName).Hash }
