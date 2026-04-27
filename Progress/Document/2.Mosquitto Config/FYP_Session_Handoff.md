# FYP Project — Session Handoff Document
**Project:** Multi-Panel Electronics Enclosure Monitoring System with Web-Based Dashboard
**Student:** Edwin (UP2517673)
**Supervisor:** Dr. Loo Poh Kok
**Submission Deadline:** June 23, 2026

---

## Project Overview

Monitors 10 electronics enclosure panels (1 physical ESP32 + 9 simulated) using:
- Temperature / humidity sensors
- RFID access control
- Automated cooling (fan)
- Web dashboard with real-time monitoring and predictive maintenance

---

## Locked-In Technology Stack

### Components (Main Structure)
These appear in architecture diagrams and FYP write-ups. When supervisor asks "what is your tech stack?" — answer with these 5 only.

| # | Component | Layer | Role | Status |
|---|-----------|-------|------|--------|
| 1 | Arduino + PubSubClient | ESP32 Firmware | Reads sensors, publishes MQTT, receives commands | ✅ Done |
| 2 | Mosquitto Broker (Level 2) | Communication | Routes all MQTT messages, password-authenticated | ✅ Done |
| 3 | FastAPI | Backend | Subscribes to MQTT, stores data, serves REST API | ✅ Env ready |
| 4 | PostgreSQL | Database | Stores all sensor readings and historical data | ⬜ Next |
| 5 | React + Chart.js | Frontend | Web dashboard, real-time and historical display | ⬜ Later |

### Connectors & Runners (Supporting Tools)
These are NOT drawn in architecture diagrams. They are the "cables and engines" between components. Only mention them when supervisor asks *how* components communicate.

| Package | Type | Sits Between | Role |
|---------|------|--------------|------|
| `uvicorn` | Runner | — runs FastAPI | ASGI server that starts the FastAPI application |
| `paho-mqtt` | Connector | FastAPI ↔ Mosquitto | Python MQTT client library |
| `psycopg2-binary` | Connector | FastAPI ↔ PostgreSQL | PostgreSQL database driver for Python |

### The Rule
> **Component** = a box in your architecture diagram (you build *with* it)
> **Connector/Runner** = an arrow between boxes (you never write code *in* it)

### When to Mention What
| Situation | What to say |
|-----------|-------------|
| "What is your tech stack?" | The 5 components only |
| "How does FastAPI talk to PostgreSQL?" | psycopg2 as the driver |
| "How do you run FastAPI?" | uvicorn as the ASGI server |
| "How does Python connect to MQTT?" | paho-mqtt as the client library |

**Optional extras (only if time permits):** Grafana, TimescaleDB, Prometheus, Docker

---

## Architecture Diagram

```
ESP32 (Arduino + PubSubClient)
        ↕ MQTT [paho-mqtt]
Mosquitto Broker (Level 2)
        ↕ MQTT [paho-mqtt]
FastAPI Backend
        ↕ SQL [psycopg2]
PostgreSQL
        ↕ HTTP / REST
React + Chart.js
```

---

## Completed Milestones

### 1. Mosquitto Broker ✅
- Installed on Windows 11
- Level 2 config: password auth, dual logging, persistence, 50-client limit
- Running manually via:
  ```
  mosquitto -c "C:\Program Files\mosquitto\mosquitto.conf" -v
  ```
- **Note:** `net start mosquitto` (Windows service) starts then terminates — always use the manual command above
- Two MQTT users created:
  - `esp32_panel` / password: `test`
  - `backend_server` / password: `backend123` (or whatever was reset)
- Firewall rule added:
  ```
  netsh advfirewall firewall add rule name="Mosquitto MQTT" dir=in action=allow protocol=TCP localport=1883
  ```

### 2. ESP32 Firmware ✅
- Library migrated from Adafruit_MQTT → PubSubClient v2.8.0
- Code file: `esp32_v4_0_MOSQUITTO.ino`
- Credentials file: `credentials.h`
- MQTT Server: `192.168.100.2`
- MQTT Port: `1883`
- MQTT User: `esp32_panel`
- MQTT Password: `test`
- MQTT Client ID: `ESP32_Panel_1`
- Topic structure: `panel/{panel_id}/{data_type}`
- Topics published:
  - `panel/1/temperature`
  - `panel/1/humidity`
  - `panel/1/door_status`
  - `panel/1/fan_status`
  - `panel/1/alarm_status`
- Topics subscribed (receives commands):
  - `panel/1/control/fan`
  - `panel/1/control/reset`
- Hardware confirmed working: sensors, LCD, keypad, door, buzzer, fan

### 3. FastAPI Dev Environment ✅
- Python 3.13.2
- pip 24.3.1
- Virtual environment location:
  ```
  C:\Users\Admin\Downloads\Project_MQTT\FYP\FYP_Backend\venv
  ```
- Activate venv:
  ```
  cd C:\Users\Admin\Downloads\Project_MQTT\FYP\FYP_Backend
  venv\Scripts\activate
  ```
- Installed packages:
  ```
  pip install fastapi uvicorn paho-mqtt psycopg2-binary
  ```

