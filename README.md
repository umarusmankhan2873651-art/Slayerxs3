# SlayerX

ESP32-S3 multi-tool firmware. WiFi / BLE / nRF24 / IR attacks + scripting.
OLED UI on 6 buttons, optional WiFi web panel.

## Build
    pio run -t upload

## Flash
Hold BOOT, tap RESET, release BOOT, then upload.

## SD layout
    /slayerx/scripts/*.txt     ducky/HID scripts
    /slayerx/ir/               saved IR signals
    /slayerx/payloads/
    /slayerx/logs/

## Web panel
Boot -> YES -> join AP `slayerX` / `slayerxusman` -> captive page opens.
Or browse http://192.168.4.1/panel

## Warnings
- nRF24 needs a 10uF cap across VCC/GND or it browns out.
- BLE + WiFi together need PSRAM (N16R8 has it).
- IR LED needs a driver transistor for good range.