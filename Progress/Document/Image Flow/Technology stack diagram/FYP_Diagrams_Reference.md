# FYP Diagrams Reference
**Project:** Multi-Panel Electronics Enclosure Monitoring System
**Student:** Edwin (UP2517673)

---

## 1. System Architecture Diagram
**Purpose:** Shows all components, connections, protocols, and bidirectional data flow.
**Use this when:** FYP report, supervisor presentation, "how does everything connect?"

```
React + Chart.js (browser)
    ↕ HTTP (port 8000)
┌─── venv (FYP_Backend\venv) ───────────────────┐
│  uvicorn (ASGI runner, port 8000)             │
│      ↕ passes request/response                │
│  FastAPI (REST endpoints + MQTT handler)      │
│      ↕                       ↕                │
│  paho-mqtt (connector)   psycopg2 (connector) │
└───────────────────────────────────────────────┘
       ↕ MQTT                    ↕ SQL (INSERT/SELECT)
  Mosquitto (port 1883)       PostgreSQL
       ↕ MQTT
  ESP32 device 1 ─┐
  ESP32 device 2 ─┤  Each publishes independently
  ESP32 device 3 ─┤  1 physical + 9 simulated
  ...            ─┤
  ESP32 device 10 ┘
```

### Key details:
- **All arrows are bidirectional**
- React ↔ uvicorn: HTTP requests (GET/POST) and JSON responses
- uvicorn ↔ FastAPI: uvicorn is the runner, passes requests through
- FastAPI ↔ paho-mqtt: receives sensor data IN, sends control commands OUT (fan on/off, reboot)
- FastAPI ↔ psycopg2: INSERT sensor data, SELECT for dashboard queries
- Mosquitto ↔ paho-mqtt: bidirectional MQTT
- ESP32 → Mosquitto: publishes sensor data (temperature, humidity, door, fan, alarm)
- Mosquitto → ESP32: delivers commands (panel/{id}/control/fan, panel/{id}/control/reset)
- Communication mode: **Bidirectional MQTT pub/sub on separate topic channels**

### What lives WHERE:
- **Inside venv:** uvicorn, FastAPI, paho-mqtt, psycopg2 (Python packages)
- **Outside venv:** Mosquitto (standalone broker), PostgreSQL (standalone database), React (browser app), ESP32 (firmware)

### Component vs Connector vs Runner:
- **Component** = a box in the architecture diagram (ESP32, Mosquitto, FastAPI, PostgreSQL, React)
- **Connector** = translator between two systems (paho-mqtt translates Python↔MQTT, psycopg2 translates Python↔SQL)
- **Runner** = starts your app (uvicorn runs FastAPI on port 8000)

### Two doors into the venv:
- **Port 8000 (HTTP)** — uvicorn, where React knocks
- **Port 1883 (MQTT)** — paho-mqtt, where Mosquitto pushes sensor data

---

## 2. Data Flow Diagram
**Purpose:** Follows the journey of one piece of data through the system.
**Use this when:** Explaining "what happens to a temperature reading from sensor to screen?"

### Path A — Sensor data IN:
```
DHT sensor reads 33.9°C
  → ESP32 publishes to panel/1/temperature (MQTT)
    → Mosquitto routes the message
      → paho-mqtt delivers to FastAPI
        → FastAPI INSERTs row into PostgreSQL
          (panel_id=1, timestamp=now, temperature=33.9, ...)
```

### Path B — Dashboard data OUT:
```
User opens React dashboard
  → React sends GET /panels/1/history (HTTP)
    → uvicorn passes to FastAPI
      → FastAPI SELECTs from PostgreSQL (by datetime range)
        → PostgreSQL returns rows
          → FastAPI returns JSON
            → React receives JSON
              → Chart.js renders temperature line graph
```

### Path C — Control command (fan on/off):
```
User clicks "Turn fan ON" in React
  → React sends POST /panels/1/control/fan (HTTP)
    → uvicorn passes to FastAPI
      → FastAPI publishes "ON" to panel/1/control/fan (via paho-mqtt)
        → Mosquitto routes to ESP32 (which subscribes to panel/1/control/fan)
          → ESP32 callback fires → toggles motor pin HIGH
```

### Path D — Control command (reboot):
```
User clicks "Reboot panel 1" in React
  → React sends POST /panels/1/control/reset (HTTP)
    → uvicorn passes to FastAPI
      → FastAPI publishes "RESET" to panel/1/control/reset (via paho-mqtt)
        → Mosquitto routes to ESP32 (which subscribes to panel/1/control/reset)
          → ESP32 callback fires → calls performFullSystemReset()
```

---

## 3. Technology Stack Diagram
**Purpose:** Lists what technologies sit at each layer — no arrows, no flow.
**Use this when:** Answering "what is your tech stack?" or for resume/CV.

| Layer | Technologies |
|-------|-------------|
| Frontend | React, Chart.js |
| Backend | FastAPI, Python 3.13, uvicorn |
| Database | PostgreSQL |
| Messaging | Mosquitto broker, MQTT protocol |
| Hardware | ESP32, Arduino framework, PubSubClient |

### The 5 main components (for supervisor):
1. Arduino + PubSubClient (ESP32 firmware)
2. Mosquitto Broker (communication)
3. FastAPI (backend)
4. PostgreSQL (database)
5. React + Chart.js (frontend)

### Connectors & runners (mention only when asked HOW):
- uvicorn → runs FastAPI
- paho-mqtt → connects FastAPI to Mosquitto
- psycopg2 → connects FastAPI to PostgreSQL

---

## Diagram Type Summary

| Diagram | Answers | Shows |
|---------|---------|-------|
| System architecture | "How does everything connect?" | Components, protocols, ports, bidirectional arrows |
| Data flow | "What happens to my data?" | Journey of one reading from sensor to screen |
| Technology stack | "What did you build with?" | Technologies grouped by layer, no arrows |
