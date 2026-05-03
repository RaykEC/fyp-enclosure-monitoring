import { useState, useEffect } from "react";
import StatusInfo from "./StatusInfo";
import SensorChart from "./SensorChart";
import LogsModal from "./LogsModal";
import { fetchPanel, fetchHistory, sendReset } from "../services/api";

function PanelDetail({ selectedPanel }) {
  const [panel, setPanel] = useState(null);
  const [history, setHistory] = useState([]);
  const [showLogs, setShowLogs] = useState(false);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    setLoading(true); /*ELSn warning not a bug */

    Promise.all([
      fetchPanel(selectedPanel),
      fetchHistory(selectedPanel, 720),
    ]).then(([panelData, historyData]) => {
      setPanel(panelData);
      setHistory(historyData);
      setLoading(false);
    }).catch(() => {
      setLoading(false);
    });

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

  if (loading) {
    return (
      <div className="panel-detail">
        <p className="loading-text">Loading Panel {selectedPanel}...</p>
      </div>
    );
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
          <SensorChart history={history} />
        </div>

        <div className="info-area">
          <StatusInfo panel={panel} />

          <div className="action-buttons">
            <button className="btn btn-logs" onClick={() => setShowLogs(true)}>
              Logs
            </button>
            <button className="btn btn-reboot" onClick={handleReboot}>
              Reboot
            </button>
          </div>
        </div>
      </div>

      {showLogs && (
        <LogsModal
          panelId={selectedPanel}
          onClose={() => setShowLogs(false)}
        />
      )}
    </div>
  );
}

export default PanelDetail;