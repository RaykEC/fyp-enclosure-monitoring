import { useState } from "react";
import { exportReadings, exportAlerts } from "../services/api";

function LogsModal({ panelId, onClose }) {
  const [logType, setLogType] = useState("readings");
  const [fromDate, setFromDate] = useState("");
  const [error, setError] = useState("");

  function handleSubmit() {
    // Validate date input
    if (!fromDate || fromDate.length !== 8 || isNaN(fromDate)) {
      setError("Please enter a valid value (YYYYMMDD)");
      return;
    }

    // Validate it's a real date
    const year = fromDate.substring(0, 4);
    const month = fromDate.substring(4, 6);
    const day = fromDate.substring(6, 8);
    const testDate = new Date(`${year}-${month}-${day}`);
    if (isNaN(testDate.getTime())) {
      setError("Please enter a valid value (YYYYMMDD)");
      return;
    }

    setError("");

    if (logType === "readings") {
      exportReadings(panelId, fromDate).catch((err) => {
        setError(err.message);
      });
    } else {
      exportAlerts(panelId, fromDate).catch((err) => {
        setError(err.message);
      });
    }
  }

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal-content" onClick={(e) => e.stopPropagation()}>
        <h2>Export Logs — Panel {panelId}</h2>

        <div className="modal-field">
          <label className="modal-label">Please select:</label>
          <select
            className="modal-select"
            value={logType}
            onChange={(e) => setLogType(e.target.value)}
          >
            <option value="readings">Sensor Readings</option>
            <option value="alerts">Alerts</option>
          </select>
        </div>

        <div className="modal-field">
          <label className="modal-label">Date:</label>
          <input
            className="modal-input"
            type="text"
            value={fromDate}
            onChange={(e) => setFromDate(e.target.value)}
            placeholder="YYYYMMDD"
          />
          <span className="modal-hint">(YYYYMMDD)</span>
        </div>

        {error && <p className="modal-error">{error}</p>}

        <div className="modal-buttons">
          <button className="btn btn-submit" onClick={handleSubmit}>Submit</button>
          <button className="btn btn-cancel" onClick={onClose}>Cancel</button>
        </div>
      </div>
    </div>
  );
}

export default LogsModal;