# Custom Server and Migration Guide

This guide explains how to reproduce the cloud dashboard on another server or change the MQTT broker.

## 1. What Must Keep Running

The runtime system requires these cloud-side services:

```text
Node-RED
InfluxDB
Grafana
PostgreSQL
```

The MQTT broker may be EMQX Cloud or a self-hosted broker. The local computer is not required after deployment.

## 2. Server Options

Recommended baseline:

```text
Ubuntu 22.04 LTS
2 vCPU
2 GB RAM
40 GB disk
Public IPv4 address
```

Tested provider:

```text
Tencent Cloud Lighthouse, Frankfurt region
```

Other suitable options include AWS Lightsail, Alibaba Cloud ECS/Lighthouse, Tencent Cloud Hong Kong/Singapore, or any VPS that supports Docker.

## 3. Migrating to a New Server

### Fresh Deployment

Use this when historical data does not need to be migrated:

```bash
git clone https://github.com/<your-user>/pv-iot-project.git
cd pv-iot-project/dashboard
sudo docker-compose up -d
sudo docker exec -i postgres psql -U "$POSTGRES_USER" -d "$POSTGRES_DB" < init_device_registry.sql
```

Then import or verify Node-RED flows and Grafana dashboard.

### Migration With Existing Historical Data

If historical InfluxDB/Grafana/PostgreSQL data must be preserved, stop the stack first:

```bash
sudo docker-compose down
```

Then archive and copy these folders:

```text
dashboard/influxdb/data
dashboard/influxdb/config
dashboard/grafana/data
dashboard/node-red/data
dashboard/postgres/data
```

These folders can be large. They are not required for a clean reproducible deployment.

After copying to the new server:

```bash
sudo chown -R 1000:1000 node-red
sudo chown -R 472:472 grafana
sudo chown -R 1000:1000 influxdb
sudo chown -R 999:999 postgres
sudo docker-compose up -d
```

## 4. Changing the MQTT Broker

The current design uses EMQX Cloud as the broker. The broker is responsible for MQTT message forwarding, not long-term data storage.

### Components to Update

When changing broker address or credentials, update:

1. STM32 firmware A7670E MQTT configuration
2. Node-RED MQTT input node
3. Node-RED MQTT output node for downlink control
4. MQTTX test client, if used

### Topic Convention

```text
Uplink:   pv/device_001/data
Downlink: pv/device_001/control
```

Use a unique MQTT Client ID for each client:

```text
STM32:    pv_device_001_stm32_<uid>
Node-RED: pv_cloud_nodered_<server>
MQTTX:    mqttx_debug_<yourname>
```

Duplicate Client IDs cause broker-side disconnects.

## 5. Self-Hosted MQTT Broker Option

If EMQX Cloud is not used, a Mosquitto broker can be added to the same server.

Example `docker-compose.yml` service:

```yaml
mosquitto:
  image: eclipse-mosquitto:2
  container_name: mosquitto
  restart: always
  ports:
    - "1883:1883"
  volumes:
    - ./mosquitto/config:/mosquitto/config
    - ./mosquitto/data:/mosquitto/data
    - ./mosquitto/log:/mosquitto/log
```

Minimal `mosquitto/config/mosquitto.conf`:

```text
listener 1883
allow_anonymous true
persistence true
persistence_location /mosquitto/data/
```

For production, disable anonymous access and use username/password authentication.

## 6. Firewall Recommendations

| Port | Service | Public? |
|------|---------|---------|
| 22 | SSH | Yes, preferably limited to trusted IPs |
| 1883 | MQTT | Only if self-hosting MQTT |
| 3000 | Grafana | Yes, if remote viewing is required |
| 1880 | Node-RED editor | Temporary only |
| 8086 | InfluxDB | No |
| 5432 | PostgreSQL | No |

Node-RED continues running after port `1880` is closed. The port only controls public access to the editor.

## 7. Backup

For a quick backup:

```bash
cd dashboard
sudo docker-compose down
tar czf pv-dashboard-backup.tar.gz influxdb grafana node-red postgres docker-compose.yml
sudo docker-compose up -d
```

For a small reproducible archive without historical data, keep only:

```text
docker-compose.yml
flows.json
grafana-dashboard.json
init_device_registry.sql
insert_devices.sql
```

## 8. Verification Checklist

After migration or broker changes:

- Node-RED MQTT node is connected.
- Node-RED debug receives `pv/device_001/data`.
- InfluxDB write nodes do not show `unauthorized access`.
- PostgreSQL contains `device_001`.
- Registry synchronization flow has been triggered.
- Open-Meteo flow produces GHI data.
- Grafana data source `Save & test` succeeds.
- Grafana panels show new data in `Last 15 minutes`.
