// WebSocket connection
let socket;
let isConnected = false;
let lastTelemetry = null;

// ====== MODE STATE ======
let currentMode = "AUTO"; // default (matches UI initial)

// DOM Elements
const elements = {
  arduinoStatus: document.getElementById("arduino-status"),
  modeStatus: document.getElementById("mode-status"),
  lastUpdate: document.getElementById("last-update"),
  temperature: document.getElementById("temperature-value"),
  targetTemp: document.getElementById("target-temp"),
  humidity: document.getElementById("humidity-value"),
  targetHumidity: document.getElementById("target-humidity"),
  heaterStatus: document.getElementById("heater-status"),
  humidifierStatus: document.getElementById("humidifier-status"),
  ventState: document.getElementById("vent-state"),
  nextVent: document.getElementById("next-vent"),
  ventReason: document.getElementById("vent-reason"),
  flipStatus: document.getElementById("flip-status"),
  cycleCount: document.getElementById("cycle-count"),
  nextFlip: document.getElementById("next-flip"),
  waterLevel: document.getElementById("water-level"),
  valveStatus: document.getElementById("valve-status"),
  doorStatus: document.getElementById("door-status"),
  buzzerStatus: document.getElementById("buzzer-status"),
  logContainer: document.getElementById("log-container"),
};

// ============ Helpers ============

// Allowed always (even in AUTO)
const ALWAYS_ALLOWED_TYPES = new Set(["mode", "emergency_stop", "reset"]);

// Check if command is allowed in current mode
function canSendCommand(type) {
  if (!type) return false;
  if (ALWAYS_ALLOWED_TYPES.has(type)) return true;
  return currentMode === "MANUAL";
}

// Unified emitter with AUTO freeze behavior
function emitControl(type, value) {
  if (!socket) {
    showToast("Socket not ready", "error");
    return false;
  }

  // If AUTO: block everything except allowed types
  if (!canSendCommand(type)) {
    showToast(
      "AUTO mode: controls are disabled (Safety buttons still work)",
      "warning"
    );
    addLog(`BLOCKED in AUTO: ${type}`);
    return false;
  }

  // Send
  socket.emit("control-command", { type, value });
  return true;
}

// Freeze/Unfreeze UI controls (visual + prevent clicks on inputs/buttons)
// Keep: mode buttons + safety buttons (they are handled by ALWAYS_ALLOWED_TYPES anyway)
function applyUiFreeze() {
  const isAuto = currentMode === "AUTO";

  // Disable all inputs & buttons EXCEPT mode buttons.
  // Safety buttons don't have IDs, but even if disabled visually,
  // JS still allows them through. We will NOT disable them by selector,
  // so they remain clickable.
  const modeBtns = new Set(["auto-mode", "manual-mode"]);

  // Disable all INPUTS except nothing (we freeze them in AUTO)
  document.querySelectorAll("input, select, textarea").forEach((el) => {
    // allow none in AUTO (freeze config too as you requested)
    el.disabled = isAuto;
  });

  // Disable all BUTTONS except mode buttons + emergency/reset (by class)
  document.querySelectorAll("button").forEach((btn) => {
    const id = btn.id || "";
    const isModeBtn = modeBtns.has(id);

    // Keep safety buttons always enabled
    const isSafetyBtn =
      btn.classList.contains("btn-emergency") ||
      btn.classList.contains("btn-reset");

    btn.disabled = isAuto ? !(isModeBtn || isSafetyBtn) : false;
  });

  // Optional small hint in logs
  if (isAuto) {
    addLog("AUTO mode: UI controls frozen (Safety buttons still enabled).");
  } else {
    addLog("MANUAL mode: UI controls enabled.");
  }
}

