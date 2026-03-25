# STM32WB5MM-DK Firmware Installation Guide

## 1. Development Environment Setup

### Required Tools
| Tool | Version | Purpose |
|------|---------|---------|
| STM32CubeIDE | 1.13+ | Compile and flash firmware |
| STM32CubeMX | 6.9+ | Pin configuration and code generation |
| ST-LINK Driver | V2 | USB debugger driver |
| Serial Terminal | Tera Term / PuTTY | View debug logs |

### Installation Steps
1. Download and install [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html)
2. Install ST-LINK USB driver (prompted during IDE installation)
3. Connect the STM32WB5MM-DK board via USB cable (use the USB ST-LINK port, not USB USER)
4. Open Device Manager and confirm "STMicroelectronics STLink Virtual COM Port" is recognized

## 2. Firmware Configuration

### 2.1 Zigbee Configuration
- Role: **Coordinator** (creates and manages the network)
- Channel: Auto-select
- Join Policy: Permit Join enabled for 60 seconds after power-on
- Breaker Cluster Configuration:
  - Cluster 0xEF00 (Tuya private protocol) — receive electrical data
  - ZCL OnOff Cluster — send ON/OFF control commands

### 2.2 LPUART1 Configuration (Communication with A7670E)
```
Pin Assignment:
  PB5 (Pin 19) → LPUART1_TX → Connect to A7670E RXD
  PC0 (Pin 12) → LPUART1_RX → Connect to A7670E TXD
  GND           → Common ground

UART Parameters:
  Baud rate: 115200
  Data bits: 8
  Stop bits: 1
  Parity:    None
  Flow ctrl: None
```

### 2.3 UART Ring Buffer
The firmware implements a circular ring buffer for LPUART1 reception to prevent data loss during high-throughput periods (e.g., when A7670E sends URC notifications while the MCU is processing Zigbee data). The ring buffer is configured with a default size of 512 bytes and uses DMA-based reception with idle line detection to trigger buffer reads. This ensures no incoming bytes are dropped even when the main loop is busy with Zigbee stack processing or OLED display updates.

### 2.4 Key Constants
Modify the following constants in the firmware source code:

```c
// MQTT configuration in a7670e.h
#define MQTT_BROKER_HOST    "broker.emqx.io"
#define MQTT_BROKER_PORT    1883
#define MQTT_CLIENT_PREFIX  "pv_device_001_stm32_"
#define MQTT_PUB_TOPIC      "pv/device_001/data"
#define MQTT_SUB_TOPIC      "pv/device_001/control"
#define MQTT_PUB_INTERVAL   10000  // Publish interval in milliseconds
```

To switch to a different MQTT broker, only `MQTT_BROKER_HOST` and `MQTT_BROKER_PORT` need to be changed.

## 3. Compile and Flash

### 3.1 Open Project
1. Launch STM32CubeIDE
2. File → Import → Existing Projects into Workspace
3. Select the firmware project folder (containing the `.project` file)
4. Click Finish

### 3.2 Build
1. Right-click the project → Build Project (or Ctrl+B)
2. Verify the Console displays `Build Finished. 0 errors`
3. The compiled binary is located in the `Debug/` or `Release/` directory

### 3.3 Flash
1. Ensure the board is connected via USB and recognized
2. Run → Debug As → STM32 C/C++ Application
3. Select ST-LINK when prompted for the first time
4. Wait for flashing to complete; the firmware starts automatically

## 4. First Test

### 4.1 Verify Zigbee Pairing
1. Open a serial terminal, connect to the STM32 virtual COM port (baud rate 115200)
2. Power on the STM32
3. Power on the TOWSMR1-40 breaker
4. Wait for the following log output (approximately 10–20 seconds):

```
[M4 APPLICATION] Send command Permit join during 60s
[M4 APPLICATION] Device joined: 0x6d38
[M4 APPLICATION] Tuya device detected (0xEF00)
[M4 APPLICATION] Tuya binding SUCCESS!
[M4 APPLICATION] Coordinator can now receive DP reports
```

`Tuya binding SUCCESS!` confirms successful Zigbee pairing.

### 4.2 Verify MQTT Connection
After pairing, the A7670E module automatically connects to the MQTT broker:

```
[A7670E] ACCQ OK
[A7670E] MQTT connected to broker.emqx.io:1883
[A7670E] Subscribed to: pv/device_001/control
[POWER] V=239.2V I=0mA P=0.0W
```

`MQTT connected` and `Subscribed to` confirm a successful MQTT session.

### 4.3 Verify Data Publishing
On another computer, use an MQTT client (e.g., MQTTX) to subscribe to `pv/device_001/data`. You should receive JSON payloads:

```json
{
  "device_id": "device_001",
  "voltage": 239.2,
  "current": 0.0,
  "power": 0.0,
  "status": 1,
  "temperature": 31.6
}
```

## 5. Hardware Wiring Summary

```
STM32WB5MM-DK        A7670E 4G Module
  PB5 (TX)  --------→  RXD
  PC0 (RX)  ←--------  TXD
  GND       ←-------→  GND

Power: Both devices share a single DC 5V USB power source

STM32 Antenna: Built-in PCB antenna (integrated in STM32WB5MMG module)
A7670E Antenna: External 4G wideband antenna (SMA connector)

TOWSMR1-40 Breaker: Connected wirelessly via Zigbee, wired in series with 240V AC line
```

## 6. Troubleshooting

| Issue | Solution |
|-------|----------|
| Zigbee pairing fails | Power cycle both STM32 and breaker, wait 60 seconds |
| MQTT connection timeout | Check A7670E 4G antenna connection and SIM card data plan |
| No serial output | Ensure USB is connected to ST-LINK port, baud rate set to 115200 |
| Build errors | Verify STM32CubeIDE version ≥ 1.13 and Zigbee libraries are installed |
| Re-pairing needed after power loss | Normal behavior — STM32 Zigbee state is not persisted across reboots |
