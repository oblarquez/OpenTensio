# OpenTensio WiFi Node

Standalone ESP32 firmware for two resistive granular matrix soil water potential sensors.

## v0.1.0 compatibility

This firmware keeps the original standalone Wi-Fi prototype MQTT behavior by default:

- one JSON payload
- default topic: `jardin/sondes`
- original JSON keys: `v_bat`, `t_sol`, `rssi`, `s1_v`, `s1_r`, `s1_cb`, `s2_v`, `s2_r`, `s2_cb`

## Measurement method

The measurement logic is aligned with `OpenTensio_LoRa_TX`:

```text
GPIO/VIN_EXC -> fixed resistor -> ADS1115 input -> soil sensor -> GND
```

Each sensor divider is powered only during measurement using a short GPIO pulse:

- default settle time: 150 µs
- default discharge delay between sensors: 50 ms
- default fixed resistor: 10 kΩ
- default excitation voltage: 3.30 V

The firmware uses the same resistance conversion and the same IRROMETER-style centibar calculation as the LoRa TX firmware.

## Setup

1. Copy `config.example.h` to `config.h`.
2. Edit Wi-Fi, MQTT and pin values.
3. Open `OpenTensio_WiFi_Node.ino` in the Arduino IDE.
4. Install required libraries:
   - PubSubClient
   - Adafruit ADS1X15
   - OneWire
   - DallasTemperature
5. Flash the ESP32.

`config.h` is ignored by Git so private Wi-Fi/MQTT settings are not published.
