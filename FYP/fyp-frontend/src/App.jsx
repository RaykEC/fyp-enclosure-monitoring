import { useState, useEffect } from "react";
import Sidebar from "./components/Sidebar";
import PanelDetail from "./components/PanelDetail";
import { fetchPanels } from "./services/api";
import "./App.css";

function isOnline(lastSeen) {
  if (!lastSeen) return false;
  return new Date() - new Date(lastSeen) < 15000;
}

function App() {
  const [panels, setPanels] = useState([]);
  const [selectedPanel, setSelectedPanel] = useState(1);
  const [clock, setClock] = useState(new Date());
  const [connected, setConnected] = useState(true);

  // Tick the clock every second
  useEffect(() => {
    const timer = setInterval(() => setClock(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Fetch panels every 5 seconds
  useEffect(() => {
    async function loadPanels() {
      try {
        const data = await fetchPanels();
        setPanels(data);
        setConnected(true);
      } catch {
        setConnected(false);
      }
    }

    loadPanels();
    const interval = setInterval(loadPanels, 5000);
    return () => clearInterval(interval);
  }, []);

  // Derived state
  const onlineCount = panels.filter((p) => isOnline(p.last_seen)).length;

  return (
    <div className="app-container">
      <div className="title-bar">
        <h1 className="title-bar-name">Enclosure Monitoring System</h1>
        <div className="title-bar-info">
          <span className="title-bar-panels">
            <span className="online-dot" /> {onlineCount}/{panels.length} Online
          </span>
          <span className="title-bar-clock">
            {clock.toLocaleDateString("en-GB", {
              weekday: "short",
              day: "numeric",
              month: "short",
              year: "numeric",
            })}{" "}
            {clock.toLocaleTimeString("en-GB")}
          </span>
        </div>
      </div>

      {!connected && (
        <div className="error-banner">
          ⚠ Backend unreachable — displaying last known data
        </div>
      )}

      <div className="app-layout">
        <Sidebar
          panels={panels}
          selectedPanel={selectedPanel}
          onSelectPanel={setSelectedPanel}
        />
        <div className="main-content">
          <PanelDetail selectedPanel={selectedPanel} />
        </div>
      </div>
    </div>
  );
}

export default App;