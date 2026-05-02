import { useState, useEffect } from "react";
import Sidebar from "./components/Sidebar";
import PanelDetail from "./components/PanelDetail";
import { fetchPanels } from "./services/api";
import "./App.css";

function App() {
  const [panels, setPanels] = useState([]);
  const [selectedPanel, setSelectedPanel] = useState(1);

  useEffect(() => {
    fetchPanels().then((data) => setPanels(data));

    const interval = setInterval(() => {
      fetchPanels().then((data) => setPanels(data));
    }, 5000);

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
        <PanelDetail selectedPanel={selectedPanel} />
      </div>
    </div>
  );
}

export default App;