// Initialize WebSocket connection
function initWebSocket() {
  socket = io("http://localhost:3000", {
    transports: ["websocket", "polling"],
  });

  socket.on("connect_error", (err) => {
    console.log("SOCKET connect_error:", err.message);
    addLog("SOCKET ERROR: " + err.message);
  });

  socket.on("connect", () => {
    addLog("Connected to server");
    isConnected = true;
  });

  socket.on("disconnect", () => {
    addLog("Disconnected from server");
    isConnected = false;
    updateArduinoStatus(false);
  });

  socket.on("arduino-status", (data) => {
    updateArduinoStatus(data.connected);
  });

  socket.on("telemetry", (data) => {
    updateTelemetry(data);
  });

  socket.on("command-status", (data) => {
    if (data.success) {
      showToast(`Command sent: ${data.command}`, "success");
    } else {
      showToast(`Failed: ${data.command} - ${data.error}`, "error");
    }
  });
}

// Update Arduino connection status
function updateArduinoStatus(connected) {
  const statusElement = elements.arduinoStatus.querySelector(".status-value");
  if (connected) {
    statusElement.textContent = "Connected";
    statusElement.className = "status-value connected";
  } else {
    statusElement.textContent = "Disconnected";
    statusElement.className = "status-value disconnected";
  }
}

// Update telemetry data
function updateTelemetry(data) {
  if (data.temperature !== null) {
    elements.temperature.textContent = data.temperature.toFixed(1);
    updateTemperatureColor(data.temperature);
  }

  if (data.humidity !== null) {
    elements.humidity.textContent = data.humidity.toFixed(0);
  }

  elements.lastUpdate.textContent = new Date().toLocaleTimeString();
  lastTelemetry = data;
}

// Update temperature color based on value
function updateTemperatureColor(temp) {
  const tempValue = parseFloat(temp);
  const targetTemp = parseFloat(elements.targetTemp.textContent);
  const diff = tempValue - targetTemp;

  if (Math.abs(diff) <= 0.3) {
    elements.temperature.style.color = "#28a745"; // Green
  } else if (diff > 0) {
    elements.temperature.style.color = "#dc3545"; // Red (too hot)
  } else {
    elements.temperature.style.color = "#007bff"; // Blue (too cold)
  }
}

// Control Functions
function setMode(mode) {
  if (!isConnected) {
    showToast("Not connected to Arduino", "error");
    return;
  }

  // Update state
  currentMode = mode;

  // Update UI
  document
    .getElementById("auto-mode")
    .classList.toggle("active", mode === "AUTO");
  document
    .getElementById("manual-mode")
    .classList.toggle("active", mode === "MANUAL");
  elements.modeStatus.textContent = mode;

  // Freeze UI controls based on mode
  applyUiFreeze();

  // Send command (always allowed)
  emitControl("mode", mode);

  addLog(`Mode changed to ${mode}`);
}

function setTemperature() {
  const temp = document.getElementById("temp-setpoint").value;
  elements.targetTemp.textContent = temp;

  if (emitControl("temperature", parseFloat(temp))) {
    addLog(`Target temperature set to ${temp}°C`);
  }
}

function setHumidity() {
  const hum = document.getElementById("humidity-setpoint").value;
  elements.targetHumidity.textContent = hum;

  if (emitControl("humidity", parseInt(hum))) {
    addLog(`Target humidity set to ${hum}%`);
  }
}

function setDay() {
  const day = document.getElementById("day-input").value;

  if (emitControl("day", parseInt(day))) {
    addLog(`Incubation day set to ${day}`);
  }

  // Update RH based on day (UI only)
  let targetRH;
  if (day <= 7) targetRH = 56;
  else if (day <= 18) targetRH = 60;
  else targetRH = 65;

  elements.targetHumidity.textContent = targetRH;
}

function setEggCount() {
  const eggs = document.getElementById("egg-count").value;

  if (emitControl("egg_count", parseInt(eggs))) {
    addLog(`Egg count set to ${eggs}`);
  }
}

function toggleVentilation() {
  const enabled = document.getElementById("vent-fan-switch").checked;

  if (emitControl("ventilation", enabled)) {
    addLog(`Ventilation fan ${enabled ? "enabled" : "disabled"}`);
  } else {
    // revert UI toggle if blocked
    document.getElementById("vent-fan-switch").checked = !enabled;
  }
}

function toggleHeating() {
  const enabled = document.getElementById("heat-switch").checked;

  if (emitControl("heating", enabled)) {
    addLog(`Heating system ${enabled ? "enabled" : "disabled"}`);
  } else {
    document.getElementById("heat-switch").checked = !enabled;
  }
}

