
# 🥚 Smart Egg Incubator System

A fully automated Arduino-based egg incubator featuring temperature & humidity control, automatic egg turning, ventilation cycles, EEPROM settings storage, and a full LCD + keypad interface.

---

## 🚀 Features
- Precise temperature control with hysteresis (±0.5°C)
- Humidity monitoring & display
- Automatic egg turning every 2 hours (NEMA 23)
- Ventilation system with servo air‑flap
- 20x4 LCD user interface
- Full configuration via keypad
- Real‑time countdown of incubation days
- EEPROM storage for all settings
- Safety shutdown on sensor errors

---

## 🧰 Hardware Components

| Component | Quantity | Notes |
|----------|----------|--------|
| Arduino Mega 2560 | 1 | Main controller |
| LCD 20x4 I2C | 1 | Display interface |
| 4x4 Matrix Keypad | 1 | User input |
| DHT Sensor | 1 | Temperature & humidity |
| NEMA 23 Stepper Motor | 1 | Egg turning |
| Stepper Driver | 1 | Controls NEMA 23 |
| Servo Motor | 1 | Vent flap |
| 12V Fans | 3 | Air circulation |
| Exhaust Fan | 1 | Periodic ventilation |
| 4‑Channel Relay Module | 1 | Heater + fans |
| Heat Lamps (220V) | 2 | Incubation heating |

---

## 🔌 Wiring Overview

```
DHT Sensor:     DATA → Pin 3
LCD 20x4:       SDA → 21, SCL → 20
Keypad:         Rows → 22-25, Cols → 26-29
Servo:          Signal → Pin 2
Stepper Driver: PUL → 10 | DIR → 11 | EN → 8
Relays:         Fans → 31 | Heater → 32 | Exhaust → 33
```

---

## 📥 Installation

### 1️⃣ Install Required Libraries (Arduino IDE → Library Manager)
- LiquidCrystal_I2C  
- Keypad  
- DHT Sensor Library  
- Servo  

### 2️⃣ Upload Firmware
1. Open **Smart_Egg_Incubator.ino**
2. Select board: **Arduino Mega 2560**
3. Select correct **COM port**
4. Click **Upload**

---

## 🎮 Usage

### 🔹 Display Mode
Shows:
- Current & target temperature  
- Current & target humidity  
- Day counter  
- Heater/fan/exhaust status  
- Stepper status  

Press `*` to enter settings.

### 🔹 Configuration Mode
| Key | Action |
|-----|--------|
| 0-9 | Enter values |
| A | Add decimal point |
| C | Clear input |
| # | Save / Next |
| * | Cancel |

Flow:
```
* → Temperature → # → Humidity → # → Days → # → Done
```

---

## ⚙️ Default Settings

| Setting | Value |
|--------|--------|
| Temperature | 37.5°C |
| Humidity | 55% |
| Incubation Days | 21 |

---

## 🛡 Safety
- Heater auto‑off on DHT sensor failure  
- EEPROM stores settings permanently  
- Relay hysteresis protects components  

---

## 📄 License
MIT License – Free to use and modify.

---

## 🎓 Graduation Project
Developed as part of a **Computer Engineering** graduation project.
