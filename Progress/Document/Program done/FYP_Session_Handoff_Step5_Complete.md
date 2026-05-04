# FYP Project — Session Handoff Document
**Project:** Multi-Panel Electronics Enclosure Monitoring System with Web-Based Dashboard
**Student:** Edwin (UP2517673)
**Supervisor:** Dr. Loo Poh Kok
**Submission Deadline:** June 23, 2026
**Last Updated:** Step 5 (React Frontend) — **COMPLETE** (all sub-steps 5.1–5.9 done)

---

## Progression Tracker

```
Step 1 — venv + pip install                ✅ Done
Step 2 — mqtt_subscriber.py (paho only)    ✅ Done  ← retired, replaced by main.py
Step 3 — PostgreSQL setup                  ✅ Done  ← schema ready, data accumulating
Step 4 — FastAPI app (main.py)             ✅ Done  ← REST + MQTT + DB, all tested
Step 5 — React frontend                    ✅ COMPLETE
  5.1 — Vite project created               ✅ Done
  5.2 — api.js + CORS + export endpoints   ✅ Done
  5.3 — Sidebar + App layout               ✅ Done
  5.4 — PanelDetail + StatusInfo           ✅ Done
  5.5 — SensorChart (Chart.js)             ✅ Done
  5.6 — Export endpoints in main.py        ✅ Done (with 5.2)
  5.7 — LogsModal (CSV downloads)          ✅ Done
  5.8 — Reboot button                      ✅ Done (with 5.4)
  5.9 — Polish & title bar                 ✅ Done
```

**All 5 implementation steps are complete.** The full pipeline is live end-to-end:
ESP32 → Mosquitto → FastAPI → PostgreSQL → React Dashboard

---

## What Was Built in Sub-step 5.9 (This Session)

### 1. Title Bar (App.jsx + App.css)

A full-width header bar above the sidebar and detail area displaying three pieces of information:

- **Project name** — static text: "Enclosure Monitoring System"
- **Online panel count** — derived state (computed from `panels` array using `isOnline()` filter, no extra `useState` needed). Shows e.g. "1/10 Online" with a green dot
- **Live clock** — separate `useState` + `setInterval` ticking every second. Displays date (en-GB: `Wed, 3 May 2026`) and time (`16:32:48`)

Structural change: added an outer `app-container` (vertical flex column) wrapping the title bar and the existing `app-layout` (horizontal flex row). The title bar takes its natural height; `app-layout` gets `flex: 1` to fill the rest.

### 2. Colour Coding (StatusInfo.jsx)

Each status field now has a dedicated helper function that returns a CSS class based on the value:

| Field | Function | Normal | Warning (amber) | Critical (red) |
|-------|----------|--------|-----------------|----------------|
| Temperature | `tempClass()` | Below 45°C | 45–74°C (cooling) | ≥75°C (critical) |
| Door | `doorClass()` | `SECURE`, `GRANTED` | `OPEN` | `TIMEOUT`, `WRONG_CODE` |
| Fan | `fanClass()` | `OFF` | `ON` (actively cooling) | — |
| Alarm | `alarmClass()` | `NORMAL` | `TIMEOUT`, `TEMP_ACKED` | `CRITICAL_TEMP`, `WRONG_CODE` |

Thresholds match the ESP32 firmware's temperature state machine (45°C fan ON, 75°C critical). CSS classes `.warning` (amber `#ff9800`) and `.critical` (red `#f44336`) were already defined — this session wired them to the data.

### 3. Error Handling — Backend Unreachable Banner (App.jsx + App.css)

- Added `connected` state (`useState(true)`)
- `fetchPanels()` polling wrapped in `try/catch` inside an `async function loadPanels()`
- On successful fetch: `setConnected(true)` — banner disappears
- On fetch error: `setConnected(false)` — red banner appears below title bar
- Banner text: "⚠ Backend unreachable — displaying last known data"
- Banner appears/disappears automatically — no manual dismissal needed
- Self-healing: if backend comes back, next successful poll clears the banner

