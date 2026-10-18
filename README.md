# EarnApp Monitor - ESP32-2432S028R (CYD)

Monitor hardware în timp real pentru EarnApp, construit pe placa ESP32-2432S028R CYD (Cheap Yellow Display) cu display ILI9341 + Touch XPT2046.

Proiectul afișează balanța, totalul câștigat, device-uri online/offline și statusul plăților direct pe ecranul TFT și printr-o interfață Web modernă, fără aplicații externe.

![Platform](https://img.shields.io/badge/Platform-ESP32-blue) ![Display](https://img.shields.io/badge/Display-ILI9341%20320x240-orange) ![License](https://img.shields.io/badge/License-MIT-green)

---

### 📸 Preview

**Ecran Principal TFT:**
- Card `TOTAL EVER` - total câștigat vreodată
- Card `BALANCE / PENDING` - balanța curentă + pending + calcul până la payout-ul de $10
- Bara de jos: `TIME` (NTP) | `DEVICES` (online) | `PAYMENTS`

**Pagina System Info (TFT):**
- CPU, Flash, Heap, Uptime, Temp CPU REALĂ (senzor intern `temprature_sens_read()`)
- IP, RSSI WiFi
- Detalii EarnApp + lista completă device-uri cu status ONLINE/OFFLINE real

**Interfață Web:**
- `http://<IP-ESP>/` - Dashboard stil EarnApp cu auto-refresh 1 sec
- `http://<IP-ESP>/info` - System Info complet cu device-uri
- `http://<IP-ESP>/api/data` - JSON pentru dashboard
- `http://<IP-ESP>/api/system` - JSON pentru system info
- `http://<IP-ESP>/reset` - Reset ESP

---

### 🚀 Capabilități Hardware

**Placa:** ESP32-2432S028R (CYD)
- **Display:** ILI9341 320x240, SPI: SCLK 14, MISO 12, MOSI 13, CS 15, DC 2, RST -1
- **Touch:** XPT2046 pe HSPI separat: CLK 25, MISO 39, MOSI 32, CS 33, IRQ 36
- **Backlight Auto-Adaptiv:** Pin 21 control PWM + LDR pe pin 34 (ADC)
- **Task dedicat `backlightTask`:** rulează pe Core 1, citire LDR mediată pe 5 eșantioane, filtrare `smoothBrightness` cu `SMOOTH_FACTOR 0.03`, auto-calibrare `ldrMin/ldrMax`

---

### 🧠 Funcții Software Principale

#### 1. Conectivitate & Timp
- `WiFi.begin()` cu retry 40x500ms, afișare eroare pe TFT
- Sincronizare NTP `pool.ntp.org` + `time.nist.gov`
- Timezone România `EET-2EEST,M3.5.0/3,M10.5.0/4` cu `configTime()` și `tzset()`
- Fallback ceas pe `millis()` dacă NTP nu e sincronizat

#### 2. Fetch Date EarnApp REAL - Fără date false
Funcția `fetchEarnAppData()` face 3 tipuri de cereri cu `WiFiClientSecure` + `setInsecure()`:
- **a) `/api/money/?appid=earnapp_dashboard`:** scoate `balance`, `earnings_total`, calculează `g_pending`, `g_total_paid`
- **b) `/api/devices/?appid=earnapp_dashboard`:** parsează array-ul de device-uri. Titlu din `appid_title | title | name | uuid`
- **c) Fallback-uri inteligente dacă (b) e gol:**
    - `/api/user_data/`
    - `/api/device_statuses?appid=earnapp&version=1.676.514` (tratează atât JsonArray cât și JsonObject)

**Funcția `isRealOnline(JsonObject obj)`:** Detectare ONLINE/OFFLINE REALĂ, nu presupusă:
Verifică în ordine: `online`, `is_online`, `connected`, `is_active`, `status`/`state` ca string (online/active/connected/running/ok vs offline/inactive/disabled/disconnected)

**Anti-fake:** A fost eliminat bug-ul clasic `d["rate"] = $5.00` - se folosesc doar câmpuri reale `earned`, `earned_total`.

