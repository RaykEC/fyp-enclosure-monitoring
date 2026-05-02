import { useState, useEffect } from "react";
import Sidebar from "./components/Sidebar";
import { fetchPanels } from "./services/api";
import "./App.css";

function App() {
  const [panels, setPanels] = useState([]);
  const [selectedPanel, setSelectedPanel] = useState(1);

  // Poll for panel data every 5 seconds
  useEffect(() => {
    // Fetch immediately on load
    fetchPanels().then((data) => setPanels(data));

    // Then fetch every 5 seconds
    const interval = setInterval(() => {
      fetchPanels().then((data) => setPanels(data));
    }, 5000);

    // Cleanup — stop polling when component unmounts
    return () => clearInterval(interval);
  }, []);

  return (
    <div className="app-layout">
      <Sidebar
        panels={panels}
        selectedPanel={selectedPanel}
        onSelectPanel={setSelectedPanel}
      />
      <div className="main-content">
        <h1>Panel {selectedPanel} — Detail View</h1>
        <p>PanelDetail component goes here (sub-step 5.4)</p>
      </div>
    </div>
  );
}

export default App;