Architectural note: `api.js` was NOT changed. The service layer throws naturally on network failure (`fetch()` throws `TypeError`). Error handling belongs in the caller (`App.jsx`), not the service — same principle as `get_db_connection()` in `main.py`.

### 4. Loading States (PanelDetail.jsx)

- Added `loading` state (`useState(true)`)
- On panel switch: `setLoading(true)`, then `Promise.all([fetchPanel(), fetchHistory()])` waits for both to complete before `setLoading(false)`
- While loading: shows "Loading Panel X..." text instead of stale data from previously selected panel
- Loading duration is **data-driven** (not a hardcoded timer) — disappears the instant both API responses arrive
- The 5-second polling interval remains separate and only refreshes `fetchPanel()` (live status), not `fetchHistory()` (heavy 30-day dataset loaded once per panel switch)

### 5. Responsive Safety Net (App.css)

A single `@media (max-width: 768px)` query that:
- Stacks sidebar above content (column layout instead of row)
- Caps sidebar height at 120px with scroll
- Stacks chart above status info in panel detail
- Centres title bar content

Not a full mobile-first redesign — a graceful fallback preventing layout breakage on smaller screens (e.g. supervisor opens on tablet during demo). Explicitly a desktop-first monitoring dashboard.

### 6. Horizontal Scrollbar Fix (App.css)

Added `min-width: 0` to `.main-content` to prevent Chart.js canvas from pushing the flex child wider than available space. Common flexbox issue — flex children default to `min-width: auto` (refuse to shrink below content width).

---

## Component Architecture (Final)

```
App.jsx (owns: panels[], selectedPanel, clock, connected)
├── Title Bar (inline JSX — project name, online count, live clock)
├── Error Banner (conditional — only when !connected)
├── Sidebar.jsx (receives: panels, selectedPanel, onSelectPanel)
│   └── Renders panel list, click → updates selectedPanel
└── PanelDetail.jsx (receives: selectedPanel; owns: panel, history, showLogs, loading)
    ├── Loading state (shown while fetching)
    ├── SensorChart.jsx (receives: history)
    │   └── Chart.js Line chart: temp (red) + humidity (blue), 30 days
    ├── StatusInfo.jsx (receives: panel)
    │   └── Colour-coded: temp, humidity, door, fan, alarm with helper functions
    └── LogsModal.jsx (receives: panelId, onClose)
        └── Dropdown (readings/alerts) + date input → CSV download
```

### Data Flow (Final)

```
App.jsx
├── fetchPanels() every 5s (try/catch → connected state → error banner)
│   └── Derived: onlineCount = panels.filter(isOnline).length → title bar
├── clock: ticks every 1s → title bar
└── PanelDetail
    ├── Promise.all([fetchPanel, fetchHistory]) on panel switch → loading state
    ├── fetchPanel(id) every 5s → StatusInfo gets live values (colour-coded)
    ├── fetchHistory(id, 720) once per panel selection → SensorChart renders
    └── LogsModal
        ├── exportReadings(id, date) → browser downloads CSV
        └── exportAlerts(id, date) → browser downloads CSV
    └── Reboot button → sendReset(id) → POST to FastAPI → MQTT → ESP32
```

---

## REST Endpoints — Complete List (7 total)

| Endpoint | Method | Purpose | Added |
|----------|--------|---------|-------|
| `/panels` | GET | List all panels with current state | Step 4 |
| `/panels/{panel_id}` | GET | One panel's current state | Step 4 |
| `/panels/{panel_id}/history?hours=24` | GET | Historical readings for a panel | Step 4 |
| `/alerts` | GET | Alert log (optionally `?unacknowledged=true`) | Step 4 |
| `/panels/{panel_id}/control/reset` | POST | Send reset command via MQTT | Step 4 |
| `/panels/{panel_id}/export/readings?from_date=YYYYMMDD` | GET | Download sensor CSV | Step 5 |
| `/panels/{panel_id}/export/alerts?from_date=YYYYMMDD` | GET | Download alerts CSV | Step 5 |

