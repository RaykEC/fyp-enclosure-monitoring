function Sidebar({ panels, selectedPanel, onSelectPanel }) {

  function isOnline(lastSeen) {
    if (!lastSeen) return false;
    const now = new Date();
    const seen = new Date(lastSeen);
    return (now - seen) < 15000;
  }

  return (
    <div className="sidebar">
      <h2>Panels</h2>
      <div className="panel-list">
        {panels.map((panel) => (
          <div
            key={panel.panel_id}
            className={`panel-item ${selectedPanel === panel.panel_id ? "selected" : ""}`}
            onClick={() => onSelectPanel(panel.panel_id)}
          >
            <span
              className={`status-dot ${isOnline(panel.last_seen) ? "online" : "offline"}`}
            ></span>
            <span className="panel-name">Panel {panel.panel_id}</span>
          </div>
        ))}
      </div>
    </div>
  );
}

export default Sidebar;