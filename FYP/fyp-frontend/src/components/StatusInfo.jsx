function StatusInfo({ panel }) {
  if (!panel) return <p>Loading...</p>;

  function formatTime(timestamp) {
    if (!timestamp) return "Never";
    return new Date(timestamp).toLocaleString();
  }

  return (
    <div className="status-info">
      <div className="status-row">
        <span className="status-label">Last Updated</span>
        <span className="status-value">{formatTime(panel.last_seen)}</span>
      </div>

      <div className="status-row">
        <span className="status-label">Temperature</span>
        <span className="status-value">
          {panel.current_temperature !== null
            ? `${panel.current_temperature}°C`
            : "—"}
        </span>
      </div>

      <div className="status-row">
        <span className="status-label">Humidity</span>
        <span className="status-value">
          {panel.current_humidity !== null
            ? `${panel.current_humidity}%`
            : "—"}
        </span>
      </div>

      <div className="status-row">
        <span className="status-label">Door</span>
        <span className={`status-value ${panel.current_door_status === "OPEN" ? "warning" : ""}`}>
          {panel.current_door_status || "—"}
        </span>
      </div>

      <div className="status-row">
        <span className="status-label">Fan</span>
        <span className="status-value">
          {panel.current_fan_status || "—"}
        </span>
      </div>

      <div className="status-row">
        <span className="status-label">Alarm</span>
        <span className={`status-value ${
          panel.current_alarm_status === "CRITICAL_TEMP" ||
          panel.current_alarm_status === "WRONG_CODE"
            ? "critical"
            : panel.current_alarm_status === "TIMEOUT"
            ? "warning"
            : ""
        }`}>
          {panel.current_alarm_status || "—"}
        </span>
      </div>
    </div>
  );
}

export default StatusInfo;