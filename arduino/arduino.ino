#include <SoftwareSerial.h>

// UNO <-> ESP
SoftwareSerial esp(2, 3); // RX=D2 , TX=D3

// ===================== Modes =====================
enum RunMode { MODE_MANUAL, MODE_AUTO };
RunMode runMode = MODE_MANUAL;

// ===================== Buffers =====================
String cmdLine = "";

// ===================== Timers =====================
unsigned long lastTelemetryMs = 0;
unsigned long lastAutoTickMs  = 0;

// ===================== Fake sensors (testing now) =====================
float fakeTemp = 37.6;
int fakeHum = 55;

// ===================== Safe switching flags =====================
bool switchingMode = false;
unsigned long switchStartMs = 0;
RunMode pendingMode = MODE_MANUAL;

// timings (you can tune)
const unsigned long SWITCH_RESET_MS  = 300;   // time to apply safe reset
const unsigned long SWITCH_DELAY_MS  = 700;   // extra settle time
const unsigned long SWITCH_TOTAL_MS  = SWITCH_RESET_MS + SWITCH_DELAY_MS;

// ===================== Helpers =====================
void sendLineToEsp(const String &s) {
  esp.print(s);
  esp.print('\n');
}

void banner(const String &title) {
  Serial.println();
  Serial.println("================================");
  Serial.println(title);
  Serial.println("================================");
}

void logCmd(const String &c) {
  Serial.print("[ESP -> UNO] ");
  Serial.println(c);
}

// ===================== SAFE RESET (stub) =====================
// هنا بالمستقبل (على MEGA الحقيقي) بتطفي الريليهات/توقف الستيبّر/تقفل الصمام/الخ...
void safeResetOutputsStub(const char* reason) {
  banner(String("[RESET] Safe reset outputs  | Reason: ") + reason);

  // TODO real hardware actions later (MEGA):
  // digitalWrite(HEAT_RELAY, OFF);
  // digitalWrite(FAN_RELAY, OFF);
  // stop stepper, etc...

  Serial.println("-> All outputs set to SAFE-OFF (stub).");
}

// ===================== Mode switching =====================
void beginModeSwitch(RunMode newMode, const char* reason) {
  pendingMode = newMode;
  switchingMode = true;
  switchStartMs = millis();

  safeResetOutputsStub(reason);
}

// call in loop (non-blocking switch)
void updateModeSwitch() {
  if (!switchingMode) return;

  unsigned long elapsed = millis() - switchStartMs;

  if (elapsed < SWITCH_TOTAL_MS) {
    // during switch delay window, ignore other commands (except emergency/reset)
    return;
  }

  // finish switching
  runMode = pendingMode;
  switchingMode = false;

  banner(String("[MODE] Now running: ") + (runMode == MODE_AUTO ? "AUTO" : "MANUAL"));

  // notify ESP
  sendLineToEsp(String("MODE_OK:") + (runMode == MODE_AUTO ? "AUTO" : "MANUAL"));
}

// ===================== Stub actions =====================
void fnEmergencyStop() {
  banner("!!! [ACT] EMERGENCY_STOP !!!");
  safeResetOutputsStub("EMERGENCY_STOP");
}

void fnReset() {
  banner("[ACT] RESET");
  safeResetOutputsStub("RESET button");
}

void fnVent(bool on) {
  Serial.print("[MANUAL ACT] VENT = ");
  Serial.println(on ? "ON" : "OFF");
}

void fnHeat(bool on) {
  Serial.print("[MANUAL ACT] HEAT = ");
  Serial.println(on ? "ON" : "OFF");
}

void fnHumSys(bool on) {
  Serial.print("[MANUAL ACT] HUM_SYS = ");
  Serial.println(on ? "ON" : "OFF");
}

void fnFlip(bool on) {
  Serial.print("[MANUAL ACT] FLIP = ");
  Serial.println(on ? "ON" : "OFF");
}

void fnStartFlipSession() {
  Serial.println("[MANUAL ACT] START_FLIP_SESSION (stub)");
}

void fnStopFlipSession() {
  Serial.println("[MANUAL ACT] STOP_FLIP_SESSION (stub)");
}

void fnSetFlipIntervalHours(float h) {
  Serial.print("[MANUAL CFG] SET_FLIP_INTERVAL_H = ");
  Serial.println(h);
}

void fnSetFlipDurationMinutes(float m) {
  Serial.print("[MANUAL CFG] SET_FLIP_DURATION_M = ");
  Serial.println(m);
}

void fnTriggerVentNow() {
  Serial.println("[MANUAL ACT] TRIGGER_VENT_NOW (stub)");
}

void fnSetTemp(float t) {
  Serial.print("[CFG] SET_TEMP = ");
  Serial.println(t, 1);
}

