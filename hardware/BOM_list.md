# Bill of Materials (BOM)

| # | Component | Model / Description | Qty | Role |
|---|-----------|---------------------|-----|------|
| 1 | Edge gateway development board | STM32WB5MM-DK | 1 | Zigbee coordinator and gateway controller |
| 2 | LTE Cat-1 module | SIMCom A7670E | 1 | Cellular MQTT uplink |
| 3 | Smart circuit breaker | TOWSMR1-40 Zigbee breaker | 1 | Electrical measurement and breaker control |
| 4 | SIM card | 4G data SIM | 1 | Network access for A7670E |
| 5 | USB power supply / cable | 5 V supply | 1 | STM32 and A7670E power during testing |
| 6 | Resistive load | Laboratory resistor/load bank | 1 set | Controlled validation load |
| 7 | Digital multimeter | Laboratory instrument | 1+ | Reference electrical measurement |
| 8 | Wiring and connectors | Jumper wires, AC wiring, antenna | As needed | Prototype assembly |

## Notes

- The smart breaker is the source of voltage, current, active power, apparent power, power factor, switch status, and temperature data.
- The A7670E requires a working antenna and SIM data service for independent cloud upload.
- During lab testing, USB from a laptop can power the gateway. This does not mean the laptop is uploading data; the MQTT uplink is still performed by the A7670E.
- Prices are intentionally omitted from the public repository because they depend on supplier, region, and laboratory availability.