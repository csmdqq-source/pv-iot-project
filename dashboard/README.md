# Dashboard Stack

This folder contains the cloud-side monitoring stack:

- Node-RED for MQTT ingestion, data validation, Open-Meteo GHI retrieval, PR calculation, and alarm logic
- InfluxDB 2.7 for time-series storage
- Grafana for visualization
- PostgreSQL for the device registry

## Files

| File | Purpose |
|------|---------|
| `docker-compose.yml` | Starts InfluxDB, Node-RED, Grafana, and PostgreSQL |
| `flows.json` | Exported Node-RED flows |
| `grafana-dashboard.json` | Grafana dashboard export |
| `init_device_registry.sql` | Creates the PostgreSQL device registry and inserts `device_001` |
| `insert_devices.sql` | Optional simulated Cyprus device set |

## Start

```bash
cp .env.example .env
# Edit .env before public deployment.
docker-compose up -d
docker ps
```

Expected containers:

```text
influxdb
nodered
grafana
postgres
```

## Default Service Configuration

| Service | URL in Browser | Internal Docker URL | Credentials |
|---------|----------------|---------------------|-------------|
| Node-RED | `http://<server-ip>:1880` | `http://nodered:1880` | none by default |
| Grafana | `http://<server-ip>:3000` | `http://grafana:3000` | values from `.env` |
| InfluxDB | optional `http://<server-ip>:8086` | `http://influxdb:8086` | values from `.env` |
| PostgreSQL | do not expose publicly | `postgres:5432` | values from `.env` |

InfluxDB settings used by both Node-RED and Grafana:

```text
Organization: pv-monitoring
Bucket: pv_data
Token: value of `INFLUXDB_ADMIN_TOKEN` in `.env`
URL inside Docker: http://influxdb:8086
```

## Initialize Device Registry

Run this once after the containers start:

```bash
sudo docker exec -i postgres psql -U "$POSTGRES_USER" -d "$POSTGRES_DB" < init_device_registry.sql
```

Optional simulated devices:

```bash
sudo docker exec -i postgres psql -U "$POSTGRES_USER" -d "$POSTGRES_DB" < insert_devices.sql
```

After importing the registry, open Node-RED and manually trigger the registry synchronization flow once. Then trigger the Open-Meteo live flow to populate GHI.

## Grafana Import

1. Open Grafana at `http://<server-ip>:3000`.
2. Log in with the Grafana username and password configured in `.env`.
3. Add an InfluxDB data source:

   ```text
   Query language: Flux
   URL: http://influxdb:8086
   Organization: pv-monitoring
   Token: value of `INFLUXDB_ADMIN_TOKEN` in `.env`
   Default bucket: pv_data
   ```

4. Import `grafana-dashboard.json`.

## Node-RED Import

1. Open Node-RED at `http://<server-ip>:1880`.
2. Import `flows.json`.
3. Install missing palette nodes if prompted:

   ```text
   node-red-contrib-influxdb
   node-red-contrib-postgresql
   node-red-dashboard
   ```

4. Configure the MQTT input node to subscribe to the EMQX Cloud broker.
5. Deploy.

## Production Safety

Close port `1880` in the cloud firewall after debugging. Node-RED continues to run in the background even when the editor is not publicly accessible.

