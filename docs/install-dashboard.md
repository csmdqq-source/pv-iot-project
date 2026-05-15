# Cloud Dashboard Installation Guide

This guide describes the reproducible cloud deployment that was validated on a Tencent Cloud Ubuntu server. It starts Node-RED, InfluxDB, Grafana, and PostgreSQL using Docker Compose.

## 1. Target Architecture

```text
STM32WB5MM + A7670E
  -> EMQX Cloud MQTT
  -> Node-RED on Ubuntu cloud server
  -> InfluxDB
  -> Grafana
```

The local computer is not part of the runtime path. After deployment, the cloud server continues collecting data even if the local PC is turned off.

## 2. Cloud Server

The tested configuration was:

```text
Provider: Tencent Cloud Lighthouse
Region: Frankfurt
OS: Ubuntu 22.04 LTS
CPU/RAM: 2 vCPU / 2 GB
Disk: 40 GB SSD
```

Minimum practical configuration:

```text
1 vCPU / 1 GB RAM: possible but tight
2 vCPU / 2 GB RAM: recommended
```

## 3. Firewall Rules

Open these ports in the cloud firewall:

| Port | Protocol | Purpose | Keep Open? |
|------|----------|---------|------------|
| 22 | TCP | SSH administration | Yes |
| 3000 | TCP | Grafana dashboard | Yes |
| 1880 | TCP | Node-RED editor | Only during debugging |
| 80 | TCP | Optional HTTP | Optional |
| ICMP | Ping | Connectivity test | Optional |

Do not expose InfluxDB `8086` or PostgreSQL `5432` to the public Internet. They are accessed internally through Docker.

## 4. Install Docker

Log in to the server as `ubuntu` and run:

```bash
sudo apt update
sudo apt install -y docker.io docker-compose
sudo systemctl enable docker
sudo systemctl start docker
sudo usermod -aG docker ubuntu
```

Either log out and log back in, or use `sudo` for Docker commands:

```bash
sudo docker --version
docker-compose --version
```

## 5. Copy the Repository

Clone from GitHub:

```bash
git clone https://github.com/<your-user>/pv-iot-project.git
cd pv-iot-project/dashboard
cp .env.example .env
# Edit .env and replace placeholder passwords/tokens before deployment.
```

If uploading files manually with SFTP, upload the `dashboard` folder and then run:

```bash
cd ~/pv-iot-project/dashboard
mkdir -p node-red/data grafana/data grafana/provisioning influxdb/data influxdb/config postgres/data
sudo chown -R 1000:1000 node-red
sudo chown -R 472:472 grafana
sudo chown -R 1000:1000 influxdb
sudo chown -R 999:999 postgres
```

Do not upload local database history folders unless historical data is required. A fresh deployment starts with an empty InfluxDB and PostgreSQL.

## 6. Start Services

```bash
sudo docker-compose up -d
sudo docker ps
```

Expected containers:

```text
nodered
influxdb
grafana
postgres
```

Useful logs:

```bash
sudo docker logs nodered --tail 100
sudo docker logs influxdb --tail 100
sudo docker logs grafana --tail 100
```

## 7. Initialize PostgreSQL Device Registry

The Open-Meteo irradiance flow requires active device coordinates. Initialize the registry:

```bash
sudo docker exec -i postgres psql -U "$POSTGRES_USER" -d "$POSTGRES_DB" < init_device_registry.sql
```

Expected result includes:

```text
device_001 | 34.678 | 33.038 | limassol | real | active
```

Optional simulated devices:

```bash
sudo docker exec -i postgres psql -U "$POSTGRES_USER" -d "$POSTGRES_DB" < insert_devices.sql
```

## 8. Configure Node-RED

Open:

```text
http://<server-ip>:1880
```

Import `flows.json` if it is not already loaded.

Install missing nodes if required:

```text
node-red-contrib-influxdb
node-red-contrib-postgresql
node-red-dashboard
```