---

## Frontend — React + Vite Project

**Project location:** `C:\Users\Admin\Downloads\Project_MQTT\FYP\fyp-frontend`

**Technology:** Vite (v8.0.10) + React, Chart.js + react-chartjs-2, chartjs-adapter-date-fns

**Node.js:** v24.15.0, npm 11.13.0

**Dev server:** `npm run dev` → `http://localhost:5173`

### Folder Structure

```
fyp-frontend/
├── node_modules/
├── public/
├── src/
│   ├── assets/              ← Vite default (unused)
│   ├── components/
│   │   ├── Sidebar.jsx       ← Panel list (1–N) with green/red status dots
│   │   ├── PanelDetail.jsx   ← Main content: header, chart, status, buttons, loading state
│   │   ├── StatusInfo.jsx    ← Right column: colour-coded temp, humidity, door, fan, alarm
│   │   ├── SensorChart.jsx   ← Combined temp + humidity line chart (30 days)
│   │   └── LogsModal.jsx     ← Export popup: dropdown + date input → CSV download
│   ├── services/
│   │   └── api.js            ← All fetch calls to FastAPI (7 functions)
│   ├── App.jsx               ← Root: title bar, error banner, sidebar + panel detail
│   ├── App.css               ← All component styling + responsive media query
│   ├── main.jsx              ← Entry point (Vite default)
│   └── index.css             ← Global reset styles
├── index.html
├── package.json
├── vite.config.js
└── eslint.config.js
```

### Installed npm Packages

| Package | Purpose |
|---------|---------|
| `react` | Core UI library (Vite default) |
| `react-dom` | React DOM renderer (Vite default) |
| `chart.js` | Charting library for line charts |
| `react-chartjs-2` | React wrapper for Chart.js |
| `chartjs-adapter-date-fns` | Date formatting adapter for Chart.js time axis |
| `date-fns` | Date utility library (required by adapter) |

### Online/Offline Detection

Sidebar and PanelDetail both use a 15-second threshold on `last_seen` to determine online/offline status — NOT the `status` column in PostgreSQL. If `last_seen` is more than 15 seconds ago, the panel shows as offline (red dot). The `isOnline()` function in `App.jsx` is also used to derive the online count for the title bar.

```javascript
function isOnline(lastSeen) {
  if (!lastSeen) return false;
  return (new Date() - new Date(lastSeen)) < 15000;
}
```

---

## How to Start Everything

**Pre-flight:** Phone on WiFi (not mobile data). Hard reboot ESP32 after switching.

**Window 1 — Mosquitto:**
```
mosquitto -c "C:\Program Files\mosquitto\mosquitto.conf" -v
```

**Window 2 — FastAPI:**
```
cd C:\Users\Admin\Downloads\Project_MQTT\FYP\FYP_Backend
venv\Scripts\activate
python main.py
```

**Window 3 — React dev server:**
```
cd C:\Users\Admin\Downloads\Project_MQTT\FYP\fyp-frontend
npm run dev
```

**Browser:**
- Dashboard: `http://localhost:5173`
- Swagger UI: `http://192.168.100.2:8000/docs`

---

## Wireframe Reference (Edwin's Sketches) — All Items Built

### Image 2 — Overview Layout
- Title bar across the top ✅ (project name, online count, live clock)
- Left sidebar: panel list (1–N) with status dots ✅
- Main area: detail view for selected panel ✅

### Image 1 — Panel Detail View
- Top bar: status dot, panel name, location ✅
- Left: combined temp + humidity chart (30 days) ✅
- Right: status info (last updated, temp, humidity, door, fan, alarm) ✅ (now colour-coded)
- Logs button → modal with dropdown + date input → CSV download ✅
- Reboot button → confirmation → sends reset via MQTT ✅

