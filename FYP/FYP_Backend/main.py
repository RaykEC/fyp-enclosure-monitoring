# Region 1 — Imports
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import StreamingResponse
from contextlib import asynccontextmanager
import paho.mqtt.client as mqtt
import psycopg2
import psycopg2.extras
import json
from datetime import datetime, timedelta
import threading
import csv
import io

# Region 2 — Configuration constants
MQTT_BROKER = "192.168.100.2"
MQTT_PORT = 1883
MQTT_USER = "backend_server"
MQTT_PASS = "test"
MQTT_CLIENT_ID = "fastapi_backend"


DB_HOST = "localhost"
DB_PORT = 5432
DB_NAME = "fyp_monitoring"
DB_USER = "postgres"
DB_PASS = "test"

TEMP_THRESHOLD = 75.0

# Region 3 — Database helper
def get_db_connection():
    conn = psycopg2.connect(
        host=DB_HOST,
        port=DB_PORT,
        dbname=DB_NAME,
        user=DB_USER,
        password=DB_PASS
    )
    return conn

# Region 4 — MQTT callbacks
mqtt_client = None

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("[MQTT] Connected to Mosquitto broker")
        client.subscribe("panel/#")
        print("[MQTT] Subscribed to panel/#")
    else:
        print(f"[MQTT] Connection failed with code {rc}")


def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode()

    # Parse topic: panel/{panel_id}/{data_type}
    parts = topic.split("/")
    if len(parts) != 3:
        return
    
    panel_id = int(parts[1])
    data_type = parts[2]

    try:
        conn = get_db_connection()
        cur = conn.cursor()

        # 1. INSERT into sensor_readings (event log)
        if data_type == "temperature":
            cur.execute(
                "INSERT INTO sensor_readings (panel_id, temperature) VALUES (%s, %s)",
                (panel_id, float(payload))
            )
            cur.execute(
                "UPDATE panels SET current_temperature = %s, last_seen = NOW(), status = 'online' WHERE panel_id = %s",
                (float(payload), panel_id)
            )

            # Check threshold — INSERT alert if critical
            if float(payload) >= TEMP_THRESHOLD:
                cur.execute(
                    "INSERT INTO alerts (panel_id, alert_type, severity, message) VALUES (%s, %s, %s, %s)",
                    (panel_id, "HIGH_TEMP", "CRITICAL", f"Temperature {payload}°C exceeds {TEMP_THRESHOLD}°C")
                )
                print(f"[ALERT] Panel {panel_id}: Temperature {payload}°C — CRITICAL")

        elif data_type == "humidity":
            cur.execute(
                "INSERT INTO sensor_readings (panel_id, humidity) VALUES (%s, %s)",
                (panel_id, float(payload))
            )
            cur.execute(
                "UPDATE panels SET current_humidity = %s, last_seen = NOW(), status = 'online' WHERE panel_id = %s",
                (float(payload), panel_id)
            )

        elif data_type == "door_status":
            cur.execute(
                "INSERT INTO sensor_readings (panel_id, door_status) VALUES (%s, %s)",
                (panel_id, payload)
            )
            cur.execute(
                "UPDATE panels SET current_door_status = %s, last_seen = NOW(), status = 'online' WHERE panel_id = %s",
                (payload, panel_id)
            )

        elif data_type == "fan_status":
            cur.execute(
                "INSERT INTO sensor_readings (panel_id, fan_status) VALUES (%s, %s)",
                (panel_id, payload)
            )
            cur.execute(
                "UPDATE panels SET current_fan_status = %s, last_seen = NOW(), status = 'online' WHERE panel_id = %s",
                (payload, panel_id)
            )

        elif data_type == "alarm_status":
            cur.execute(
                "INSERT INTO sensor_readings (panel_id, alarm_status) VALUES (%s, %s)",
                (panel_id, payload)
            )
            cur.execute(
                "UPDATE panels SET current_alarm_status = %s, last_seen = NOW(), status = 'online' WHERE panel_id = %s",
                (payload, panel_id)
            )

            # Check for door alarm conditions
            if payload == "TIMEOUT":
                cur.execute(
                    "INSERT INTO alerts (panel_id, alert_type, severity, message) VALUES (%s, %s, %s, %s)",
                    (panel_id, "DOOR_TIMEOUT", "WARNING", "Door open for 60 seconds without password entry")
                )
                print(f"[ALERT] Panel {panel_id}: Door timeout alarm")

            elif payload == "WRONG_CODE":
                cur.execute(
                    "INSERT INTO alerts (panel_id, alert_type, severity, message) VALUES (%s, %s, %s, %s)",
                    (panel_id, "SECURITY_BREACH", "CRITICAL", "3 incorrect password attempts")
                )
                print(f"[ALERT] Panel {panel_id}: Security breach — wrong codes")

        conn.commit()
        cur.close()
        conn.close()

        print(f"[DB] Panel {panel_id} | {data_type} = {payload}")

    except Exception as e:
        print(f"[ERROR] Database write failed: {e}")


