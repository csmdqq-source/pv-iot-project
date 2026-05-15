# Hardware Package

This folder documents the physical components used in the PV IoT monitoring prototype.

## Included Files

| File | Purpose |
|------|---------|
| `BOM_list.md` | Bill of materials for the prototype |
| `system_architecture.png` | High-level hardware and cloud architecture |
| `wiring diagrams,.png` | Wiring overview for STM32, A7670E, and external power/load connections |
| `photos of the assembled system.png` | Photo of the assembled prototype |
| `STM32-schematic.pdf` | STM32WB5MM-DK reference schematic |

## Prototype Hardware

The validated prototype uses:

- STM32WB5MM-DK as the Zigbee coordinator and gateway controller
- A7670E LTE Cat-1 module for MQTT uplink through cellular network
- TOWSMR1-40 Zigbee smart circuit breaker for voltage, current, active power, apparent power, power factor, switch state, and temperature measurements
- USB 5 V supply for STM32/A7670E during laboratory testing
- Resistive AC load for controlled validation

## Runtime Data Path

```text
Smart breaker
  -> Zigbee / Tuya EF00
  -> STM32WB5MM gateway
  -> A7670E LTE Cat-1
  -> EMQX Cloud MQTT
  -> Cloud Node-RED / InfluxDB / Grafana
```

The local computer may be used only as a USB power source and serial log monitor. It is not part of the cloud data pipeline.