### MQTT

Configure the MQTT input node:

```text
Broker: your EMQX Cloud broker address
Port: 1883 or 8883, depending on your EMQX setting
Client ID: unique value, not reused by MQTTX or STM32
Topic: pv/device_001/data or pv/+/data
QoS: 0
```

Client ID conflicts cause MQTT disconnects such as `Session taken over`.

### InfluxDB Nodes

All InfluxDB nodes must use:

```text
Version: 2.0
URL: http://influxdb:8086
Organization: pv-monitoring
Bucket: pv_data
Token: value of INFLUXDB_ADMIN_TOKEN in dashboard/.env
```

If the token is wrong, Node-RED debug shows:

```text
unauthorized access
```

### PostgreSQL Nodes

Use:

```text
Host: postgres
Port: 5432
Database: pv_system
Username: value of POSTGRES_USER in dashboard/.env
Password: value of POSTGRES_PASSWORD in dashboard/.env
```

### Registry and Irradiance Sync

After initializing PostgreSQL:

1. Go to the `Device Registry` flow.
2. Trigger the registry synchronization inject node.
3. Confirm the debug message indicates that `device_001` and irradiance points are loaded.
4. Go to `Solar Irradiance (Open-Meteo)`.
5. Trigger the live Open-Meteo inject node.

If `irradiance_points is empty` appears, the device registry has not been synchronized.

## 9. Configure Grafana

Open:

```text
http://<server-ip>:3000
```

Default login:

```text
Use the Grafana username and password configured in dashboard/.env
```

Create or edit the InfluxDB data source:

```text
Query language: Flux
URL: http://influxdb:8086
Organization: pv-monitoring
Token: value of INFLUXDB_ADMIN_TOKEN in dashboard/.env
Default bucket: pv_data
```

Click `Save & test`.

Import:

```text
grafana-dashboard.json
```

The dashboard uses 5-minute aggregation for smoother charts and lower query load.

## 10. Verification

### MQTT Reception

In Node-RED debug, check incoming messages from:

```text
pv/device_001/data
```

Expected payload:

```json
{
  "device_id": "device_001",
  "voltage": 237.8,
  "current": 1.23,
  "power": 278,
  "active_power": 278,
  "apparent_power": 292.7,
  "power_factor": 0.95,
  "status": true,
  "temperature": 30.8
}
```

### InfluxDB Write

If Grafana shows `unauthorized`, re-check the InfluxDB token in both Grafana and Node-RED.

### Grafana Display

Set time range to `Last 15 minutes` or `Last 1 hour`. Confirm:

- Active power / apparent power / PF
- Voltage and current
- Breaker status
- GHI irradiance
- Active power vs GHI scatter plot

### PC-Off Test

After deployment, turn off the local computer or disconnect it from the network. Data should continue if:

- STM32 is powered
- A7670E has cellular network access
- EMQX Cloud is reachable
- Tencent Cloud server is running

## 11. Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| Browser cannot open `:3000` | Local network blocks non-standard ports | Try mobile hotspot or map Grafana to port 80 |
| Browser cannot open `:1880` | Node-RED firewall port closed | Temporarily open TCP 1880 |
| Node-RED MQTT disconnects | Duplicate MQTT Client ID | Use unique Client ID for STM32, MQTTX, and Node-RED |
| Node-RED InfluxDB error `unauthorized access` | Wrong token/org/bucket | Use the values configured in dashboard/.env: `INFLUXDB_ADMIN_TOKEN`, `INFLUXDB_ORG`, and `INFLUXDB_BUCKET` |
| GHI panel has no data | Device registry not initialized or not synced | Run `init_device_registry.sql`, trigger registry sync |
| Grafana shows no data after deployment | New cloud InfluxDB is empty | Wait for new MQTT data or publish a test payload |
| Cloud works but laptop cannot see Node-RED | Port 1880 closed or blocked | Use Grafana for monitoring; open 1880 only when editing flows |
