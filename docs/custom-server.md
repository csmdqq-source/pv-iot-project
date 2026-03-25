# Custom Server Configuration & Portable Dashboard

This document explains how to migrate the system to any computer and how to switch to a different MQTT broker.

---

## 1. Changing the MQTT Broker

### 1.1 Broker Options

| Option | Use Case | Pros | Cons |
|--------|----------|------|------|
| broker.emqx.io (current) | Development/testing | Free, no setup | Public, no privacy |
| Self-hosted Mosquitto | Production deployment | Full control, authentication | Requires server with public IP |
| Private EMQX deployment | Large-scale deployment | High performance, clustering | Complex configuration |
| Cloud-managed (AWS IoT Core, etc.) | Commercial deployment | High availability, auto-scaling | Pay per message |

### 1.2 Self-Hosted Mosquitto Broker (Recommended)

**Docker method (simplest):**

Add to docker-compose.yml:
```yaml
  mosquitto:
    image: eclipse-mosquitto:2
    ports:
      - "1883:1883"
      - "9001:9001"
    volumes:
      - ./mosquitto/config:/mosquitto/config
      - mosquitto-data:/mosquitto/data
      - mosquitto-log:/mosquitto/log
```

Create `mosquitto/config/mosquitto.conf`:
```
listener 1883
allow_anonymous true
persistence true
persistence_location /mosquitto/data/
log_dest file /mosquitto/log/mosquitto.log
```

To enable authentication, set `allow_anonymous` to `false` and add:
```
password_file /mosquitto/config/passwd
```

Generate password file:
```bash
docker exec -it mosquitto mosquitto_passwd -c /mosquitto/config/passwd admin
```

**Manual installation (Linux):**
```bash
sudo apt install mosquitto mosquitto-clients -y
sudo systemctl enable mosquitto
sudo systemctl start mosquitto

# Test
mosquitto_pub -h localhost -t test -m "hello"
mosquitto_sub -h localhost -t test
```

### 1.3 Update Broker Address in All Components

After switching brokers, update the address in these three locations:

**① STM32 Firmware**

Edit `a7670e.h`:
```c
#define MQTT_BROKER_HOST    "your-server-IP-or-domain"
#define MQTT_BROKER_PORT    1883
```
Recompile and flash the firmware.

**② Node-RED**

1. Open http://localhost:1880
2. Double-click any MQTT node → Edit Server configuration
3. Change Server to the new broker address
4. If authentication is enabled, enter username and password
5. Click Update → Done → Deploy

MQTT nodes to update:
- Flow 2: MQTT In node (subscribes to `pv/+/data`)
- Flow 3: MQTT Out node (publishes to `pv/{device_id}/control`)
- Simulator: MQTT Out node

**③ Verify Connection**

```bash
# Subscribe on the new broker
mosquitto_sub -h your-server-IP -t "pv/#" -v

# Check Node-RED debug panel for incoming messages
# Check STM32 serial log for "MQTT connected to new-address:1883"
```

---

## 2. Installing the Dashboard on Any Computer

### Method 1: Using Docker (Recommended — 5 minutes)

