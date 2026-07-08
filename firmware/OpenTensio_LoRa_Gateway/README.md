# OpenTensio LoRa Gateway

This firmware receives OpenTensio LoRa packets and forwards them to MQTT.

It is intended for ESP32 / Heltec-style LoRa boards with an OLED display.

## v0.1.0 compatibility goal

OpenTensio v0.1.0 is designed to be **drop-in compatible** with the original working field deployment.

By default, it keeps:

- the original `MC;...` LoRa payload format;
- the original individual Home Assistant MQTT state topics;
- the same basic gateway behavior.

This means an existing Home Assistant dashboard using the legacy MQTT topics should continue receiving data after reflashing, provided Wi-Fi/MQTT credentials and LoRa settings are configured correctly.

## Setup

1. Copy `config.example.h` to `config.h`.
2. Edit Wi-Fi, MQTT and LoRa settings.
3. Open `OpenTensio_LoRa_Gateway.ino` with the Arduino IDE.
4. Install the required libraries.
5. Compile and upload to the gateway board.

`config.h` is intentionally ignored by Git so Wi-Fi and MQTT credentials are not published.

## Supported payload for v0.1.0

```text
MC;cb1;cb2;t_box;t_soil;v1;v2;r1;r2;vbat;t_air;h_air;p_air
```

A parser for a future versioned `OT1;node_id;...` payload may exist in the code, but `MC;...` is the default v0.1.0 format.

## Default MQTT output

The gateway publishes these individual state topics by default:

```text
homeassistant/sensor/houblon1/state
homeassistant/sensor/houblon2/state
homeassistant/sensor/temp_boitier/state
homeassistant/sensor/temp_sol/state
homeassistant/sensor/v1/state
homeassistant/sensor/v2/state
homeassistant/sensor/r1/state
homeassistant/sensor/r2/state
homeassistant/sensor/vbat/state
homeassistant/sensor/t_air/state
homeassistant/sensor/h_air/state
homeassistant/sensor/p_air/state
homeassistant/sensor/lora_rssi/state
homeassistant/sensor/lora_snr/state
```

Optional JSON output can be enabled later in `config.h`, but it is disabled by default in v0.1.0 to avoid breaking existing Home Assistant setups.