# Region 5 — FastAPI app + lifespan hook
@asynccontextmanager
async def lifespan(app):
    global mqtt_client

    # Create MQTT client
    mqtt_client = mqtt.Client(client_id=MQTT_CLIENT_ID)
    mqtt_client.username_pw_set(MQTT_USER, MQTT_PASS)

    # Wire up callbacks (hand over the cards)
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message

    # Connect to Mosquitto
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT)

    # Start background thread
    mqtt_client.loop_start()
    print("[SYSTEM] MQTT background thread started")

    yield  # FastAPI runs here — accepting HTTP requests

    # Shutdown cleanup
    mqtt_client.loop_stop()
    mqtt_client.disconnect()
    print("[SYSTEM] MQTT disconnected")


app = FastAPI(lifespan=lifespan)

# CORS — allow React dev server to call this API
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# Region 6 — REST endpoints

@app.get("/panels")
def get_all_panels():
    conn = get_db_connection()
    cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
    cur.execute("SELECT * FROM panels ORDER BY panel_id")
    panels = cur.fetchall()
    cur.close()
    conn.close()
    return panels


@app.get("/panels/{panel_id}")
def get_panel(panel_id: int):
    conn = get_db_connection()
    cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
    cur.execute("SELECT * FROM panels WHERE panel_id = %s", (panel_id,))
    panel = cur.fetchone()
    cur.close()
    conn.close()
    if panel is None:
        raise HTTPException(status_code=404, detail="Panel not found")
    return panel


@app.get("/panels/{panel_id}/history")
def get_panel_history(panel_id: int, hours: int = 24):
    conn = get_db_connection()
    cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
    cur.execute(
        "SELECT * FROM sensor_readings WHERE panel_id = %s AND recorded_at >= NOW() - INTERVAL '%s hours' ORDER BY recorded_at DESC",
        (panel_id, hours)
    )
    readings = cur.fetchall()
    cur.close()
    conn.close()
    return readings


@app.get("/alerts")
def get_alerts(unacknowledged: bool = False):
    conn = get_db_connection()
    cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
    if unacknowledged:
        cur.execute("SELECT * FROM alerts WHERE acknowledged = FALSE ORDER BY triggered_at DESC")
    else:
        cur.execute("SELECT * FROM alerts ORDER BY triggered_at DESC")
    alerts = cur.fetchall()
    cur.close()
    conn.close()
    return alerts


@app.post("/panels/{panel_id}/control/reset")
def reset_panel(panel_id: int):
    if mqtt_client is None or not mqtt_client.is_connected():
        raise HTTPException(status_code=503, detail="MQTT not connected")
    topic = f"panel/{panel_id}/control/reset"
    mqtt_client.publish(topic, "RESET")
    return {"message": f"Reset command sent to panel {panel_id}"}


@app.get("/panels/{panel_id}/export/readings")
def export_readings(panel_id: int, from_date: str = ""):
    # Validate date format
    if not from_date or len(from_date) != 8:
        raise HTTPException(status_code=400, detail="Please enter a valid value (YYYYMMDD)")
    try:
        start_date = datetime.strptime(from_date, "%Y%m%d")
    except ValueError:
        raise HTTPException(status_code=400, detail="Please enter a valid value (YYYYMMDD)")

    conn = get_db_connection()
    cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
    cur.execute(
        """SELECT recorded_at, temperature, humidity 
           FROM sensor_readings 
           WHERE panel_id = %s AND recorded_at >= %s 
             AND (temperature IS NOT NULL OR humidity IS NOT NULL)
           ORDER BY recorded_at ASC""",
        (panel_id, start_date)
    )
    rows = cur.fetchall()
    cur.close()
    conn.close()

    # Build CSV in memory
    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow(["timestamp", "temperature", "humidity"])
    for row in rows:
        writer.writerow([row["recorded_at"], row["temperature"], row["humidity"]])

    output.seek(0)
    filename = f"panel_{panel_id}_readings_{from_date}.csv"
    return StreamingResponse(
        output,
        media_type="text/csv",
        headers={"Content-Disposition": f"attachment; filename={filename}"}
    )


@app.get("/panels/{panel_id}/export/alerts")
def export_alerts(panel_id: int, from_date: str = ""):
    # Validate date format
    if not from_date or len(from_date) != 8:
        raise HTTPException(status_code=400, detail="Please enter a valid value (YYYYMMDD)")
    try:
        start_date = datetime.strptime(from_date, "%Y%m%d")
    except ValueError:
        raise HTTPException(status_code=400, detail="Please enter a valid value (YYYYMMDD)")

    conn = get_db_connection()
    cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
    cur.execute(
        """SELECT triggered_at, alert_type, severity, message, acknowledged 
           FROM alerts 
           WHERE panel_id = %s AND triggered_at >= %s 
           ORDER BY triggered_at ASC""",
        (panel_id, start_date)
    )
    rows = cur.fetchall()
    cur.close()
    conn.close()

    # Build CSV in memory
    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow(["timestamp", "alert_type", "severity", "message", "acknowledged"])
    for row in rows:
        writer.writerow([row["triggered_at"], row["alert_type"], row["severity"], row["message"], row["acknowledged"]])

    output.seek(0)
    filename = f"panel_{panel_id}_alerts_{from_date}.csv"
    return StreamingResponse(
        output,
        media_type="text/csv",
        headers={"Content-Disposition": f"attachment; filename={filename}"}
    )

# Region 7 — Direct-run block
if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)