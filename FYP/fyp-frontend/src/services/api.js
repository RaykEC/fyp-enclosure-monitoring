// Base URL of FastAPI backend
const API_BASE = "http://192.168.100.2:8000";

// ── Dashboard endpoints (return JSON) ──

export async function fetchPanels() {
  const response = await fetch(`${API_BASE}/panels`);
  return response.json();
}

export async function fetchPanel(panelId) {
  const response = await fetch(`${API_BASE}/panels/${panelId}`);
  return response.json();
}

export async function fetchHistory(panelId, hours = 720) {
  const response = await fetch(`${API_BASE}/panels/${panelId}/history?hours=${hours}`);
  return response.json();
}

export async function fetchAlerts() {
  const response = await fetch(`${API_BASE}/alerts`);
  return response.json();
}

export async function sendReset(panelId) {
  const response = await fetch(`${API_BASE}/panels/${panelId}/control/reset`, {
    method: "POST",
  });
  return response.json();
}

// ── Export endpoints (trigger CSV download) ──

export async function exportReadings(panelId, fromDate) {
  const response = await fetch(`${API_BASE}/panels/${panelId}/export/readings?from_date=${fromDate}`);
  if (!response.ok) {
    const error = await response.json();
    throw new Error(error.detail);
  }
  const blob = await response.blob();
  const url = window.URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = `panel_${panelId}_readings_${fromDate}.csv`;
  a.click();
  window.URL.revokeObjectURL(url);
}

export async function exportAlerts(panelId, fromDate) {
  const response = await fetch(`${API_BASE}/panels/${panelId}/export/alerts?from_date=${fromDate}`);
  if (!response.ok) {
    const error = await response.json();
    throw new Error(error.detail);
  }
  const blob = await response.blob();
  const url = window.URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = `panel_${panelId}_alerts_${fromDate}.csv`;
  a.click();
  window.URL.revokeObjectURL(url);
}