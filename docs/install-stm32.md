# STM32WB5MM-DK Firmware Installation Guide

This guide explains how to build, flash, and verify the STM32WB5MM gateway firmware.

## 1. Development Environment

Required tools:

| Tool | Purpose |
|------|---------|
| STM32CubeIDE 1.13+ | Build and flash firmware |
| STM32CubeProgrammer | Flash wireless stack if required |
| ST-LINK driver | Debug and serial access |
| Serial terminal | View logs |
| MQTTX | Optional MQTT verification |

## 2. Hardware Connections

```text
STM32WB5MM-DK        A7670E LTE Cat-1
PB5 / LPUART1_TX  -> A7670E RXD
PC0 / LPUART1_RX  -> A7670E TXD
GND               -> GND
5 V               -> A7670E VCC, if powered from the same USB supply
```

The STM32 board may be powered by a computer USB port. In this case the computer is only a power source and optional serial logger. MQTT upload is still performed through:

```text
STM32 -> A7670E -> cellular network -> EMQX Cloud
```

## 3. Firmware Configuration

Configure the MQTT settings in the firmware source where the A7670E MQTT constants are defined:

```c
#define MQTT_BROKER_HOST      "your-emqx-cloud-host"
#define MQTT_BROKER_PORT      1883
#define MQTT_DEVICE_ID        "device_001"
#define MQTT_TOPIC            "pv/device_001/data"
#define MQTT_CONTROL_TOPIC    "pv/device_001/control"
```

Use a unique MQTT Client ID for the STM32. Do not reuse the MQTTX or Node-RED Client ID.

## 4. MQTT Payload

The current payload contains both direct breaker values and derived electrical values:

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

`power` is retained for compatibility and is treated as active power. `apparent_power` is calculated as voltage multiplied by current. `power_factor` is calculated as active power divided by apparent power when apparent power is greater than zero.

## 5. Build and Flash

1. Open STM32CubeIDE.
2. Import the firmware project from:

   ```text
   stm32-firmware/Zigbee_OnOff_Server_Coord/STM32CubeIDE
   ```

3. Build the project.
4. Flash with ST-LINK.
5. Open the serial terminal at 115200 baud.

If the Zigbee wireless stack is not present on the M0+ core, flash the STM32WB Zigbee FFD wireless stack using STM32CubeProgrammer before running the application.

## 6. Zigbee Verification

Expected logs:

```text
Permit join
Device joined
Tuya device detected
Tuya binding SUCCESS
Coordinator can now receive DP reports
```

Breaker data logs should include:

```text
[POWER] V=237.2V I=1231mA P=277W S=292.0VA PF=0.95
[TEMP] DP=131 Raw=309 -> 30.9C
```

## 7. MQTT Verification

Expected A7670E/MQTT logs:

```text
MQTT connected
Subscribed to: pv/device_001/control
MQTT publish OK
```

Use MQTTX to subscribe:

```text
Topic: pv/device_001/data
```

If MQTTX disconnects with:

```text
Session taken over
```

change the MQTTX Client ID. Each MQTT client must use a unique Client ID.

## 8. Cloud Verification

After the cloud dashboard is running:

1. Open Node-RED on the cloud server.
2. Confirm the MQTT input node is connected.
3. Confirm debug receives `pv/device_001/data`.
4. Open Grafana and set time range to `Last 15 minutes`.
5. Confirm active power, apparent power, PF, voltage, and current are updating.

## 9. Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| No MQTT upload | SIM has no data service or A7670E not registered | Check antenna, SIM, registration logs |
| MQTT disconnects repeatedly | Duplicate Client ID | Change MQTTX/Node-RED Client ID |
| STM32 powered but no upload | USB only powers MCU; A7670E not powered | Confirm shared 5 V and common GND |
| No Zigbee data | Breaker not paired | Re-power breaker and permit join again |
| Grafana has no data | Cloud pipeline not receiving or writing | Check Node-RED debug and InfluxDB token |
