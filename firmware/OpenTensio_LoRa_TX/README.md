# OpenTensio LoRa TX

Low-power LoRa field transmitter for soil water tension monitoring.

## Features

- Reads 2 resistive granular matrix soil water potential sensors.
- Reads a DS18B20 soil temperature probe.
- Optionally reads a DS18B20 enclosure temperature probe.
- Optionally reads a BME280 for air temperature, humidity and pressure.
- Sends a compact LoRa payload to an OpenTensio gateway.
- Uses deep sleep between readings.

## Sensor excitation

This firmware does **not** leave the soil sensors permanently connected to 3.3 V.

Each sensor divider is powered by a dedicated GPIO only during measurement:

```text
GPIO / VIN_EXC -> 10 kOhm reference resistor -> ADS1115 input -> sensor -> GND
```

The firmware then switches the GPIO back to `LOW` before LoRa transmission and before deep sleep.

This is a short DC pulse measurement method. It greatly reduces sensor polarization compared with permanent DC excitation. For long-term reference measurements, AC excitation remains the preferred method for resistive granular matrix sensors.

## Payload format for v0.1.0

OpenTensio v0.1.0 deliberately keeps the original field payload for drop-in compatibility:

```text
MC;cb1;cb2;t_box;t_soil;v1;v2;r1;r2;vbat;t_air;h_air;p_air
```

Example:

```text
MC;32.5;41.2;24.1;18.7;1.234;1.456;5970;7890;3.92;22.5;58;1013
```

A future version may introduce a versioned payload such as `OT1;node_id;...`, but this is **not enabled by default** in v0.1.0.

## Configuration

Copy `config.example.h` to `config.h`, then adapt pins, node ID and LoRa parameters.

`config.h` is intentionally ignored by Git.