#### 3. Afișare TFT
- `drawUIStaticElements()` - desenează scheletul UI cu carduri rotunjite și culori RGB565 custom
- `updateBottomInfoDisplay()` - update devices & payments
- `updateTimeOnly()` - update doar ceas, fără flicker
- Paletă custom: `COLOR_BG`, `COLOR_CARD_BG`, `COLOR_TITLE`, `COLOR_ACCENT`, etc.

#### 4. Touch & Calibrare
- `readTouchPoint()` - filtrare presiune `z < 200`, mapare `map(pt.x, TS_MINX, TS_MAXX, 0, 320)`
- `calibrareTouch()` - calibrare în 4 colțuri (10,10), (310,10), (10,230), (310,230), salvare în EEPROM cu flag `1234`
- `handleTouch()` - debounce 400ms, click pe buton Info `i` -> `afiseazaSystemInfo()`, click oriunde pe carduri -> refresh forțat API

#### 5. System Info & Reset de Siguranță
- `afiseazaSystemInfo()` - pagină dedicată (paginaCurenta = 2) care rulează max 25 sec
- Buton Reset Roșu `R` - desenat prin `desenButonResetTFT()`
- **Mecanism anti-reset accidental:** Trebuie să ȚII APĂSAT 3 secunde (`RESET_HOLD_TIME 3000`) cu bară de progres și countdown. Dacă iei degetul, se anulează.
- Afișare: CPU Freq, Flash Size, Heap Free, Uptime, Temp CPU reală via `getCpuTemp()`, IP, RSSI, detalii EarnApp

#### 6. WebServer
Funcția `setupWebServer()`:
- `/` - servește `HTML_MAIN` din PROGMEM
- `/info` - servește `HTML_INFO`
- `/reset` - pagină de confirmare + `ESP.restart()` după 1.5s
- `/api/data` și `/api/system` - JSON live cu `ArduinoJson`

#### 7. Temperatura CPU REALĂ
- `getCpuTemp()` folosește senzorul intern ESP32 `temprature_sens_read()` (valoare în Fahrenheit convertită în Celsius). Nu folosește estimare pe heap.

---

### 📦 Librării Necesare
Instalează din Library Manager:
- `WiFi`, `HTTPClient`, `WiFiClientSecure`, `EEPROM`, `SPI`, `time` (incluse în ESP32 Core)
- `ArduinoJson` by Benoit Blanchon (v7+)
- `Adafruit GFX` + `Adafruit ILI9341`
- `XPT2046_Touchscreen` by Paul Stoffregen

### ⚙️ Instalare
1. Pune SSID / parola WiFi la `ssid` / `password`
2. Loghează-te pe earnapp.com -> F12 -> Application -> Cookies -> copiază `oauth-token`, `oauth-refresh-token`, `xsrf-token`, `XSRF-TOKEN` în variabilele `earn_oauth_token` etc.
3. Selectează Board: `ESP32 Dev Module`, Partition: `Default`
4. Uploadează. La prima pornire atinge ecranul pentru calibrare.

### 🔒 Securitate
NU posta token-urile reale pe 【entity-GitHub¦canonical_name=GitHub】. Folosește `secrets.h` sau variabile de mediu.

Licență: 【entity-MIT¦canonical_name=MIT】 - Copyright (c) 2026 adisorin


-------------------------


# EarnApp Monitor - ESP32-2432S028R (CYD)

Real-time hardware monitor for EarnApp, built on ESP32-2432S028R CYD (Cheap Yellow Display) with ILI9341 display + XPT2046 touch.

The project displays balance, total earnings, online/offline devices and payment status directly on the TFT and via a modern Web Interface, no external app needed.

---

### 📸 Preview

**Main TFT Screen:**
- `TOTAL EVER` card - lifetime earnings
- `BALANCE / PENDING` card - current balance + pending + calculation to $10 payout threshold
- Bottom bar: `TIME` (NTP) | `DEVICES` (online) | `PAYMENTS`

**System Info Page (TFT):**
- CPU, Flash, Heap, Uptime, REAL CPU Temp (internal sensor `temprature_sens_read()`)
- IP, WiFi RSSI
- EarnApp details + full device list with REAL ONLINE/OFFLINE status

