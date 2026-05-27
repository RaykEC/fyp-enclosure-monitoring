# Multi-Panel Electronics Enclosure Monitoring System

A real-time IoT monitoring system for electronics enclosure panels with automated cooling, alert management, and a web-based dashboard.

**Final Year Project (PJE40)** | University of Portsmouth | BSc Computer Science

---

## Overview

This system monitors up to 10 electronics enclosure panels (1 physical ESP32, 9 simulated) tracking temperature, humidity, door status, fan status, and alarm status. Data flows from the ESP32 sensor node through MQTT to a FastAPI backend, is stored in PostgreSQL, and displayed on a React dashboard.

---

## System Architecture

```
ESP32 (Sensors + PubSubClient)
        ↕ MQTT (Publish/Subscribe)
Mosquitto Broker (Port 1883)
        ↕ MQTT (paho-mqtt)
FastAPI Backend (Port 8000)
        ↕ SQL (psycopg2)
PostgreSQL Database (fyp_monitoring)
        ↕ REST API (JSON)
React + Chart.js Dashboard (Port 5173)
```

---

## Features

- Real-time temperature and humidity monitoring (10-second intervals)
- Automated fan control with hysteresis (ON at 45°C, OFF at 38°C)
- Three alert conditions: critical temperature (≥75°C), door timeout (60s), security breach (3 wrong passwords)
- Web dashboard with live status, historical charts (30 days), and colour-coded indicators
- Remote system reset via dashboard
- CSV export of sensor readings and alerts
- Online/offline panel detection (15-second threshold)
- Error banner when backend is unreachable (self-healing on reconnection)

---

## Technology Stack

| Component | Technology | Purpose |
|-----------|-----------|---------|
| Firmware | Arduino C++ + PubSubClient | Sensor reading, fan control, MQTT publishing |
| Broker | Eclipse Mosquitto | MQTT message routing with password authentication |
| Backend | FastAPI + uvicorn | REST API + MQTT subscriber |
| Database | PostgreSQL 18 | Current state and historical data storage |
| Frontend | React + Vite + Chart.js | Web dashboard with data visualisation |

---

## Prerequisites

- Windows 11 (development environment)
- Arduino IDE (for ESP32 firmware)
- Python 3.14+ with pip
- Node.js v24+ with npm
- PostgreSQL 18+
- Mosquitto MQTT Broker

### Hardware

- ESP32-WROOM-32E on expansion board
- DHT11 temperature/humidity sensor
- Reed switch (door detection)
- 4x4 keypad
- LCD 1602 I2C display
- DC motor (fan) with PN2222 transistor
- Buzzer with PN2222 transistor
- Push button (system reset)

---

## Setup Instructions

### 1. Mosquitto Broker

Install Mosquitto and create MQTT users:

```bash
mosquitto_passwd -c "C:\Program Files\mosquitto\passwd" esp32_panel
mosquitto_passwd "C:\Program Files\mosquitto\passwd" backend_server
```

Configure `mosquitto.conf` with:
- Listener on port 1883 (all interfaces)
- Password file path
- Persistence enabled
- Logging enabled

Add a Windows Firewall rule to allow inbound traffic on port 1883.

### 2. PostgreSQL Database

```sql
CREATE DATABASE fyp_monitoring;
```

Connect to the database and create the three tables (panels, sensor_readings, alerts) as defined in the project schema. Seed the panels table with 10 rows.

### 3. FastAPI Backend

```bash
cd FYP_Backend
python -m venv venv
venv\Scripts\activate
pip install fastapi uvicorn paho-mqtt psycopg2-binary
```

### 4. React Frontend

```bash
cd fyp-frontend
npm install
```

### 5. ESP32 Firmware

1. Open `esp32_v4_0_MOSQUITTO.ino` in Arduino IDE
2. Create a `credentials.h` file with your Wi-Fi and MQTT credentials
3. Upload to ESP32

---

## Running the System

**Pre-requisite:** Phone must be on Wi-Fi (not mobile data) for ESP32 to reach the PC.

**Terminal 1 - Mosquitto Broker:**
```bash
mosquitto -c "C:\Program Files\mosquitto\mosquitto.conf" -v
```

**Terminal 2 - FastAPI Backend:**
```bash
cd FYP_Backend
venv\Scripts\activate
python main.py
```

**Terminal 3 - React Frontend:**
```bash
cd fyp-frontend
npm run dev
```

**Power on the ESP32.**

### Access Points

| Service | URL |
|---------|-----|
| Dashboard | http://localhost:5173 |
| Swagger UI | http://192.168.100.2:8000/docs |

---

## REST API Endpoints

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/panels` | GET | List all panels with current state |
| `/panels/{panel_id}` | GET | One panel's current state |
| `/panels/{panel_id}/history?hours=24` | GET | Historical sensor readings |
| `/alerts` | GET | Alert log |
| `/panels/{panel_id}/control/reset` | POST | Send reset command to ESP32 |
| `/panels/{panel_id}/export/readings?from_date=YYYYMMDD` | GET | Download sensor readings CSV |
| `/panels/{panel_id}/export/alerts?from_date=YYYYMMDD` | GET | Download alerts CSV |

---

## Network Topology

```
#can bypass the phone and directly connect to wifi router.
ESP32 → Wi-Fi → Phone (hotspot, mobile data OFF)
    ↕ Wi-Fi (subnet: 192.168.100.x)
Home Router (192.168.100.1)
    ↕ Ethernet
PC / Mosquitto / PostgreSQL / FastAPI (192.168.100.2)
```

---

## Project Structure

```
FYP/
├── FYP_Backend/
│   ├── main.py              # FastAPI backend (MQTT + REST API + DB)
│   ├── mqtt_subscriber.py   # Retired proof-of-concept
│   └── venv/                # Python virtual environment
├── fyp-frontend/
│   ├── src/
│   │   ├── components/
│   │   │   ├── Sidebar.jsx
│   │   │   ├── PanelDetail.jsx
│   │   │   ├── StatusInfo.jsx
│   │   │   ├── SensorChart.jsx
│   │   │   └── LogsModal.jsx
│   │   ├── services/
│   │   │   └── api.js
│   │   ├── App.jsx
│   │   ├── App.css
│   │   └── main.jsx
│   └── package.json
├── esp32_v4_0_MOSQUITTO.ino  # ESP32 firmware
└── credentials.h             # MQTT/Wi-Fi credentials (not committed)
```

---

## Author

Edwin Chan Zi Min (UP2517673)
BSc Computer Science, University of Portsmouth

---

## License

This project was developed as a Final Year Project (PJE40) for academic purposes.
