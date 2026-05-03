function StatusInfo({ panel }) {
  if (!panel) return <p>Loading...</p>;

  function formatTime(timestamp) {
    if (!timestamp) return "Never";
    return new Date(timestamp).toLocaleString();
  }

  function tempClass(temp) {
    if (temp === null) return "";
    if (temp >= 75) return "critical";
    if (temp >= 45) return "warning";
    return "";
  }

  function doorClass(status) {
    if (!status) return "";
    if (status === "TIMEOUT" || status === "WRONG_CODE") return "critical";
    if (status === "OPEN") return "warning";
    return "";
  }

  function fanClass(status) {
    if (status === "ON") return "warning";
    return "";
  }

  function alarmClass(status) {
    if (!status) return "";
    if (status === "CRITICAL_TEMP" || status === "WRONG_CODE") return "critical";
    if (status === "TIMEOUT" || status === "TEMP_ACKED") return "warning";
    return "";
  }

  return (
    <div className="status-info">
      <div className="status-row">
        <span className="status-label">Last Updated</span>
        <span className="status-value">{formatTime(panel.last_seen)}</span>
      </div>

      <div className="status-row">
        <span className="status-label">Temperature</span>
        <span className={`status-value ${tempClass(panel.current_temperature)}`}>
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
        <span className={`status-value ${doorClass(panel.current_door_status)}`}>
          {panel.current_door_status || "—"}
        </span>
      </div>

      <div className="status-row">
        <span className="status-label">Fan</span>
        <span className={`status-value ${fanClass(panel.current_fan_status)}`}>
          {panel.current_fan_status || "—"}
        </span>
      </div>

      <div className="status-row">
        <span className="status-label">Alarm</span>
        <span className={`status-value ${alarmClass(panel.current_alarm_status)}`}>
          {panel.current_alarm_status || "—"}
        </span>
      </div>
    </div>
  );
}

export default StatusInfo;