**Prerequisites:**
- [Docker Desktop](https://www.docker.com/products/docker-desktop/) installed
- Minimum 4 GB available RAM

**Steps:**

```bash
# 1. Clone the repository
git clone https://github.com/csmdqq-source/pv-iot-project.git
cd pv-iot-project/dashboard

# 2. Start all services
docker-compose up -d

# 3. Wait ~30 seconds, check status
docker ps    # Should show 4 containers in Running state

# 4. Import Node-RED flows
#    Open http://localhost:1880
#    Menu → Import → Upload flows.json → Deploy

# 5. Import Grafana dashboard
#    Open http://localhost:3000 (admin / admin123)
#    Dashboards → Import → Upload grafana-dashboard.json

# 6. Initialize PostgreSQL
docker exec -i postgres psql -U admin -d pv_system < insert_devices.sql

# 7. Update MQTT broker address if not using the public broker
#    Edit MQTT node Server configuration in Node-RED
```

**Stop and restart:**
```bash
docker-compose stop     # Stop (data preserved)
docker-compose start    # Restart
docker-compose down     # Stop and remove containers (volumes preserved)
docker-compose down -v  # Stop and delete all data (use with caution)
```

### Method 2: Manual Installation (Without Docker)

For environments where Docker is unavailable.

**2.1 Install Node-RED**
```bash
# Install Node.js (v18+)
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
sudo apt install nodejs -y

# Install Node-RED
sudo npm install -g node-red

# Install required nodes
cd ~/.node-red
npm install node-red-contrib-influxdb node-red-dashboard node-red-node-postgresql

# Start
node-red
# Access at http://localhost:1880
```

**2.2 Install InfluxDB**
```bash
# Ubuntu/Debian
wget https://dl.influxdata.com/influxdb/releases/influxdb2_2.7.1-1_amd64.deb
sudo dpkg -i influxdb2_2.7.1-1_amd64.deb
sudo systemctl enable influxdb
sudo systemctl start influxdb

# Initialize
influx setup \
  --username admin \
  --password admin123456 \
  --org pv-monitoring \
  --bucket pv_data \
  --token my-super-secret-token-12345 \
  --force

# Access at http://localhost:8086
```

**2.3 Install Grafana**
```bash
# Ubuntu/Debian
sudo apt install -y software-properties-common
wget -q -O - https://packages.grafana.com/gpg.key | sudo apt-key add -
echo "deb https://packages.grafana.com/oss/deb stable main" | sudo tee /etc/apt/sources.list.d/grafana.list
sudo apt update
sudo apt install grafana -y
sudo systemctl enable grafana-server
sudo systemctl start grafana-server

# Access at http://localhost:3000 (admin / admin)
```

**2.4 Install PostgreSQL**
```bash
sudo apt install postgresql -y
sudo -u postgres psql

# Inside psql
CREATE USER admin WITH PASSWORD 'admin123456';
CREATE DATABASE pv_system OWNER admin;
\q

# Import schema and data
psql -U admin -d pv_system -f create_tables.sql
psql -U admin -d pv_system -f insert_devices.sql
```

**2.5 Configure Connections**

For manual installation, all services use `localhost`:
- Node-RED MQTT: `broker.emqx.io:1883` (or your custom broker)
- Node-RED → InfluxDB: `http://localhost:8086`
- Node-RED → PostgreSQL: `localhost:5432`
- Grafana → InfluxDB: `http://localhost:8086`

---

## 3. Minimum System Requirements

| Resource | Docker Method | Manual Installation |
|----------|--------------|-------------------|
| OS | Windows 10+, Ubuntu 20.04+, macOS | Ubuntu 20.04+ (recommended) |
| RAM | 4 GB+ | 2 GB+ |
| Disk | 10 GB free | 5 GB free |
| CPU | Dual-core+ | Dual-core+ |
| Network | Internet required (MQTT + Open-Meteo API) | Same |

---

## 4. Firewall & Port Configuration

If the dashboard needs to be accessed from other devices (e.g., viewing Grafana on a phone), open these ports:

| Port | Service | Required? |
|------|---------|-----------|
| 1880 | Node-RED | Optional (admin only) |
| 1883 | MQTT Broker | Required (if self-hosted) |
| 3000 | Grafana | Required (dashboard access) |
| 8086 | InfluxDB | Optional (debugging only) |
| 5432 | PostgreSQL | Optional (admin only) |

**Linux firewall:**
```bash
sudo ufw allow 3000/tcp    # Grafana
sudo ufw allow 1883/tcp    # MQTT (if self-hosted)
```

**Windows firewall:** Control Panel → Windows Defender Firewall → Advanced Settings → Inbound Rules → New Rule → Port → Add 3000 and 1883.

---

## 5. Data Backup & Restore

### 5.1 Docker Backup
```bash
# Backup all data volumes
docker-compose stop
docker run --rm -v pv-iot-project_influxdb-data:/data -v $(pwd):/backup alpine tar czf /backup/influxdb-backup.tar.gz /data
docker run --rm -v pv-iot-project_grafana-data:/data -v $(pwd):/backup alpine tar czf /backup/grafana-backup.tar.gz /data
docker run --rm -v pv-iot-project_nodered-data:/data -v $(pwd):/backup alpine tar czf /backup/nodered-backup.tar.gz /data
docker run --rm -v pv-iot-project_postgres-data:/data -v $(pwd):/backup alpine tar czf /backup/postgres-backup.tar.gz /data
docker-compose start
```

### 5.2 Restore
```bash
docker-compose stop
docker run --rm -v pv-iot-project_influxdb-data:/data -v $(pwd):/backup alpine tar xzf /backup/influxdb-backup.tar.gz -C /
# Repeat for other volumes
docker-compose start
```

---

## 6. Migration Checklist

1. [ ] Install Docker Desktop on the new computer
2. [ ] Clone the GitHub repository
3. [ ] Run `docker-compose up -d`
4. [ ] Import Node-RED flows.json and configure MQTT broker address
5. [ ] Import Grafana dashboard JSON and configure InfluxDB data source
6. [ ] Initialize PostgreSQL schema and device data
7. [ ] Update MQTT broker address in STM32 firmware (if changed)
8. [ ] Verify: Node-RED receives data → InfluxDB has records → Grafana displays charts
