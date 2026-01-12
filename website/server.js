const express = require("express");
const http = require("http");
const socketIo = require("socket.io");
const cors = require("cors");

// node-fetch v3 is ESM, so in commonjs we use dynamic import
const fetchFn = (...args) =>
  import("node-fetch").then(({ default: fetch }) => fetch(...args));

const app = express();
const server = http.createServer(app);
const io = socketIo(server, {
  cors: { origin: "*", methods: ["GET", "POST"] },
});

app.use(cors());
app.use(express.json());
app.use(express.static(".")); // serves index.html, script.js, style.css

// =============== ESP CONFIG ===============
const ESP_BASE_URL = "http://10.173.204.153"; // IP تبع ESP
const TELEMETRY_POLL_MS = 1000;
const HTTP_TIMEOUT_MS = 2000;

let deviceConnected = false;
let lastTelemetryCache = {
  temperature: null,
  humidity: null,
  age_ms: null,
  timestamp: null,
};

// =============== helpers ===============
function withTimeout(ms) {
  const controller = new AbortController();
  const t = setTimeout(() => controller.abort(), ms);
  return { controller, clear: () => clearTimeout(t) };
}

async function sendToEsp(command) {
  const url = `${ESP_BASE_URL}/cmd`;
  const { controller, clear } = withTimeout(HTTP_TIMEOUT_MS);

  try {
    const r = await fetchFn(url, {
      method: "POST",
      headers: { "Content-Type": "text/plain" },
      body: command,
      signal: controller.signal,
    });

    const text = await r.text().catch(() => "");
    if (!r.ok) throw new Error(`ESP /cmd failed: ${r.status} ${text}`);

    try {
      return JSON.parse(text);
    } catch {
      return { ok: true, raw: text };
    }
  } finally {
    clear();
  }
}

async function getTelemetryFromEsp() {
  const url = `${ESP_BASE_URL}/telemetry`;
  const { controller, clear } = withTimeout(HTTP_TIMEOUT_MS);

  try {
    const r = await fetchFn(url, { signal: controller.signal });
    const text = await r.text().catch(() => "");
    if (!r.ok) throw new Error(`ESP /telemetry failed: ${r.status} ${text}`);
    return JSON.parse(text);
  } finally {
    clear();
  }
}

function mapControlToCommand({ type, value }) {
  switch (type) {
    case "mode":
      return `MODE:${value}`; // AUTO / MANUAL

    case "temperature":
      return `SET_TEMP:${value}`; // 37.6

    case "humidity":
      return `SET_HUM:${value}`; // 56

    case "ventilation":
      return `VENT:${value ? "ON" : "OFF"}`;

    case "heating":
      return `HEAT:${value ? "ON" : "OFF"}`;

    case "humidity_system":
      return `HUM_SYS:${value ? "ON" : "OFF"}`;

    case "flip":
      return `FLIP:${value ? "ON" : "OFF"}`;

    case "start_flip_session":
      return "START_FLIP_SESSION";

    case "stop_flip_session":
      return "STOP_FLIP_SESSION";

    case "flip_interval_hours":
      return `SET_FLIP_INTERVAL_H:${value}`;

    case "flip_duration_minutes":
      return `SET_FLIP_DURATION_M:${value}`;

    case "trigger_vent_now":
      return "TRIGGER_VENT_NOW";

    case "water_valve":
      return `WATER_VALVE:${value ? "OPEN" : "CLOSE"}`;

    case "day":
      return `SET_DAY:${value}`;

    case "egg_count":
      return `SET_EGGS:${value}`;

    case "reset":
      return "RESET";

    case "buzzer_test":
      return `BUZZER:${value ? "ON" : "OFF"}`;

    case "silence_alarm":
      return "SILENCE_ALARM";

    case "emergency_stop":
      return "EMERGENCY_STOP";

    default:
      return null;
  }
}

// http://localhost:3000/api/ping-esp
app.get("/api/ping-esp", async (req, res) => {
  try {
    const stUrl = `${ESP_BASE_URL}/status`;
    const { controller, clear } = withTimeout(HTTP_TIMEOUT_MS);

    const r = await fetchFn(stUrl, { signal: controller.signal });
    const text = await r.text().catch(() => "");
    clear();

    res.status(r.status).send(text || "OK");
  } catch (e) {
    res.status(500).json({ ok: false, error: e.message });
  }
});

// optional: show last telemetry cache
app.get("/api/telemetry", (req, res) => {
  res.json({ ok: true, ...lastTelemetryCache });
});

// =============== telemetry polling ===============
async function pollTelemetry() {
  try {
    const t = await getTelemetryFromEsp();

    if (!deviceConnected) {
      deviceConnected = true;
      io.emit("arduino-status", { connected: true });
      console.log("[ESP] connected");
    }

    const temp = t.temperature == null ? null : Number(t.temperature);
    const hum = t.humidity == null ? null : Number(t.humidity);

    const payload = {
      temperature: Number.isFinite(temp) ? temp : null,
      humidity: Number.isFinite(hum) ? hum : null,
      age_ms: t.age_ms == null ? null : Number(t.age_ms),
      timestamp: new Date().toISOString(),
    };

    lastTelemetryCache = payload;
    io.emit("telemetry", payload);
  } catch (err) {
    if (deviceConnected) {
      deviceConnected = false;
      io.emit("arduino-status", { connected: false });
      console.log("[ESP] disconnected:", err.message);
    }
  }
}

setInterval(pollTelemetry, TELEMETRY_POLL_MS);
pollTelemetry();

// =============== socket.io ===============
io.on("connection", (socket) => {
  console.log("Client connected:", socket.id);
  socket.emit("arduino-status", { connected: deviceConnected });

  if (lastTelemetryCache?.timestamp) {
    socket.emit("telemetry", lastTelemetryCache);
  }

  socket.on("control-command", async (data) => {
    const command = mapControlToCommand(data);

    if (!command) {
      socket.emit("command-status", {
        success: false,
        command: "",
        error: `Unknown command type: ${data?.type}`,
      });
      return;
    }

    try {
      const resp = await sendToEsp(command);
      socket.emit("command-status", { success: true, command, resp });
    } catch (err) {
      socket.emit("command-status", {
        success: false,
        command,
        error: err.message,
      });
    }
  });
});

// =============== start ===============
const PORT = process.env.PORT || 3000;
server.listen(PORT, "0.0.0.0", () => {
  console.log(`Server running on http://localhost:${PORT}`);
  console.log(`ESP base URL: ${ESP_BASE_URL}`);
  console.log(`Try: http://localhost:${PORT}/api/ping-esp`);
  console.log(`Try: http://localhost:${PORT}/api/telemetry`);
});