function toggleHumiditySystem() {
  const enabled = document.getElementById("humidity-switch").checked;

  if (emitControl("humidity_system", enabled)) {
    addLog(`Humidity system ${enabled ? "enabled" : "disabled"}`);
  } else {
    document.getElementById("humidity-switch").checked = !enabled;
  }
}

function toggleAutoFlip() {
  const enabled = document.getElementById("flip-switch").checked;

  if (emitControl("flip", enabled)) {
    addLog(`Auto flip ${enabled ? "enabled" : "disabled"}`);
  } else {
    document.getElementById("flip-switch").checked = !enabled;
  }
}

function startFlipSession() {
  if (emitControl("start_flip_session")) {
    addLog("Flip session started");
  }
}

function stopFlipSession() {
  // Your server maps "flip false" to stop
  if (emitControl("flip", false)) {
    addLog("Flip session stopped");
  }
}

function setFlipInterval() {
  const interval = document.getElementById("flip-interval").value;
  addLog(`Flip interval set to ${interval} hours`);
  // (Optional) add a new command type later
}

function setFlipDuration() {
  const duration = document.getElementById("flip-duration").value;
  addLog(`Flip duration set to ${duration} minutes`);
  // (Optional) add a new command type later
}

function controlWaterValve(open) {
  if (emitControl("water_valve", open)) {
    addLog(`Water valve ${open ? "opened" : "closed"}`);
  }
}

function triggerVentilation() {
  // Manual override: ON 5s then OFF
  if (!emitControl("ventilation", true)) return;

  setTimeout(() => {
    emitControl("ventilation", false);
  }, 5000);

  addLog("Manual ventilation triggered");
}

function testBuzzer() {
  if (!emitControl("buzzer_test", true)) return;

  setTimeout(() => {
    emitControl("buzzer_test", false);
  }, 1000);

  addLog("Buzzer test");
}

function silenceAlarm() {
  if (emitControl("silence_alarm")) {
    addLog("Alarm silenced");
  }
}

function emergencyStop() {
  // ALWAYS allowed even in AUTO
  if (confirm("⚠️ ARE YOU SURE?\nThis will stop ALL systems immediately!")) {
    emitControl("emergency_stop");

    // Reset all switches (UI)
    document.querySelectorAll(".toggle-switch input").forEach((switchEl) => {
      switchEl.checked = false;
    });

    addLog("!!! EMERGENCY STOP ACTIVATED !!!");
    showToast("EMERGENCY STOP ACTIVATED", "error");
  }
}

function systemReset() {
  // ALWAYS allowed even in AUTO
  if (confirm("Reset all systems to default state?")) {
    emitControl("reset");
    addLog("System reset initiated");
    showToast("System reset", "warning");
  }
}

// Utility Functions
function addLog(message) {
  const logContainer = elements.logContainer;
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

function clearLogs() {
  elements.logContainer.innerHTML = "";
  addLog("Logs cleared");
}

function showToast(message, type = "info") {
  // Remove existing toasts
  document.querySelectorAll(".toast").forEach((toast) => toast.remove());

  const toast = document.createElement("div");
  toast.className = `toast ${type}`;
  toast.textContent = message;

  document.body.appendChild(toast);

  // Show toast
  setTimeout(() => toast.classList.add("show"), 10);

  // Hide after 3 seconds
  setTimeout(() => {
    toast.classList.remove("show");
    setTimeout(() => toast.remove(), 300);
  }, 3000);
}

// Initialize the application
document.addEventListener("DOMContentLoaded", () => {
  initWebSocket();
  addLog("Web interface initialized");

  // Set initial mode based on UI active button
  const autoBtn = document.getElementById("auto-mode");
  const manualBtn = document.getElementById("manual-mode");
  if (manualBtn && manualBtn.classList.contains("active"))
    currentMode = "MANUAL";
  else currentMode = "AUTO";

  elements.modeStatus.textContent = currentMode;
  applyUiFreeze();

  // Update time every second
  setInterval(() => {
    if (lastTelemetry) {
      updateTelemetry(lastTelemetry);
    }
  }, 1000);
});
