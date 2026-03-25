# Dashboard Installation

## Quick Start (Docker)

```bash
docker-compose up -d
```

Wait ~30 seconds, then access:

| Service | URL | Login |
|---------|-----|-------|
| Node-RED | http://localhost:1880 | — |
| Grafana | http://localhost:3000 | admin / admin123 |
| InfluxDB | http://localhost:8086 | admin / admin123456 |

## Setup Steps

### 1. Import Node-RED Flows
1. Open http://localhost:1880
2. Menu (☰) → Import → Upload `flows.json`
3. Click **Deploy**

### 2. Import Grafana Dashboard
1. Open http://localhost:3000 → Login
2. Add Data Source → InfluxDB → Flux
   - URL: `http://influxdb:8086`
   - Token: `my-super-secret-token-12345`
   - Org: `pv-monitoring`
3. Dashboards → Import → Upload `grafana-dashboard.json`

### 3. Initialize Device Registry
```bash
docker exec -i postgres psql -U admin -d pv_system < insert_devices.sql
```

## Files

| File | Description |
|------|-------------|
| docker-compose.yml | Starts InfluxDB, Node-RED, Grafana, PostgreSQL |
| flows.json | Node-RED flows (MQTT pipeline, PR detection, simulator, remote control) |
| grafana-dashboard.json | Grafana dashboard (7 panels: power, GHI, voltage/current, scatter, map, alerts, status) |
| insert_devices.sql | 50 simulated devices across 5 cities in Cyprus |

## Stop / Restart

```bash
docker-compose stop      # Stop (data preserved)
docker-compose start     # Restart
docker-compose down      # Remove containers (data volumes preserved)
docker-compose down -v   # Delete everything (caution!)
```