### 4. Basic MQTT Subscriber ✅
- File: `mqtt_subscriber.py` (in FYP_Backend folder)
- Connects as `backend_server` to Mosquitto
- Subscribes to `panel/#` (all panel topics)
- Prints all incoming messages to terminal
- Run with:
  ```
  python mqtt_subscriber.py
  ```
- Full data flow confirmed end to end:
  ```
  ESP32 → Mosquitto Broker → Python MQTT Subscriber
  ```
- Sample output:
  ```
  Connected to Mosquitto broker successfully!
  Subscribed to topic: panel/#
  [RECEIVED] Topic: panel/1/temperature | Value: 33.9
  [RECEIVED] Topic: panel/1/humidity | Value: 50.0
  ```

---

## Network Topology

### Current Development Setup
```
ESP32 (10.153.203.189)
    ↕ WiFi hotspot (subnet: 10.153.203.x)
Mobile Phone
    inner face: 10.153.203.1  ← faces ESP32
    outer face: 192.168.100.223 ← faces home router
    ↕ WiFi (subnet: 192.168.100.x)
Home Router (192.168.100.1)
    ↕ Ethernet
PC / Mosquitto (192.168.100.2)
```

### How the Phone Acts as a NAT Router
The phone has **two network interfaces simultaneously**:

| Interface | IP | Faces |
|-----------|-----|-------|
| Hotspot (inner) | `10.153.203.1` | ESP32 side |
| WiFi (outer) | `192.168.100.223` | Home router side |

The phone performs **NAT (Network Address Translation)** — it receives packets from the ESP32, rewrites the source address to its own `192.168.100.223`, and forwards them to the PC. Replies come back to the phone which delivers them to the ESP32. This is exactly what a home router does to connect your private network to the internet.

### Why Connection Fails with Mobile Data
```
ESP32 → Phone hotspot → Mobile data network → ??? (no route to 192.168.100.2)
```
When phone uses mobile data, the outer face points at the mobile carrier network — completely separate from the home router. There is no path to reach `192.168.100.2`.

### Why Connection Works with WiFi
```
ESP32 → Phone hotspot → Phone WiFi → Home Router → PC (192.168.100.2)
```
Phone WiFi points the outer face back at the home router, completing the bridge between both subnets.

### Critical Rule
> **Phone must be on WiFi (not mobile data)** for ESP32 to reach PC.
> After switching, always hard reboot the ESP32 to force a fresh connection.

### Future Production Topology (Planned)
```
ESP32 → Internet → Linode Firewall → Ubuntu OS → Docker → Mosquitto Broker
```
- Ports: 1883 (standard MQTT) and 8883 (MQTT over TLS)
- Hashed password authentication on both ports
- This removes the local network dependency entirely

---

## Key Configuration Reference

| Item | Value |
|------|-------|
| Broker IP | `192.168.100.2` |
| Broker Port | `1883` |
| ESP32 MQTT User | `esp32_panel` |
| Backend MQTT User | `backend_server` |
| MQTT Client ID | `ESP32_Panel_1` |
| Topic Structure | `panel/{panel_id}/{data_type}` |
| Backend wildcard subscription | `panel/#` |
| FYP_Backend path | `C:\Users\Admin\Downloads\Project_MQTT\FYP\FYP_Backend` |

---

## How to Start Everything (Checklist)

Every new session, open 3 Command Prompt windows:

**Window 1 — Start Mosquitto broker:**
```
mosquitto -c "C:\Program Files\mosquitto\mosquitto.conf" -v
```

**Window 2 — Activate venv and run subscriber:**
```
cd C:\Users\Admin\Downloads\Project_MQTT\FYP\FYP_Backend
venv\Scripts\activate
python mqtt_subscriber.py
```

**Window 3 — Free for other commands**

Then power on ESP32 and confirm `[RECEIVED]` messages appear in Window 2.

---

## Next Step — Step 3: PostgreSQL Setup

Tasks remaining:
1. Install PostgreSQL on Windows 11
2. Create database for sensor data
3. Design schema:
   - `panel_id`, `timestamp`, `temperature`, `humidity`, `door_status`, `fan_status`, `alarm_status`
4. Connect FastAPI + MQTT + PostgreSQL (Step 4)
5. REST API endpoints:
   - `GET /panels` — list all panels
   - `GET /panels/{id}/current` — current readings
   - `GET /panels/{id}/history` — historical data

---

## Career Context

Edwin's approach is **career-first** — choosing industry-relevant technologies over simpler alternatives.

FYP components as resume talking points:
- MQTT + Mosquitto → IoT/infrastructure experience
- FastAPI → Modern Python backend
- PostgreSQL → Industry standard database
- React + Chart.js → Frontend
- Docker (future) → DevOps/containerization

Career transition path: CS degree → Junior QA/SDET → Mid-Level SDET → DevOps/DevSecOps/SRE

---

## Learning Preferences

- **Concept before code** — always explain the why before the how
- **Analogy-driven** — responds well to concrete analogies
- **Systematic** — asks clarifying questions before proceeding
- **Documentation** — values markdown reference docs as checkpoints
