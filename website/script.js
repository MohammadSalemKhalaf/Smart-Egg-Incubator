// WebSocket connection
let socket;
let isConnected = false;
let lastTelemetry = null;

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

  // Update UI
  document
    .getElementById("auto-mode")
    .classList.toggle("active", mode === "AUTO");
  document
    .getElementById("manual-mode")
    .classList.toggle("active", mode === "MANUAL");
  elements.modeStatus.textContent = mode;

  // Send command
  socket.emit("control-command", {
    type: "mode",
    value: mode,
  });

  addLog(`Mode changed to ${mode}`);
}

function setTemperature() {
  const temp = document.getElementById("temp-setpoint").value;
  elements.targetTemp.textContent = temp;

  socket.emit("control-command", {
    type: "temperature",
    value: parseFloat(temp),
  });

  addLog(`Target temperature set to ${temp}°C`);
}

function setHumidity() {
  const hum = document.getElementById("humidity-setpoint").value;
  elements.targetHumidity.textContent = hum;

  socket.emit("control-command", {
    type: "humidity",
    value: parseInt(hum),
  });

  addLog(`Target humidity set to ${hum}%`);
}

function setDay() {
  const day = document.getElementById("day-input").value;

  socket.emit("control-command", {
    type: "day",
    value: parseInt(day),
  });

  addLog(`Incubation day set to ${day}`);

  // Update RH based on day
  let targetRH;
  if (day <= 7) targetRH = 56;
  else if (day <= 18) targetRH = 60;
  else targetRH = 65;

  elements.targetHumidity.textContent = targetRH;
}

function setEggCount() {
  const eggs = document.getElementById("egg-count").value;

  socket.emit("control-command", {
    type: "egg_count",
    value: parseInt(eggs),
  });

  addLog(`Egg count set to ${eggs}`);
}

function toggleVentilation() {
  const enabled = document.getElementById("vent-fan-switch").checked;

  socket.emit("control-command", {
    type: "ventilation",
    value: enabled,
  });

  addLog(`Ventilation fan ${enabled ? "enabled" : "disabled"}`);
}

function toggleHeating() {
  const enabled = document.getElementById("heat-switch").checked;

  socket.emit("control-command", {
    type: "heating",
    value: enabled,
  });

  addLog(`Heating system ${enabled ? "enabled" : "disabled"}`);
}

function toggleHumiditySystem() {
  const enabled = document.getElementById("humidity-switch").checked;

  socket.emit("control-command", {
    type: "humidity_system",
    value: enabled,
  });

  addLog(`Humidity system ${enabled ? "enabled" : "disabled"}`);
}

function toggleAutoFlip() {
  const enabled = document.getElementById("flip-switch").checked;

  socket.emit("control-command", {
    type: "flip",
    value: enabled,
  });

  addLog(`Auto flip ${enabled ? "enabled" : "disabled"}`);
}

function startFlipSession() {
  socket.emit("control-command", {
    type: "start_flip_session",
  });

  addLog("Flip session started");
}

function stopFlipSession() {
  socket.emit("control-command", {
    type: "flip",
    value: false,
  });

  addLog("Flip session stopped");
}

function setFlipInterval() {
  const interval = document.getElementById("flip-interval").value;

  addLog(`Flip interval set to ${interval} hours`);
  // Note: You may need to add this command to your Arduino code
}

function setFlipDuration() {
  const duration = document.getElementById("flip-duration").value;

  addLog(`Flip duration set to ${duration} minutes`);
  // Note: You may need to add this command to your Arduino code
}

function controlWaterValve(open) {
  socket.emit("control-command", {
    type: "water_valve",
    value: open,
  });

  addLog(`Water valve ${open ? "opened" : "closed"}`);
}

function triggerVentilation() {
  // Trigger immediate ventilation
  socket.emit("control-command", {
    type: "ventilation",
    value: true,
  });

  setTimeout(() => {
    socket.emit("control-command", {
      type: "ventilation",
      value: false,
    });
  }, 5000);

  addLog("Manual ventilation triggered");
}

function testBuzzer() {
  // Test buzzer for 1 second
  socket.emit("control-command", {
    type: "buzzer_test",
    value: true,
  });

  setTimeout(() => {
    socket.emit("control-command", {
      type: "buzzer_test",
      value: false,
    });
  }, 1000);

  addLog("Buzzer test");
}

function silenceAlarm() {
  socket.emit("control-command", {
    type: "silence_alarm",
  });

  addLog("Alarm silenced");
}

function emergencyStop() {
  if (confirm("⚠️ ARE YOU SURE?\nThis will stop ALL systems immediately!")) {
    socket.emit("control-command", {
      type: "emergency_stop",
    });

    // Reset all switches
    document.querySelectorAll(".toggle-switch input").forEach((switchEl) => {
      switchEl.checked = false;
    });

    addLog("!!! EMERGENCY STOP ACTIVATED !!!");
    showToast("EMERGENCY STOP ACTIVATED", "error");
  }
}

function systemReset() {
  if (confirm("Reset all systems to default state?")) {
    socket.emit("control-command", {
      type: "reset",
    });

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

  // Update time every second
  setInterval(() => {
    if (lastTelemetry) {
      updateTelemetry(lastTelemetry);
    }
  }, 1000);
});
