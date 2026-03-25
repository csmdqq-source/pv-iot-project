import json
import time

import paho.mqtt.client as mqtt
import serial


# ====== Config ======
SERIAL_PORT = "COM3"
SERIAL_BAUD = 115200
SERIAL_TIMEOUT_SEC = 1

MQTT_HOST = "127.0.0.1"
MQTT_PORT = 1883
MQTT_TOPIC = "pv/device_001/data"
MQTT_USERNAME = "pv_device_001"
MQTT_PASSWORD = "device_secure_pwd"  # set if needed

PUBLISH_QOS = 0
KEEPALIVE_SEC = 60


def connect_mqtt() -> mqtt.Client:
    client = mqtt.Client(client_id="pv_device_001_gw")
    if MQTT_USERNAME:
        client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    client.connect(MQTT_HOST, MQTT_PORT, KEEPALIVE_SEC)
    client.loop_start()
    return client


def main() -> None:
    ser = serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=SERIAL_TIMEOUT_SEC)
    client = connect_mqtt()

    print(f"[GW] Serial: {SERIAL_PORT} @ {SERIAL_BAUD}")
    print(f"[GW] MQTT: {MQTT_HOST}:{MQTT_PORT} topic={MQTT_TOPIC}")

    while True:
        line = ser.readline()
        if not line:
            continue

        try:
            text = line.decode(errors="ignore").strip()
        except Exception:
            continue

        if not text:
            continue

        # try JSON parse; if prefixed, extract JSON fragment
        try:
            payload = json.loads(text)
        except Exception:
            start = text.find("{")
            end = text.rfind("}")
            if start >= 0 and end > start:
                try:
                    payload = json.loads(text[start:end + 1])
                except Exception:
                    continue
            else:
                continue

        # ensure required fields for Node-RED flow
        if "active_power" not in payload and "power" in payload:
            payload["active_power"] = payload["power"]
        # keep only active_power (drop power)
        if "active_power" in payload and "power" in payload:
            payload.pop("power", None)
        if "efficiency" not in payload:
            payload["efficiency"] = 0.5
        if "temperature" not in payload:
            payload["temperature"] = 35.3

        payload_str = json.dumps(payload, separators=(",", ":"))
        try:
            client.publish(MQTT_TOPIC, payload_str, qos=PUBLISH_QOS)
            print(f"[GW] Pub -> {MQTT_TOPIC}: {payload_str}")
        except Exception as exc:
            print(f"[GW] Publish failed: {exc}")
            time.sleep(1)


if __name__ == "__main__":
    main()
