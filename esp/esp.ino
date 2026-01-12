#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <math.h>

// ===================== WiFi =====================
const char* WIFI_SSID = "aboshi";
const char* WIFI_PASS = "12345678";

// ===================== UART to Arduino (UNO الآن / MEGA لاحقاً) =====================
// ESP RX  = GPIO14 (D5)  <- UNO TX  (مهم: UNO->ESP مباشرة غالباً OK)
// ESP TX  = GPIO12 (D6)  -> UNO RX  (مهم: لا توصل 5V على RX للـ ESP بدون level shift)
#define ESP_RX_PIN 14
#define ESP_TX_PIN 12

const uint32_t UNO_BAUD = 9600;          // لازم يطابق esp.begin على UNO
SoftwareSerial unoSerial(ESP_RX_PIN, ESP_TX_PIN); // (RX, TX)

// ===================== HTTP Server =====================
ESP8266WebServer server(80);

// ===================== Telemetry cache =====================
String rxLine;
String lastLineFromUno;

float lastTemp = NAN;
float lastHum  = NAN;

unsigned long lastTelemetryMs = 0;

// ===================== Helpers =====================
static bool isValidTemp(float t) { return (t > -40.0f && t < 125.0f); }
static bool isValidHum(float h)  { return (h >= 0.0f && h <= 100.0f); }

// Parse line formats:
// 1) "Temp: 37.4 Humidity: 55"
// 2) "TEMP=37.4,HUM=55"
void parseTelemetryLine(const String& line) {
  // Format 1
  int tPos = line.indexOf("Temp:");
  int hPos = line.indexOf("Humidity:");
  if (tPos >= 0 && hPos >= 0) {
    String tStr = line.substring(tPos + 5, hPos);
    tStr.trim();
    String hStr = line.substring(hPos + 9);
    hStr.trim();

    float t = tStr.toFloat();
    float h = hStr.toFloat();

    if (isValidTemp(t)) lastTemp = t;
    if (isValidHum(h))  lastHum  = h;

    lastTelemetryMs = millis();
    return;
  }

  // Format 2
  int tp = line.indexOf("TEMP=");
  int hp = line.indexOf("HUM=");
  if (tp >= 0 && hp >= 0) {
    String tStr = line.substring(tp + 5);
    int comma = tStr.indexOf(',');
    if (comma >= 0) tStr = tStr.substring(0, comma);
    tStr.trim();

    String hStr = line.substring(hp + 4);
    hStr.trim();

    float t = tStr.toFloat();
    float h = hStr.toFloat();

    if (isValidTemp(t)) lastTemp = t;
    if (isValidHum(h))  lastHum  = h;

    lastTelemetryMs = millis();
    return;
  }
}

void onLineFromUno(const String& line) {
  lastLineFromUno = line;
  parseTelemetryLine(line);

  Serial.print("[UNO -> ESP] ");
  Serial.println(line);
}

void readUnoNonBlocking() {
  while (unoSerial.available()) {
    char c = (char)unoSerial.read();
    if (c == '\r') continue;

    if (c == '\n') {
      if (rxLine.length()) {
        onLineFromUno(rxLine);
        rxLine = "";
      }
    } else {
      if (rxLine.length() < 220) rxLine += c;
    }
  }
}

// ===================== ROUTES =====================

// POST /cmd  (body can be plain text OR JSON {"command":"..."} )
void handleCmd() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"no body\"}");
    return;
  }

  String body = server.arg("plain");
  body.trim();

  String cmd;

  if (body.startsWith("{")) {
    StaticJsonDocument<256> doc;
    auto err = deserializeJson(doc, body);
    if (!err && doc.containsKey("command")) {
      cmd = (const char*)doc["command"];
      cmd.trim();
    }
  }

  if (!cmd.length()) {
    cmd = body;
  }

  Serial.println("================================");
  Serial.print("[WEB -> ESP] CMD: ");
  Serial.println(cmd);
  Serial.println("================================");

  // Send ONLY when command received
  unoSerial.print(cmd);
  unoSerial.print('\n');

  StaticJsonDocument<256> out;
  out["ok"] = true;
  out["received"] = cmd;
  String resp;
  serializeJson(out, resp);
  server.send(200, "application/json", resp);
}

// GET /telemetry  (THIS is what your Node server is calling)
void handleTelemetry() {
  StaticJsonDocument<256> out;
  out["ok"] = true;

  bool hasT = !isnan(lastTemp);
  bool hasH = !isnan(lastHum);
  out["has_telemetry"] = (hasT && hasH);

  if (hasT) out["temperature"] = lastTemp;
  else      out["temperature"] = nullptr;

  if (hasH) out["humidity"] = lastHum;
  else      out["humidity"] = nullptr;

  uint32_t age = (lastTelemetryMs == 0) ? 0 : (uint32_t)(millis() - lastTelemetryMs);
  out["age_ms"] = age;

  String resp;
  serializeJson(out, resp);
  server.send(200, "application/json", resp);
}

// GET /status  (used by /api/ping-esp in Node)
void handleStatus() {
  StaticJsonDocument<256> out;
  out["ok"] = true;
  out["ip"] = WiFi.localIP().toString();
  out["rssi"] = WiFi.RSSI();

  out["uno_baud"] = (uint32_t)UNO_BAUD;
  out["last_from_uno"] = lastLineFromUno;

  String resp;
  serializeJson(out, resp);
  server.send(200, "application/json", resp);
}

void handleRoot() {
  server.send(200, "text/plain",
    "ESP8266 Bridge OK\n"
    "POST /cmd\n"
    "GET  /telemetry\n"
    "GET  /status\n"
  );
}

// ===================== Setup / Loop =====================
void setup() {
  Serial.begin(115200);
  delay(200);

  unoSerial.begin(UNO_BAUD);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("ESP8266 IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/telemetry", HTTP_GET, handleTelemetry);
  server.on("/cmd", HTTP_POST, handleCmd);

  server.begin();
  Serial.println("HTTP server started on port 80");
}

void loop() {
  server.handleClient();
  readUnoNonBlocking();
  yield();
  delay(1);
}
