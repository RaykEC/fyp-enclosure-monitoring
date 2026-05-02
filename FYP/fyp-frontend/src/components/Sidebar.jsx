function Sidebar({ panels, selectedPanel, onSelectPanel }) {
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
              className={`status-dot ${panel.status === "online" ? "online" : "offline"}`}
            ></span>
            <span className="panel-name">Panel {panel.panel_id}</span>
          </div>
        ))}
      </div>
    </div>
  );
}

export default Sidebar;