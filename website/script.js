// ===================== WebSocket =====================
let socket;
let isConnected = false;
let lastTelemetry = null;

// Mode state
let currentMode = "AUTO"; // default UI state

// ===================== DOM =====================
const elements = {
  arduinoStatus: document.getElementById("arduino-status"),
  modeStatus: document.getElementById("mode-status"),
  lastUpdate: document.getElementById("last-update"),

  temperature: document.getElementById("temperature-value"),
  targetTemp: document.getElementById("target-temp"),

  humidity: document.getElementById("humidity-value"),
  targetHumidity: document.getElementById("target-humidity"),

  logContainer: document.getElementById("log-container"),
};

// ===================== Helpers =====================
function addLog(message) {
  const logContainer = elements.logContainer;
  if (!logContainer) return;

  const logEntry = document.createElement("div");
  logEntry.className = "log-entry";

  const time = new Date().toLocaleTimeString();
  logEntry.innerHTML = `
    <span class="log-time">${time}</span>
    <span class="log-message">${message}</span>
  `;
  logContainer.appendChild(logEntry);
  logContainer.scrollTop = logContainer.scrollHeight;
}

function showToast(message, type = "info") {
  document.querySelectorAll(".toast").forEach((toast) => toast.remove());

  const toast = document.createElement("div");
  toast.className = `toast ${type}`;
  toast.textContent = message;

  document.body.appendChild(toast);

  setTimeout(() => toast.classList.add("show"), 10);
  setTimeout(() => {
    toast.classList.remove("show");
    setTimeout(() => toast.remove(), 300);
  }, 2500);
}

// ===================== UI: Arduino status =====================
function updateArduinoStatus(connected) {
  const statusElement = elements.arduinoStatus?.querySelector(".status-value");
  if (!statusElement) return;

  if (connected) {
    statusElement.textContent = "Connected";
    statusElement.className = "status-value connected";
  } else {
    statusElement.textContent = "Disconnected";
    statusElement.className = "status-value disconnected";
  }
}

// ===================== UI: telemetry =====================
function updateTemperatureColor(temp) {
  const targetTemp = parseFloat(elements.targetTemp?.textContent || "37.6");
  const diff = temp - targetTemp;

  if (Math.abs(diff) <= 0.3) elements.temperature.style.color = "#28a745";
  else if (diff > 0) elements.temperature.style.color = "#dc3545";
  else elements.temperature.style.color = "#007bff";
}

function updateTelemetry(data) {
  if (data?.temperature !== null && data?.temperature !== undefined) {
    elements.temperature.textContent = Number(data.temperature).toFixed(1);
    updateTemperatureColor(Number(data.temperature));
  } else {
    elements.temperature.textContent = "--";
  }

  if (data?.humidity !== null && data?.humidity !== undefined) {
    elements.humidity.textContent = Number(data.humidity).toFixed(0);
  } else {
    elements.humidity.textContent = "--";
  }

  elements.lastUpdate.textContent = new Date().toLocaleTimeString();
  lastTelemetry = data;
}

// ===================== Freeze controls in AUTO =====================
// IMPORTANT: keep emergency + reset + mode buttons enabled in both modes
function setControlsEnabled(enabled) {
  const controls = document.querySelectorAll("button, input, select, textarea");

  controls.forEach((el) => {
    el.disabled = !enabled;
  });

  // Always enabled buttons (work in AUTO and MANUAL)
  const alwaysEnabledIds = [
    "auto-mode",
    "manual-mode",
    "emergency-stop",
    "system-reset",
  ];

  alwaysEnabledIds.forEach((id) => {
    const el = document.getElementById(id);
    if (el) el.disabled = false;
  });

  // If your emergency/reset buttons are not using these IDs,
  // you can add their real IDs here.
}

// ===================== Mode handling =====================
function applyModeUI(mode) {
  currentMode = mode;

  const autoBtn = document.getElementById("auto-mode");
  const manualBtn = document.getElementById("manual-mode");

  if (autoBtn) autoBtn.classList.toggle("active", mode === "AUTO");
  if (manualBtn) manualBtn.classList.toggle("active", mode === "MANUAL");

  if (elements.modeStatus) elements.modeStatus.textContent = mode;

  // AUTO => freeze everything except emergency/reset/mode
  setControlsEnabled(mode === "MANUAL");
}

// ===================== Socket init =====================
function initWebSocket() {
  socket = io(window.location.origin, {
    transports: ["websocket", "polling"],
  });

  socket.on("connect", () => {
    isConnected = true;
    addLog("Connected to server");
  });

  socket.on("disconnect", () => {
    isConnected = false;
    addLog("Disconnected from server");
    updateArduinoStatus(false);
  });

  socket.on("arduino-status", (data) => {
    updateArduinoStatus(!!data.connected);
  });

  socket.on("telemetry", (data) => {
    updateTelemetry(data);
  });

  socket.on("command-status", (data) => {
    if (data.success) showToast(`Command sent: ${data.command}`, "success");
    else showToast(`Failed: ${data.command} - ${data.error}`, "error");
  });

  socket.on("connect_error", (err) => {
    addLog("SOCKET ERROR: " + err.message);
  });
}

// ===================== Commands =====================
function sendControl(type, value) {
  if (!isConnected) {
    showToast("Not connected to server", "error");
    return;
  }
  socket.emit("control-command", { type, value });
}

// ===================== GLOBAL FUNCTIONS (for onclick) =====================

