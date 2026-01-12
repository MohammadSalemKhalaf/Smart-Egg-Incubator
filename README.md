# 🥚 Smart Incubator IoT Control System  
Web → Node.js → ESP8266 → Arduino

This project is a hardware-based IoT system developed as part of a **Computer Engineering** project at **An-Najah National University**.  
It demonstrates how web technologies can be integrated with embedded systems to build a **real-time, safe, and interactive incubator control platform**.

The system allows monitoring and controlling an incubator through a web dashboard accessible from both PC and mobile devices.

---

## 🌐 System Architecture

Web Dashboard (HTML / JavaScript)  
→ Node.js Server (Express + Socket.io)  
→ ESP8266 Bridge (HTTP + UART)  
→ Arduino  
- Arduino UNO (testing & simulation)  
- Arduino MEGA (real incubator hardware)

---

## ✨ Features

- Real-time monitoring:
  - Temperature
  - Humidity
- Web-based control:
  - AUTO / MANUAL modes
  - Heating system control
  - Ventilation control
  - Humidity system control
  - Egg flipping controls
  - Water valve control
- Safety-first design:
  - Emergency Stop (always active)
  - System Reset (always active)
- Mobile-friendly dashboard
- Safe testing mode (no real hardware activation on Arduino UNO)

---

## 🧱 Project Structure

project/  
├─ server.js          – Node.js backend server  
├─ index.html         – Web dashboard UI  
├─ script.js          – Front-end logic (Socket.io + controls)  
├─ style.css          – Dashboard styling  
├─ esp8266.ino        – ESP8266 bridge (HTTP + UART)  
├─ arduino_uno.ino    – Arduino UNO test code (safe stub)  
└─ README.md          – Project documentation  

---

## 🔧 System Operation

### Telemetry Flow
- Arduino sends serial data in the format:  
  Temp: 37.6 Humidity: 55
- ESP8266 reads and parses the data.
- Telemetry is exposed through:  
  GET /telemetry
- Node.js fetches telemetry and sends it live to the web dashboard.
- The dashboard updates readings in real time.

### Command Flow
- User presses a button on the web dashboard.
- Command is sent to Node.js via Socket.io.
- Node.js forwards the command to ESP8266.
- ESP8266 sends the command to Arduino via UART.
- Arduino handles the command (currently prints only in test mode).

---

## ⚙️ Requirements

### Software
- Node.js (LTS recommended)
- Arduino IDE
- ESP8266 Board Package installed

### Hardware (Testing Phase)
- ESP8266 NodeMCU
- Arduino UNO
- Jumper wires
- Common GND between ESP and Arduino
- Voltage divider recommended (Arduino TX → ESP RX)

---

## 🔌 Wiring (Arduino UNO ↔ ESP8266)

Arduino UNO  
- D2 → RX (from ESP)  
- D3 → TX (to ESP)  

ESP8266  
- GPIO14 (D5) ← RX  
- GPIO12 (D6) → TX  

ESP8266 operates on **3.3V logic only**.

---

## 🚀 Installation & Running the Project

1. Start the server (server is started ONLY using):
npm start

The server will run on:
http://localhost:3000

---

## 📱 Accessing the Dashboard on Mobile

- Phone and PC must be on the same Wi-Fi network.
- Find the PC IP address using:
ipconfig
- Open on phone browser:
http://PC_IP:3000

(localhost works only on the same device)

---

## 🌐 ESP8266 HTTP Endpoints

GET /status  
GET /telemetry  
POST /cmd  

Example telemetry response:
{
  "ok": true,
  "temperature": 37.6,
  "humidity": 55
}

---


## 🔄 Arduino MEGA Integration

This project is already integrated with the **real Arduino MEGA hardware**.

- The MEGA runs the full incubator control logic.
- All functions interact with real components (sensors, relays, motors, etc.).
- The web dashboard, Node.js server, and ESP8266 bridge are fully aligned with the MEGA firmware.

---

## 🛑 Safety

- Emergency Stop and Reset work in both MANUAL and AUTO modes.
- Switching between modes performs a safe reset with a short delay to prevent hardware conflicts.

---

## 🎓 Academic Context

University: An-Najah National University  
Department: Computer Engineering  
Project Type: Hardware & IoT System  
Focus Areas:
- Embedded Systems
- Internet of Things (IoT)
- Real-Time Monitoring
- Safety-Critical Design

---

## 📌 Final Notes

This project demonstrates a modern approach to combining software and hardware into a scalable IoT solution.  
It is designed to be educational, extendable, and suitable for academic evaluation and demonstrations.
