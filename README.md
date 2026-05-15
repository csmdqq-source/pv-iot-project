# PV IoT Monitoring System

This repository contains a reproducible implementation of a photovoltaic monitoring IoT system based on an STM32WB5MM Zigbee gateway, an A7670E LTE Cat-1 module, EMQX Cloud MQTT, Node-RED, InfluxDB, PostgreSQL, and Grafana.

The system was validated with a real smart circuit breaker reporting voltage, current, active power, apparent power, power factor, switch status, and temperature. Cloud processing is deployed on an Ubuntu server using Docker Compose, so the data pipeline continues to run even when the local development computer is turned off.

## System Architecture

```text
Smart breaker
  -> Zigbee / Tuya EF00
  -> STM32WB5MM gateway
  -> A7670E LTE Cat-1
  -> EMQX Cloud MQTT
  -> Node-RED on cloud server
  -> InfluxDB
  -> Grafana dashboard
```

Cloud-side irradiance is obtained from the Open-Meteo API. Node-RED merges measured active power with GHI and calculates PR-based fault indicators.

## Repository Structure

```text
README.md
hardware/
  BOM_list.md
  system_architecture.png
  wiring diagrams,.png
  photos of the assembled system.png
  STM32-schematic.pdf
stm32-firmware/
  README.md
  Zigbee_OnOff_Server_Coord/
dashboard/
  docker-compose.yml
  flows.json
  grafana-dashboard.json
  init_device_registry.sql
  insert_devices.sql
  README.md
docs/
  install-stm32.md
  install-dashboard.md
  custom-server.md
```

## Hardware Requirements

- STM32WB5MM-DK development board
- A7670E LTE Cat-1 module with antenna and valid SIM data service
- TOWSMR1-40 Zigbee smart circuit breaker
- 5 V USB power supply
- AC resistive load or PV/inverter output for validation
- Optional: MQTTX for MQTT debugging

## Software Requirements

- STM32CubeIDE for firmware build and flashing
- Docker and docker-compose on the cloud server
- A cloud Ubuntu server, tested with:
  - Tencent Cloud Lighthouse
  - Region: Frankfurt
  - Ubuntu 22.04 LTS
  - 2 vCPU / 2 GB RAM / 40 GB SSD
- EMQX Cloud MQTT broker

## Quick Start

1. Flash the STM32 firmware.

   See [docs/install-stm32.md](docs/install-stm32.md).

2. Prepare the cloud server and start the dashboard stack.

   See [docs/install-dashboard.md](docs/install-dashboard.md).

3. Configure the MQTT broker in Node-RED.

   The default uplink topic is:

   ```text
   pv/device_001/data
   ```

   The default downlink control topic is:

   ```text
   pv/device_001/control
   ```

4. Initialize the device registry.

   ```bash
   cd dashboard
   sudo docker exec -i postgres psql -U admin -d pv_system < init_device_registry.sql
   ```

5. Open Grafana.

   ```text
   http://<server-ip>:3000
   ```

   Default login:

   ```text
   Use the Grafana account configured in `dashboard/.env`.
   ```

6. Verify that Node-RED receives MQTT messages and Grafana displays active power, apparent power, power factor, voltage, current, and GHI.

## MQTT Payload

The current firmware publishes one JSON payload to `pv/device_001/data`:

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

`power` is retained for backward compatibility and is treated as active power.

## Verified Cloud Deployment

The system has been reproduced on a Tencent Cloud Ubuntu server. The local computer is only used for configuration and debugging. Once deployed, the cloud services continue to run independently:

```text
Node-RED -> InfluxDB -> Grafana -> browser / phone
```

The local computer may be turned off after deployment. Real hardware data continues only if the STM32 gateway is powered and the A7670E SIM card can still access the cellular network.

## Security Notes

For cloud deployment, open only the ports needed for operation:

| Port | Service | Recommendation |
|------|---------|----------------|
| 22 | SSH | Keep open for administration |
| 3000 | Grafana | Keep open if remote dashboard access is required |
| 1880 | Node-RED editor | Open only during debugging, then close |
| 8086 | InfluxDB | Do not expose publicly |
| 5432 | PostgreSQL | Do not expose publicly |

Change the default Grafana password before long-term operation.

## Documentation

- [STM32 firmware installation](docs/install-stm32.md)
- [Cloud dashboard installation](docs/install-dashboard.md)
- [Custom server and migration guide](docs/custom-server.md)
- [Dashboard package notes](dashboard/README.md)