**Web Interface:**
- `http://<IP-ESP>/` - EarnApp-styled dashboard with 1s auto-refresh
- `http://<IP-ESP>/info` - Full System Info with devices
- Endpoints: `/api/data`, `/api/system`, `/reset`

---

### 🚀 Hardware Capabilities

**Board:** ESP32-2432S028R (CYD)
- **Display:** ILI9341 320x240, SPI: SCLK 14, MISO 12, MOSI 13, CS 15, DC 2
- **Touch:** XPT2046 on dedicated HSPI: CLK 25, MISO 39, MOSI 32, CS 33, IRQ 36
- **Auto Adaptive Backlight:** Pin 21 PWM control + LDR on pin 34 (ADC)
- **Dedicated `backlightTask`:** Pinned to Core 1, LDR reading averaged over 5 samples, smoothing with `SMOOTH_FACTOR 0.03`, auto-calibration of `ldrMin/ldrMax`

---

### 🧠 Core Software Functions

#### 1. Connectivity & Time
- `WiFi.begin()` with 40x500ms retry, error display on TFT
- NTP sync `pool.ntp.org`
- Timezone Romania `EET-2EEST` via `configTime()` + `tzset()`
- Fallback clock using `millis()` if NTP not synced

#### 2. REAL EarnApp Data Fetch - No Fake Fallbacks
`fetchEarnAppData()` does 3 request types with `WiFiClientSecure`:
- **a) `/api/money/`:** gets `balance`, `earnings_total`, calculates pending/paid
- **b) `/api/devices/`:** parses device array, title from `appid_title | title | name | uuid`
- **c) Smart Fallbacks if (b) is empty:**
    - `/api/user_data/`
    - `/api/device_statuses`

**`isRealOnline()` function:** REAL online detection:
Checks `online`, `is_online`, `connected`, `is_active`, string `status`/`state` (online/active/connected vs offline/inactive/disabled). Returns true only if not explicitly marked offline.

**Anti-fake fix:** Removed classic `$5.00 rate` bug - uses only real fields `earned`, `earned_total`.

#### 3. TFT Rendering
- `drawUIStaticElements()` - draws UI skeleton with rounded cards and custom RGB565 palette
- `updateBottomInfoDisplay()` / `updateTimeOnly()` - partial refresh without flicker

#### 4. Touch & Calibration
- `readTouchPoint()` - pressure filter `z < 200`, coordinate mapping
- `calibrareTouch()` - 4-point calibration, save to EEPROM with flag `1234`
- `handleTouch()` - 400ms debounce, Info `i` button -> `afiseazaSystemInfo()`, tap anywhere -> force API refresh

#### 5. System Info & Safe Reset
- `afiseazaSystemInfo()` - dedicated page (paginaCurenta = 2) running max 25s
- Red Reset `R` button via `desenButonResetTFT()`
- **Safe Reset Mechanism:** You must HOLD for 3 seconds (`RESET_HOLD_TIME`) with progress bar and countdown. If you release early, it cancels.

#### 6. WebServer
`setupWebServer()` serves PROGMEM HTML and live JSON APIs.

#### 7. REAL CPU Temperature
- `getCpuTemp()` uses internal ESP32 sensor `temprature_sens_read()` (Fahrenheit -> Celsius). Not heap estimation.

---

### 📦 Required Libraries
- `ArduinoJson` (v7+), `Adafruit GFX`, `Adafruit ILI9341`, `XPT2046_Touchscreen`

### ⚙️ Setup
1. Set SSID/password
2. Login to earnapp.com -> F12 -> Application -> Cookies -> copy tokens to `earn_oauth_token` variables
3. Board: `ESP32 Dev Module`
4. Upload. On first boot, touch screen to calibrate.

### 🔒 Security
DO NOT push real tokens to GitHub. Use `secrets.h`.

License: MIT - Copyright (c) 2026 adisorin

### 🔒 Securitate
NU posta token-urile reale pe 【entity-GitHub¦canonical_name=GitHub】. Folosește `secrets.h` sau variabile de mediu.

Licență: 【entity-MIT¦canonical_name=MIT】 - Copyright (c) 2026 adisorin