void fnSetHum(int h) {
  Serial.print("[CFG] SET_HUM = ");
  Serial.println(h);
}

// ===================== Command parser =====================
// IMPORTANT: during switchingMode, we ignore normal commands
void handleCommand(const String &c) {
  logCmd(c);

  // always allow emergency/reset even during switching
  if (c == "EMERGENCY_STOP") { fnEmergencyStop(); return; }
  if (c == "RESET")          { fnReset(); return; }

  // if switching, ignore others
  if (switchingMode) {
    Serial.println("[SWITCH] Ignoring command during safe switch window...");
    return;
  }

  // mode command
  if (c.startsWith("MODE:")) {
    String v = c.substring(5);
    v.trim();
    if (v == "AUTO")  beginModeSwitch(MODE_AUTO, "MODE change");
    else              beginModeSwitch(MODE_MANUAL, "MODE change");
    return;
  }

  // Config commands (allowed in both modes, but in AUTO you can treat as settings)
  if (c.startsWith("SET_TEMP:")) { fnSetTemp(c.substring(9).toFloat()); return; }
  if (c.startsWith("SET_HUM:"))  { fnSetHum(c.substring(8).toInt()); return; }

  // If AUTO: ignore manual actuators
  if (runMode == MODE_AUTO) {
    Serial.println("[AUTO] Manual actuator command ignored.");
    return;
  }

  // MANUAL only actions
  if (c.startsWith("VENT:")) {
    String v = c.substring(5); v.trim();
    fnVent(v == "ON"); return;
  }
  if (c.startsWith("HEAT:")) {
    String v = c.substring(5); v.trim();
    fnHeat(v == "ON"); return;
  }
  if (c.startsWith("HUM_SYS:")) {
    String v = c.substring(8); v.trim();
    fnHumSys(v == "ON"); return;
  }
  if (c.startsWith("FLIP:")) {
    String v = c.substring(5); v.trim();
    fnFlip(v == "ON"); return;
  }

  if (c == "START_FLIP_SESSION") { fnStartFlipSession(); return; }
  if (c == "STOP_FLIP_SESSION")  { fnStopFlipSession(); return; }

  if (c.startsWith("SET_FLIP_INTERVAL_H:")) {
    fnSetFlipIntervalHours(c.substring(20).toFloat());
    return;
  }
  if (c.startsWith("SET_FLIP_DURATION_M:")) {
    fnSetFlipDurationMinutes(c.substring(20).toFloat());
    return;
  }

  if (c == "TRIGGER_VENT_NOW") { fnTriggerVentNow(); return; }

  Serial.println("[WARN] Unknown command");
}

void readFromEsp() {
  while (esp.available()) {
    char ch = (char)esp.read();
    if (ch == '\r') continue;

    if (ch == '\n') {
      if (cmdLine.length()) {
        String line = cmdLine;
        cmdLine = "";
        line.trim();
        if (line.length()) handleCommand(line);
      }
    } else {
      cmdLine += ch;
      if (cmdLine.length() > 200) cmdLine = "";
    }
  }
}

// ===================== Telemetry =====================
void updateFakeSensors() {
  // just to see numbers changing
  fakeTemp += 0.05;
  if (fakeTemp > 38.2) fakeTemp = 37.2;
  fakeHum = 58; // ثابتة
}

void sendTelemetry() {
  updateFakeSensors();
  String line = "Temp: " + String(fakeTemp, 1) + " Humidity: " + String(fakeHum);
  sendLineToEsp(line);
}

// ===================== AUTO LOOP (stub) =====================
void autoTick() {
  // هنا بالمستقبل (MEGA الحقيقي) تشتغل state machines
  Serial.println("[AUTO] running logic tick... (stub)");
}

// ===================== Setup/Loop =====================
void setup() {
  Serial.begin(9600);
  esp.begin(9600);

  banner("UNO READY - Dual Mode (MANUAL/AUTO) with Safe Switching");

  // handshake once (so ESP knows UNO alive)
  sendLineToEsp("UNO_READY");

  // start safe state
  safeResetOutputsStub("BOOT");
  runMode = MODE_MANUAL;
  banner("[MODE] Now running: MANUAL");
}

void loop() {
  readFromEsp();

  // handle safe switching non-blocking
  updateModeSwitch();

  // telemetry every 1s
  if (millis() - lastTelemetryMs >= 1000) {
    lastTelemetryMs = millis();
    sendTelemetry();
  }

  // auto loop tick every 2s (only in AUTO and not switching)
  if (!switchingMode && runMode == MODE_AUTO) {
    if (millis() - lastAutoTickMs >= 2000) {
      lastAutoTickMs = millis();
      autoTick();
    }
  }
}
