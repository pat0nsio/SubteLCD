# SubteLCD

![Arduino](https://img.shields.io/badge/Arduino-ESP32-00979D?logo=arduino&logoColor=white)
![C++](https://img.shields.io/badge/C++-Sketch-00599C?logo=cplusplus&logoColor=white)
![Python](https://img.shields.io/badge/Python-prototype-3776AB?logo=python&logoColor=white)
![API](https://img.shields.io/badge/API-BA_Transporte_GCBA-003087)

Arduino sketch for an ESP32 that replicates the arrival time displays found in Buenos Aires subway stations. It queries the GCBA public transport API in real time and shows the next two incoming trains — line, station, direction, and a live countdown — on a 20×4 LCD screen.

---

## How it works

On boot, the ESP32 connects to WiFi and queries the BA Transport GTFS API (`/subtes/forecastGTFS`). It filters by target line, station, and direction, extracts the arrival timestamps, sorts the candidates, and displays the two nearest trains with a countdown.

The API is re-queried every 30 seconds. Between queries, the display refreshes every second by decrementing the countdown locally using `millis()`, without making additional network requests.

```
┌────────────────────┐
│ Linea D - Bulnes   │  ← line + station (row 0)
│ Sentido: Catedral  │  ← direction label  (row 1)
│ 1) 2 min 15 s      │  ← next train       (row 2)
│ 2) 6 min 48 s      │  ← second train     (row 3)
└────────────────────┘
```

---

## Hardware

| Component | Details |
|---|---|
| Microcontroller | ESP32 (WiFi + HTTPS support) |
| Display | LCD 2004 (20 columns × 4 rows), parallel interface, 4-bit mode |
| Connection | Direct wiring — no I2C backpack |

### LCD wiring (ESP32 → LCD)

| LCD pin | ESP32 GPIO |
|---|---|
| RS | 22 |
| E | 23 |
| D4 | 5 |
| D5 | 18 |
| D6 | 19 |
| D7 | 21 |

---

## Dependencies

Install via Arduino Library Manager:

- `ArduinoJson` — JSON parsing with filter support (memory-efficient on ESP32)
- `LiquidCrystal` — parallel LCD driver
- `WiFi`, `WiFiClientSecure`, `HTTPClient` — built into the ESP32 Arduino core

---

## Configuration

Edit the constants at the top of `SubteLCD.ino` before flashing:

```cpp
const char* WIFI_SSID          = "TU_WIFI";
const char* WIFI_PASSWORD      = "TU_PASSWORD";

const char* CLIENT_ID          = "your_client_id";
const char* CLIENT_SECRET      = "your_client_secret";

// Target station
const char* OBJETIVO_LINEA     = "LineaD";   // LineaA, LineaB, LineaC, LineaD, LineaE, LineaH
const char* OBJETIVO_ESTACION  = "Bulnes";   // station name as returned by the API
const int   OBJETIVO_DIRECCION = 1;          // 1 = toward one terminal, 0 = toward the other
const char* TEXTO_SENTIDO      = "Catedral"; // display label for the direction
```

API credentials are free — register at [BA Transporte](https://www.buenosaires.gob.ar/desarrollourbano/transporte/apitransporte).

---

## Flashing

1. Open `SubteLCD.ino` in the Arduino IDE (or PlatformIO)
2. Select board: **ESP32 Dev Module**
3. Fill in WiFi and API credentials
4. Upload

The LCD will show a connecting screen during boot and switch to live data once the first API response is received.

---

## API

This project uses the GCBA Transport API:

```
GET https://apitransporte.buenosaires.gob.ar/subtes/forecastGTFS
    ?client_id=CLIENT_ID&client_secret=CLIENT_SECRET
```

The sketch uses `ArduinoJson`'s filter feature to deserialize only the relevant fields (`Header.timestamp`, `Linea.Route_Id`, `Linea.Direction_ID`, `Estaciones.stop_name`, `Estaciones.arrival.time`), keeping heap usage low on the ESP32.

---

## Project structure

```
SubteLCD/
├── SubteLCD.ino   # ESP32 sketch — main implementation
├── main.py        # Python prototype — early exploration of the API response
└── .gitignore     # excludes .env (API credentials)
```


**Author:** [pat0nsio](https://github.com/pat0nsio)
