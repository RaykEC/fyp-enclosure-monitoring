import { useState, useEffect } from "react";
import StatusInfo from "./StatusInfo";
import { fetchPanel, sendReset } from "../services/api";

function PanelDetail({ selectedPanel }) {
  const [panel, setPanel] = useState(null);

  // Fetch panel data on selection change + poll every 5 seconds
  useEffect(() => {

    fetchPanel(selectedPanel).then((data) => setPanel(data));

    const interval = setInterval(() => {
      fetchPanel(selectedPanel).then((data) => setPanel(data));
    }, 5000);

    return () => clearInterval(interval);
  }, [selectedPanel]);

  function isOnline(lastSeen) {
    if (!lastSeen) return false;
    return (new Date() - new Date(lastSeen)) < 15000;
  }

  function handleReboot() {
    if (window.confirm(`Are you sure you want to reboot Panel ${selectedPanel}?`)) {
      sendReset(selectedPanel).then((response) => {
        alert(response.message);
      });
    }
  }

  return (
    <div className="panel-detail">
      <div className="panel-header">
        <span className={`status-dot ${panel && isOnline(panel.last_seen) ? "online" : "offline"}`}></span>
        <h1>Panel {selectedPanel}</h1>
        <span className="panel-location">{panel?.location || "—"}</span>
      </div>

      <div className="panel-body">
        <div className="chart-area">
          <p>Chart goes here (sub-step 5.5)</p>
        </div>

        <div className="info-area">
          <StatusInfo panel={panel} />

          <div className="action-buttons">
            <button className="btn btn-logs" onClick={() => alert("Logs modal coming in sub-step 5.7")}>
              Logs
            </button>
            <button className="btn btn-reboot" onClick={handleReboot}>
              Reboot
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}

export default PanelDetail;