// Mode
window.setMode = function setMode(mode) {
  applyModeUI(mode);
  sendControl("mode", mode);
  addLog(`Mode changed to ${mode}`);
};

// Temperature/Humidity targets
window.setTemperature = function setTemperature() {
  const temp = document.getElementById("temp-setpoint")?.value;
  if (!temp) return;
  if (elements.targetTemp) elements.targetTemp.textContent = temp;

  sendControl("temperature", parseFloat(temp));
  addLog(`Target temperature set to ${temp}°C`);
};

window.setHumidity = function setHumidity() {
  const hum = document.getElementById("humidity-setpoint")?.value;
  if (!hum) return;
  if (elements.targetHumidity) elements.targetHumidity.textContent = hum;

  sendControl("humidity", parseInt(hum));
  addLog(`Target humidity set to ${hum}%`);
};

window.setDay = function setDay() {
  const day = document.getElementById("day-input")?.value;
  if (!day) return;

  sendControl("day", parseInt(day));
  addLog(`Incubation day set to ${day}`);

  // update UI target humidity based on day
  const d = parseInt(day);
  let targetRH;
  if (d <= 7) targetRH = 56;
  else if (d <= 18) targetRH = 60;
  else targetRH = 65;
  if (elements.targetHumidity) elements.targetHumidity.textContent = targetRH;
};

window.setEggCount = function setEggCount() {
  const eggs = document.getElementById("egg-count")?.value;
  if (!eggs) return;

  sendControl("egg_count", parseInt(eggs));
  addLog(`Egg count set to ${eggs}`);
};

// Toggles
window.toggleVentilation = function toggleVentilation() {
  const enabled = document.getElementById("vent-fan-switch")?.checked;
  sendControl("ventilation", !!enabled);
  addLog(`Ventilation fan ${enabled ? "enabled" : "disabled"}`);
};

window.toggleHeating = function toggleHeating() {
  const enabled = document.getElementById("heat-switch")?.checked;
  sendControl("heating", !!enabled);
  addLog(`Heating system ${enabled ? "enabled" : "disabled"}`);
};

window.toggleHumiditySystem = function toggleHumiditySystem() {
  const enabled = document.getElementById("humidity-switch")?.checked;
  sendControl("humidity_system", !!enabled);
  addLog(`Humidity system ${enabled ? "enabled" : "disabled"}`);
};

window.toggleAutoFlip = function toggleAutoFlip() {
  const enabled = document.getElementById("flip-switch")?.checked;
  sendControl("flip", !!enabled);
  addLog(`Auto flip ${enabled ? "enabled" : "disabled"}`);
};

// Flip actions
window.startFlipSession = function startFlipSession() {
  sendControl("start_flip_session", true);
  addLog("Flip session START requested");
};

window.stopFlipSession = function stopFlipSession() {
  sendControl("stop_flip_session", true);
  addLog("Flip session STOP requested");
};

// Flip settings
window.setFlipInterval = function setFlipInterval() {
  // IMPORTANT: make sure your input id is flip-interval
  const v = document.getElementById("flip-interval")?.value;
  if (!v) return;

  const hours = parseFloat(v);
  sendControl("flip_interval_hours", hours);
  addLog(`Flip interval set to ${hours} hours`);
};

window.setFlipDuration = function setFlipDuration() {
  // IMPORTANT: make sure your input id is flip-duration
  const v = document.getElementById("flip-duration")?.value;
  if (!v) return;

  const mins = parseFloat(v);
  sendControl("flip_duration_minutes", mins);
  addLog(`Flip duration set to ${mins} minutes`);
};

// Vent manual trigger
window.triggerVentilationNow = function triggerVentilationNow() {
  sendControl("trigger_vent_now", true);
  addLog("Ventilation TRIGGER NOW requested");
};

// Water / buzzer
window.controlWaterValve = function controlWaterValve(open) {
  sendControl("water_valve", !!open);
  addLog(`Water valve ${open ? "opened" : "closed"}`);
};

window.testBuzzer = function testBuzzer() {
  sendControl("buzzer_test", true);
  setTimeout(() => sendControl("buzzer_test", false), 1000);
  addLog("Buzzer test");
};

window.silenceAlarm = function silenceAlarm() {
  sendControl("silence_alarm", true);
  addLog("Alarm silenced");
};

// Emergency + Reset (ALWAYS enabled in both modes)
window.emergencyStop = function emergencyStop() {
  if (confirm("⚠️ ARE YOU SURE?\nThis will stop ALL systems immediately!")) {
    sendControl("emergency_stop", true);

    // reset toggles UI (optional)
    document
      .querySelectorAll(".toggle-switch input")
      .forEach((el) => (el.checked = false));

    addLog("!!! EMERGENCY STOP ACTIVATED !!!");
    showToast("EMERGENCY STOP ACTIVATED", "error");
  }
};

window.systemReset = function systemReset() {
  if (confirm("Reset all systems to default safe state?")) {
    sendControl("reset", true);
    addLog("System reset initiated");
    showToast("System reset", "warning");
  }
};

// Logs
window.clearLogs = function clearLogs() {
  if (elements.logContainer) elements.logContainer.innerHTML = "";
  addLog("Logs cleared");
};

// ===================== Start =====================
document.addEventListener("DOMContentLoaded", () => {
  initWebSocket();

  // Default AUTO mode => freeze controls except emergency/reset/mode
  applyModeUI("AUTO");

  addLog("Web interface initialized");
});
