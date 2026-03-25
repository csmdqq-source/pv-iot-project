# Dashboard Installation Guide (Node-RED + InfluxDB + Grafana)

## 1. Docker-Based Installation (Recommended)

### 1.1 Prerequisites
- Operating System: Windows 10/11, Ubuntu 20.04+, or macOS
- [Docker Desktop](https://www.docker.com/products/docker-desktop/) installed (Windows/Mac) or Docker Engine + docker-compose (Linux)
- Minimum 4 GB available RAM, 10 GB disk space

### 1.2 Start Services

```bash
# Clone the repository
git clone https://github.com/csmdqq-source/pv-iot-project.git
cd pv-iot-project/dashboard

# Start all services in the background
docker-compose up -d

# Verify running status (should show 4 containers)
docker ps
```

### 1.3 Service Access

| Service | URL | Username | Password |
|---------|-----|----------|----------|
| Node-RED | http://localhost:1880 | — | — |
| Node-RED Dashboard | http://localhost:1880/ui | — | — |
| Grafana | http://localhost:3000 | admin | admin123 |
| InfluxDB | http://localhost:8086 | admin | admin123456 |
| PostgreSQL | localhost:5432 | admin | admin123456 |

### 1.4 docker-compose.yml Overview

```yaml
version: '3'
services:
  influxdb:
    image: influxdb:2.7
    ports:
      - "8086:8086"
    environment:
      - DOCKER_INFLUXDB_INIT_MODE=setup
      - DOCKER_INFLUXDB_INIT_USERNAME=admin
      - DOCKER_INFLUXDB_INIT_PASSWORD=admin123456
      - DOCKER_INFLUXDB_INIT_ORG=pv-monitoring
      - DOCKER_INFLUXDB_INIT_BUCKET=pv_data
      - DOCKER_INFLUXDB_INIT_ADMIN_TOKEN=my-super-secret-token-12345
    volumes:
      - influxdb-data:/var/lib/influxdb2

  nodered:
    image: nodered/node-red:latest
    ports:
      - "1880:1880"
    volumes:
      - nodered-data:/data
    depends_on:
      - influxdb

  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin123
    volumes:
      - grafana-data:/var/lib/grafana
    depends_on:
      - influxdb

  postgres:
    image: postgres:15
    ports:
      - "5432:5432"
    environment:
      - POSTGRES_USER=admin
      - POSTGRES_PASSWORD=admin123456
      - POSTGRES_DB=pv_system
    volumes:
      - postgres-data:/var/lib/postgresql/data

volumes:
  influxdb-data:
  nodered-data:
  grafana-data:
  postgres-data:
```

## 2. Node-RED Configuration

### 2.1 Import Flows
1. Open http://localhost:1880
2. Click the hamburger menu (top-right) → Import
3. Select the `dashboard/flows.json` file and click Import
4. Click the red **Deploy** button to activate the flows

### 2.2 Install Required Nodes
If any nodes appear red after import (missing dependencies), install them:
1. Menu → Manage Palette → Install
2. Search and install:
   - `node-red-contrib-influxdb` (InfluxDB connector)
   - `node-red-dashboard` (Dashboard UI)
   - `node-red-node-postgresql` (PostgreSQL connector)

### 2.3 Configure MQTT Broker Connection
1. Double-click any MQTT In node
2. Click the pencil icon next to Server
3. Configure:
   - Server: `broker.emqx.io`
   - Port: `1883`
   - Client ID: leave blank (auto-generated)
   - Protocol: MQTT V3.1.1
4. Click Update → Done → Deploy

### 2.4 Flow Descriptions

| Flow | Function | Key Nodes |
|------|----------|-----------|
| Flow 1 | Irradiance collection | Fetches GHI data from Open-Meteo API every 10 minutes for 8 grid points |
| Flow 2 | PV monitoring pipeline | MQTT In → Validate & Transform → PR calculation → Anomaly detection → InfluxDB |
| Flow 3 | Remote control | Dashboard button → MQTT Publish control command → Log to control_log |
| Flow 4 | Device registry sync | Queries PostgreSQL every 5 minutes, syncs to global variables |

### 2.5 Configure InfluxDB Connection
1. Double-click any InfluxDB Out node
2. Configure:
   - Version: 2.0
   - URL: `http://influxdb:8086` (inside Docker) or `http://localhost:8086` (external)
   - Token: `my-super-secret-token-12345`
   - Organization: `pv-monitoring`
   - Bucket: `pv_data`

### 2.6 Configure PostgreSQL Connection
1. Double-click the PostgreSQL node
2. Configure:
   - Host: `postgres` (inside Docker) or `localhost` (external)
   - Port: `5432`
   - Database: `pv_system`
   - Username: `admin`
   - Password: `admin123456`

## 3. Grafana Configuration

### 3.1 Add InfluxDB Data Source
1. Open http://localhost:3000, log in (admin / admin123)
2. Left menu → Connections → Data Sources → Add data source
3. Select InfluxDB
4. Configure:
   - Query Language: **Flux**
   - URL: `http://influxdb:8086` (inside Docker)
   - Organization: `pv-monitoring`
   - Token: `my-super-secret-token-12345`
   - Default Bucket: `pv_data`
5. Click Save & Test — a green banner confirms success

### 3.2 Import Dashboard
1. Left menu → Dashboards → Import
2. Upload `dashboard/grafana-dashboard.json`
3. Select the InfluxDB data source just created
4. Click Import

### 3.3 Dashboard Panel Descriptions

| Panel | Type | Data Source | Description |
|-------|------|------------|-------------|
| Real-time Power | Time Series | breaker_data.power | Power over time |
| Solar Irradiance GHI | Time Series | breaker_data.ghi | GHI over time |
| Voltage & Current | Time Series (dual Y-axis) | breaker_data.voltage/current | Green bars = voltage, yellow line = current |
| Power/GHI Scatter | XY Chart | breaker_data (X=ghi, Y=power) | For PR analysis |
| Breaker Status | Stat | breaker_data.status | Shows ON/OFF state |
| Geo Map | GeoMap | breaker_data (lat, lon, power) | Device locations with power heatmap |
| Alert History | Table | alerts | Detected anomaly events |

### 3.4 Dashboard Variable Configuration
The dashboard uses a `device_id` variable for device switching:
- Variable name: `device_id`
- Query type: Query
- Data source: InfluxDB
- Query: `import "influxdata/influxdb/schema" schema.tagValues(bucket: "pv_data", tag: "device_id")`

All panel Flux queries filter by `${device_id}`.

## 4. PostgreSQL Device Registry Initialization

### 4.1 Create Table Schema
```bash
docker exec -it postgres psql -U admin -d pv_system
```

```sql
CREATE TABLE device_registry (
    device_id VARCHAR(50) PRIMARY KEY,
    latitude DOUBLE PRECISION NOT NULL,
    longitude DOUBLE PRECISION NOT NULL,
    name VARCHAR(100),
    location VARCHAR(100),
    k_factor DOUBLE PRECISION DEFAULT 0.15,
    panel_power_wp INTEGER DEFAULT 0,
    device_type VARCHAR(20) DEFAULT 'real',
    status VARCHAR(20) DEFAULT 'active',
    install_date DATE,
    notes TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_location ON device_registry(location);
CREATE INDEX idx_status ON device_registry(status);
CREATE INDEX idx_coords ON device_registry(latitude, longitude);
```

### 4.2 Import Simulated Devices
```bash
docker exec -i postgres psql -U admin -d pv_system < dashboard/insert_devices.sql
```

## 5. Verify Installation

### 5.1 Check Data Flow
1. Node-RED → Debug panel should show incoming MQTT messages
2. InfluxDB → Data Explorer: query `from(bucket:"pv_data") |> range(start:-1h)` should return data
3. Grafana → Select a device; panels should display data curves

### 5.2 Check Simulator
1. After deploying the simulator flow in Node-RED, 50 simulated devices begin generating data
2. The Grafana Geo Map panel should show device markers across 5 cities in Cyprus

## 6. Troubleshooting

| Issue | Solution |
|-------|----------|
| Container fails to start | Run `docker-compose logs <service>` to inspect error logs |
| Node-RED cannot reach InfluxDB | Use `http://influxdb:8086` (Docker internal DNS), not localhost |
| Grafana shows no data | Verify data source token is correct and time range covers existing data |
| PostgreSQL connection refused | Confirm port 5432 is not in use; use `postgres` as hostname inside Docker |
| Panels show "No data" | Measurement name is `breaker_data`, not `device_metrics` |
| Data gaps after laptop sleep | Set Power Options → Never sleep; Lid close → Do nothing |
