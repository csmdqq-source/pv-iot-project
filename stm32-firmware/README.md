# STM32 Firmware Package

This folder contains the STM32 gateway firmware project used in the PV IoT monitoring prototype.

The firmware runs on an STM32WB5MM-DK board configured as a Zigbee coordinator. It receives Tuya EF00 data from the TOWSMR1-40 Zigbee smart breaker, formats the electrical measurements, and sends them to the cloud through an A7670E LTE Cat-1 module using MQTT.

## Firmware Role

```text
TOWSMR1-40 smart breaker
  -> Zigbee / Tuya EF00
  -> STM32WB5MM-DK coordinator
  -> UART AT commands
  -> A7670E LTE Cat-1
  -> EMQX Cloud MQTT
```

## Development Tools

| Tool | Purpose |
|------|---------|
| STM32CubeIDE | Open, build, and flash the project |
| STM32CubeProgrammer | Flash or update the STM32WB wireless stack if required |
| Serial terminal | Observe debug logs from the gateway |
| MQTTX | Optional MQTT verification client |

## Project Location

Open this project in STM32CubeIDE:

```text
stm32-firmware/Zigbee_OnOff_Server_Coord/STM32CubeIDE
```

The source files most relevant to the cloud upload are:

```text
Core/Src/a7670e.c
Core/Inc/a7670e.h
STM32_WPAN/App/app_zigbee.c
```

## MQTT Configuration

Before flashing to a real device, configure the MQTT broker and topics in the firmware source where the A7670E MQTT constants are defined.

Use placeholders in the public repository. Do not commit real EMQX usernames, passwords, or private tokens.

Expected topics:

```text
Uplink:   pv/device_001/data
Downlink: pv/device_001/control
```

Each MQTT client must use a unique Client ID. Do not reuse the same Client ID in STM32, Node-RED, and MQTTX; otherwise EMQX may disconnect one client with `Session taken over`.

## Published Payload

The current firmware payload includes active power, apparent power, and power factor:

```json
{
  "device_id": "device_001",
  "timestamp": 1550,
  "voltage": 237.8,
  "current": 1.23,
  "power": 278,
  "active_power": 278,
  "apparent_power": 292.7,
  "power_factor": 0.95,
  "status": true,
  "efficiency": 0.5,
  "temperature": 30.8
}
```

`power` is retained for backward compatibility and should be interpreted as active power.

## Expected Serial Logs

A normal power report looks like:

```text
[M4 APPLICATION] [POWER] V=237.2V I=1231mA P=277W S=292.0VA PF=0.95
[M4 APPLICATION] [TEMP] DP=131 Raw=310 -> 31.0C
```

A normal MQTT upload sequence should show connection and publish success messages from the A7670E driver.

## Build and Flash

1. Open STM32CubeIDE.
2. Import the existing STM32 project from `Zigbee_OnOff_Server_Coord/STM32CubeIDE`.
3. Build the project.
4. Connect the STM32WB5MM-DK through ST-LINK USB.
5. Flash and run the firmware.
6. Open the serial terminal at the configured baud rate and verify Zigbee pairing and MQTT upload logs.

## Hardware Notes

- The STM32 board may be powered from a laptop USB port during testing.
- If the laptop is only providing USB power, it is not part of the MQTT/cloud upload path.
- Cloud upload still depends on the A7670E cellular connection and SIM data service.
- If the SIM card has expired but data still arrives, it may be due to remaining balance, grace period, or temporary network allowance. Verify by powering the board independently and viewing Grafana from another device.

## Troubleshooting

| Symptom | Likely Cause | Check |
|---------|--------------|-------|
| No MQTT data in Node-RED | A7670E not registered or MQTT not connected | Antenna, SIM, APN, MQTT logs |
| MQTTX or Node-RED disconnects | Duplicate MQTT Client ID | Use unique Client IDs |
| Grafana shows no recent data | Cloud pipeline not writing to InfluxDB | Node-RED debug and InfluxDB token |
| No active/apparent power fields | Old firmware payload | Rebuild and flash the updated firmware |
| Zigbee data missing | Breaker not paired or Tuya EF00 not reporting | Pairing logs and breaker power |