---

## Alert System — Three Triggers

| Condition | `alert_type` | `severity` | Source |
|-----------|-------------|-----------|--------|
| Temperature ≥ 75°C | `HIGH_TEMP` | `CRITICAL` | ESP32 publishes temperature, `on_message` checks threshold |
| Door open 60s, no password | `DOOR_TIMEOUT` | `WARNING` | ESP32 publishes `alarm_status = TIMEOUT` |
| 3 wrong password attempts | `SECURITY_BREACH` | `CRITICAL` | ESP32 publishes `alarm_status = WRONG_CODE` |

---

## Database Schema (Unchanged from Step 3)

### Tables

| Table | Row purpose | Write pattern |
|-------|------------|---------------|
| `panels` | Current state of each panel | UPDATE in place on every MQTT message |
| `sensor_readings` | Timestamped snapshot per message | INSERT only (append-only event log) |
| `alerts` | An event that happened | INSERT only (+ UPDATE on `acknowledged`) |

### Sensor Readings — One Column Per Row

Each MQTT message creates a separate row with only one data column filled. The ESP32 publishes each data type as a separate MQTT message. The React dashboard combines nearby timestamps for display.

### Panels Table — NULL Values

`current_door_status`, `current_fan_status`, `current_alarm_status` start as `NULL` and only fill in when the ESP32 publishes a state change. Temperature and humidity fill in quickly (published every 10 seconds). Status fields fill in on first state change event or on ESP32 reboot.

---

## ESP32 Control Logic Summary

### Physical Controls (on-site only)
- **Keypad password entry** — correct = access, 3 wrong = `SECURITY_ALARM`
- **Double-press reset button** — acknowledges critical temp alarm (silences buzzer). Publishes `alarm_status = TEMP_ACKED`
- **3-second hold reset button** — full system reset (`performFullSystemReset()`)

### Remote Control (via MQTT from dashboard)
- `panel/{id}/control/reset` with payload `"RESET"` — triggers `performFullSystemReset()`
- `panel/{id}/control/fan` exists in firmware but is **not exposed** as a REST endpoint — fan is automated by temperature state machine

### Temperature State Machine
```
NORMAL ──(≥45°C)──► COOLING ──(≥75°C)──► CRITICAL ──(double-press)──► ACKNOWLEDGED
  ▲                   ▲  ▲                   │                            │
  │                   │  │                   │                            │
  └───(<38°C)─────────┘  └───(<75°C)─────────┘                           │
                          └───(<75°C)─────────────────────────────────────┘
```

---

## Networking Topology (Unchanged)

```
ESP32 → WiFi → Phone (hotspot, mobile data OFF)
    ↕ WiFi (subnet: 192.168.100.x)
Home Router (192.168.100.1)
    ↕ Ethernet
PC / Mosquitto / PostgreSQL / FastAPI (192.168.100.2)
```

**Phone access to dashboard:** Tested — phone can reach React dev server at `http://192.168.100.2:5173` (requires `npm run dev -- --host` and firewall rule for port 5173). CORS blocks FastAPI calls from phone origin (would need `192.168.100.2:5173` added to allowed origins in `main.py`). Not fixed — desktop is the primary use case.

---

## Key Configuration Reference

| Item | Value |
|------|-------|
| Mosquitto Broker IP | `192.168.100.2` |
| Mosquitto Port | `1883` |
| ESP32 MQTT User | `esp32_panel` |
| ESP32 MQTT Client ID | `ESP32_Panel_1` |
| Backend MQTT User | `backend_server` |
| Backend MQTT Client ID | `fastapi_backend` |
| PostgreSQL Host | `localhost` |
| PostgreSQL Port | `5432` |
| PostgreSQL Database | `fyp_monitoring` |
| PostgreSQL User | `postgres` |
| FastAPI Port | `8000` |
| FastAPI Docs URL | `http://192.168.100.2:8000/docs` |
| React Dev Server | `http://localhost:5173` |
| FYP_Backend path | `C:\Users\Admin\Downloads\Project_MQTT\FYP\FYP_Backend` |
| fyp-frontend path | `C:\Users\Admin\Downloads\Project_MQTT\FYP\fyp-frontend` |
| ESP32 Firmware | `esp32_v4_0_MOSQUITTO.ino` |
| Temp Fan ON | 45°C |
| Temp Fan OFF | 38°C (hysteresis) |
| Temp Critical / Alert Threshold | 75°C |
| Door Timeout | 60 seconds |
| Max Password Attempts | 3 |

