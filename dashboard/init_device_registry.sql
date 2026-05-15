CREATE TABLE IF NOT EXISTS device_registry (
  id SERIAL PRIMARY KEY,
  device_id TEXT UNIQUE NOT NULL,
  latitude DOUBLE PRECISION NOT NULL,
  longitude DOUBLE PRECISION NOT NULL,
  name TEXT,
  location TEXT,
  k_factor DOUBLE PRECISION DEFAULT 0.22,
  panel_power_wp DOUBLE PRECISION DEFAULT 300,
  device_type TEXT DEFAULT 'real',
  status TEXT DEFAULT 'active',
  created_at TIMESTAMPTZ DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_device_registry_status ON device_registry(status);
CREATE INDEX IF NOT EXISTS idx_device_registry_location ON device_registry(location);
CREATE INDEX IF NOT EXISTS idx_device_registry_coords ON device_registry(latitude, longitude);

INSERT INTO device_registry
  (device_id, latitude, longitude, name, location, k_factor, panel_power_wp, device_type, status)
VALUES
  ('device_001', 34.6780, 33.0380, 'Real Breaker Device', 'limassol', 0.22, 300, 'real', 'active')
ON CONFLICT (device_id) DO UPDATE SET
  latitude = EXCLUDED.latitude,
  longitude = EXCLUDED.longitude,
  name = EXCLUDED.name,
  location = EXCLUDED.location,
  k_factor = EXCLUDED.k_factor,
  panel_power_wp = EXCLUDED.panel_power_wp,
  device_type = EXCLUDED.device_type,
  status = EXCLUDED.status;

SELECT device_id, latitude, longitude, location, device_type, status
FROM device_registry
ORDER BY device_id;