---

## Key Concepts Learned Across All Sessions

### Backend (Steps 1–4)
- **Callback pattern** — `on_connect`/`on_message` defined by you, called by paho-mqtt (inversion of control)
- **Threading model** — main thread (uvicorn/HTTP), background thread (paho-mqtt/MQTT)
- **Lifespan hook** — `yield` splits startup and shutdown in FastAPI
- **Loose coupling** — MQTT listener and REST waiter share PostgreSQL as dead-drop
- **Database connection pattern** — `get_db_connection()` vending machine, one connection per caller
- **State vs event log** — `panels` = rolling state (UPDATE), `sensor_readings`/`alerts` = permanent log (INSERT)
- **Return code (`rc`)** — Mosquitto's connection response, delivered by paho-mqtt

### Frontend (Step 5)
- **State (`useState`)** — watched variables that trigger re-render on change
- **Props** — data flows down (parent → child), actions flow up (child calls parent's function)
- **`useEffect`** — side effects: data fetching, timers. Dependency array controls when it re-runs
- **Derived state** — computed from existing state at render time, no extra `useState` needed (e.g. `onlineCount`)
- **`Promise.all()`** — wait for multiple async operations to complete before proceeding
- **Data-driven loading** — loading state tied to actual data arrival, not hardcoded timers
- **Smart vs dumb components** — smart fetch data (PanelDetail), dumb receive and display (StatusInfo, SensorChart)
- **Service layer** — `api.js` centralises all API calls; error handling belongs in callers, not the service
- **Media queries** — CSS `@media` rules as conditional styling based on screen size
- **Flexbox `min-width: 0`** — allows flex children to shrink below content width

### Cross-Cutting
- **CORS** — browser security policy; backend must explicitly allow frontend's origin
- **`localhost` vs IP** — `localhost` = loopback to self; other devices on the network need the actual IP
- **Vite `--host` flag** — binds dev server to `0.0.0.0` (network-accessible) instead of `127.0.0.1` (local only)
- **OOP + functional mix** — `FastAPI()`, `mqtt.Client()` = OOP; `lifespan=lifespan`, `on_connect = on_connect` = functional

---

## Files Modified/Created Across Step 5 (Both Sessions)

### Modified:
| File | Changes |
|------|---------|
| `FYP_Backend/main.py` | CORS middleware, StreamingResponse import, csv/io imports, two export endpoints |

### Created (Session 1 — sub-steps 5.1–5.8):
| File | Purpose |
|------|---------|
| `fyp-frontend/` (entire project) | Vite + React project |
| `src/services/api.js` | Centralised API call functions |
| `src/components/Sidebar.jsx` | Panel list with status dots |
| `src/components/PanelDetail.jsx` | Main content area: chart + status + buttons |
| `src/components/StatusInfo.jsx` | Right column status display |
| `src/components/SensorChart.jsx` | Combined temp/humidity line chart |
| `src/components/LogsModal.jsx` | Export modal: dropdown + date → CSV download |
| `src/App.jsx` | Root component (replaced Vite default) |
| `src/App.css` | All component styling (replaced Vite default) |
| `src/index.css` | Global reset styles (replaced Vite default) |

### Modified (Session 2 — sub-step 5.9):
| File | Changes |
|------|---------|
| `src/App.jsx` | Added: `app-container` wrapper, title bar (name + clock + online count), `isOnline()` helper, `clock` state, `connected` state, error banner, `async loadPanels()` with try/catch |
| `src/App.css` | Added: title bar styles, error banner styles, loading text styles, responsive media query, `min-width: 0` fix, `app-container` wrapper |
| `src/components/StatusInfo.jsx` | Added: `tempClass()`, `doorClass()`, `fanClass()`, `alarmClass()` helper functions for colour coding |
| `src/components/PanelDetail.jsx` | Added: `loading` state, `Promise.all()` for parallel fetching, loading indicator |

---

## Known Issues

1. **ESLint warning in PanelDetail.jsx:** May show warnings about useEffect dependencies — functional but cosmetic
2. **DeprecationWarning in main.py:** `Callback API version 1 is deprecated` — paho-mqtt works fine
3. **Status column in PostgreSQL:** Not used for online/offline — React checks `last_seen` with 15-second threshold instead
4. **Fan status NULL:** Remains NULL until temperature crosses 45°C threshold
5. **No alert acknowledge endpoint:** Filter exists in GET /alerts but no POST endpoint to mark alerts as acknowledged (future work)
6. **No alert recovery logging:** No "resolved" event when alarm conditions clear (future work)
7. **date-fns format:** Chart x-axis uses `MMM dd` format (e.g., "Apr 25") — MMM = short month name, not month number
8. **Phone CORS:** Phone can reach React dev server but CORS blocks FastAPI calls — would need `192.168.100.2:5173` added to allowed origins (not fixed — desktop is primary use case)

---

## Optional Enhancements (Future Work / Time Permitting)

- AlertTable component for displaying alert data within the dashboard (currently alerts only via CSV export)
- Alert acknowledge endpoint (`POST /alerts/{id}/acknowledge`)
- Alert recovery logging (resolved events when alarm conditions clear)
- Auto-scroll sidebar if many panels
- Dark mode toggle
- Panel search/filter in sidebar
- Docker containerisation
- Linode cloud deployment
- TLS/SSL on port 8883
- Hashed Mosquitto passwords
- Daily CSV export endpoint
- Grafana / TimescaleDB integration

---

## Career Context — Complete Talking Points

### Backend & Infrastructure
- **MQTT + Mosquitto** → IoT/infrastructure, message broker configuration
- **FastAPI + REST API design** → Modern Python backend, API contract design
- **Callback pattern + threading model** → Asynchronous programming, concurrency awareness
- **PostgreSQL with schema design** → Normalisation, strategic denormalization, surrogate keys
- **Loose coupling** → MQTT and REST as independent subsystems sharing a data store
- **Alert system with severity levels** → Monitoring and incident response design
- **Two-way MQTT control** → Full-stack IoT (sensor data in, commands out)
- **CSV export (StreamingResponse)** → In-memory file generation, browser download triggering

### Frontend
- **React + Chart.js** → Frontend development, data visualisation
- **Component architecture** → Smart vs dumb components, separation of concerns
- **REST API consumption** → Service layer pattern, centralised API calls
- **CORS configuration** → Cross-origin security, middleware setup
- **HTTP polling** → Real-time data display without WebSockets
- **State management** → useState, useEffect, derived state, props-based data flow
- **Error handling** → Try/catch, connection status detection, user-facing error states
- **Responsive design** → CSS media queries, flexbox layout, graceful degradation
- **Promise.all()** → Parallel async operations, data-driven loading states

### Career Path
CS degree → Junior QA/SDET → Mid-Level SDET → DevOps/DevSecOps/SRE

---

## What Comes Next

**Implementation is complete.** The next phase is FYP academic documentation:
- Final report writing
- System architecture diagrams (reference: `FYP_Diagrams_Reference.md`)
- Testing evidence and screenshots
- User guide / deployment instructions
- Evaluation and critical analysis
- Future work section

This is a separate workstream from implementation — kept strictly separate per Edwin's scope discipline.
