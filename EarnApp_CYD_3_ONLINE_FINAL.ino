/*【entity-MIT¦canonical_name=MIT】 License

Copyright (c) 2026 adisorin

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.*/

// --- Librarii necesare pentru WiFi, HTTP, JSON, TFT, Touch, EEPROM, timp si WebServer ---
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <EEPROM.h>
#include <time.h>
#include <WebServer.h>

// --- Configurare Wi-Fi ---
const char* ssid = ".............. ";
const char* password = "................";

// DUPA CE TE RE-LOGHEZI, pune tokenurile noi aici - NU posta tokenuri reale pe net

// AFTER YOU LOG BACK IN, put the new tokens here - DO NOT post real tokens online
const char* earn_oauth_token = "....................";
const char* earn_refresh_token = "..................";
const char* earn_xsrf_token = ".....................";
const char* earn_xsrf_token2 = "....................";

// --- Variabile globale pentru System Info - EARNAPP ---
String g_currentIdentity = "EarnApp"; // Identitate curenta afisata
double g_balance = 0.0; // Balanta curenta
double g_total_ever = 0.0; // Total castigat vreodata
double g_pending = 0.0; // Suma in asteptare
double g_total_paid = 0.0; // Total platit
int g_device_count = 0; // Numar device-uri online
int g_payments_count = 0; // Numar plati
int g_devices_found = 0; // Numar device-uri gasite de API
// Structura pentru un device EarnApp
struct EarnDevice { String title; 
float earned; 
float earned_total; 
bool online; };
EarnDevice g_devices[10]; // Array maxim 10 device-uri
float g_temp_cpu; // Temperatura CPU initiala
bool g_timeSynced = false; // Flag sincronizare timp NTP
WebServer server(80); // Server web pe portul 80

// --- Pini specifici pentru ESP32-2432S028 (CYD) ---
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define PIN_BACKLIGHT 21 // Pin control backlight
#define PIN_LDR 34 // Pin senzor lumina LDR
#define ADC_SAMPLES 5 // Numar esantioane pentru mediere LDR
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
SPIClass touchscreenSPI(HSPI); // SPI separat pentru touchscreen
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ); // Obiect touchscreen
Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST); // Obiect display TFT

// --- Paletă de Culori personalizată (RGB565) ---
#define COLOR_BG 0x0000 // Negru fundal
#define COLOR_CARD_BG 0x1082 // Fundal carduri
#define COLOR_TITLE 0x07FF // Cyan titlu
#define COLOR_TEXT 0xFFFF // Alb text
#define COLOR_LABEL 0xCE79 // Gri label
#define COLOR_STATUS_OK 0x07E0 // Verde status OK
#define COLOR_STATUS_ERR 0xF800 // Rosu eroare
#define COLOR_VALUE_MYST 0xFFE0 // Galben valori
#define COLOR_ACCENT 0xFD20 // Portocaliu accent

unsigned long lastMillis = 0; // Ultimul timp de fetch API
unsigned long lastTimeUpdateMillis = 0; // Ultimul timp de update ceas
const long interval = 120000; // Interval API 2 minute
const long ldrInterval = 50; // Interval citire LDR 50ms
float smoothBrightness = 5.0; // Valoare filtrata pentru luminozitate
const float SMOOTH_FACTOR = 0.03; // Factor de netezire backlight
volatile int ldrMin = 4095; // Minim LDR detectat
volatile int ldrMax = 0; // Maxim LDR detectat
int TS_MINX = 200, TS_MAXX = 3800; // Calibrare touch X
int TS_MINY = 200, TS_MAXY = 3800; // Calibrare touch Y
bool calibrated = false; // Flag daca touch e calibrat
#define EEPROM_SIZE 64 // Dimensiune EEPROM
#define ADDR_FLAG 0 // Adresa flag calibrare
#define ADDR_MINX 4 // Adresa MINX in EEPROM
#define ADDR_MAXX 8 // Adresa MAXX
#define ADDR_MINY 12 // Adresa MINY
#define ADDR_MAXY 16 // Adresa MAXY
#define SYSINFO_BTN_X 290 // Pozitie buton info X
#define SYSINFO_BTN_Y 4 // Pozitie buton info Y
#define SYSINFO_BTN_W 26 // Latime buton info
#define SYSINFO_BTN_H 26 // Inaltime buton info
#define RESET_BTN_X 288 // Pozitie buton reset X
#define RESET_BTN_Y 18 // Pozitie buton reset Y
#define RESET_BTN_R 12 // Raza buton reset
#define RESET_HOLD_TIME 3000 // 3 secunde // Timp apasare lunga pentru reset
struct TouchPoint { uint16_t x; uint16_t y; bool touched; }; // Struct punct touch
unsigned long lastTouchMillis = 0; // Anti-bounce touch
int paginaCurenta = 1; // Pagina curenta 1=main 2=system info

// Prototipuri functii
int readLdrAvg();
void updateBacklightFromLDR();
void drawUIStaticElements();
void fetchEarnAppData();
void updateBottomInfoDisplay();
void updateTimeOnly();
bool readTouchPoint(TouchPoint &p);
void calibrareTouch();
void handleTouch();
void desenConturEcran();
void desenButonSysInfo();
void afiseazaSystemInfo();
void desenButonResetTFT();
void resetESP();
void setupWebServer();
float getCpuTemp();

// --- Temperatura CPU REALA - senzor intern ESP32 ---
#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read(); // Functie externa din SDK ESP32
#ifdef __cplusplus
}
#endif

// Functie care citeste temperatura interna CPU ESP32
float getCpuTemp() {
  // Senzor intern ESP32 - returneaza in Fahrenheit brut, convertim in Celsius
  // Valoare reala, nu formula cu heap
  float temp_f = (float)temprature_sens_read();
  float temp_c = (temp_f - 32.0) / 1.8;
  return temp_c;
}

// Task separat pentru control backlight in functie de lumina ambientala
void backlightTask(void *pvParameters) {
  delay(300);
  int first = readLdrAvg(); // Prima citire LDR
  ldrMin = first - 100; // Initializare min/max
  ldrMax = first + 200;
  if(ldrMin < 0) ldrMin = 0;
  if(ldrMax > 4095) ldrMax = 4095;
  smoothBrightness = first;
  for(;;){ // Loop infinit task
    updateBacklightFromLDR();
    vTaskDelay(ldrInterval / portTICK_PERIOD_MS);
  }
}

// Citeste LDR cu mediere pe mai multe esantioane
int readLdrAvg() {
  long sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(PIN_LDR);
    delay(5);
  }
  return (int)(sum / ADC_SAMPLES);
}

// Actualizeaza PWM backlight in functie de LDR cu filtrare
void updateBacklightFromLDR() {
  int ldrValue = readLdrAvg();
  if (ldrValue < ldrMin) ldrMin = ldrValue;
  if (ldrValue > ldrMax) ldrMax = ldrValue;
  if (ldrMax <= ldrMin) ldrMax = ldrMin + 1;
  int targetPwm = map(ldrValue, ldrMin, ldrMax, 200, 1); // Mapare inversa lumina->PWM
  smoothBrightness = smoothBrightness + SMOOTH_FACTOR * (targetPwm - smoothBrightness);
  int pwmValue = (int)smoothBrightness;
  if (pwmValue < 20) pwmValue = 2;
  if (pwmValue > 200) pwmValue = 100;
  analogWrite(PIN_BACKLIGHT, pwmValue);
}

// Pagina HTML principala pentru WebServer
const char HTML_MAIN[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>EarnApp Monitor</title>
<link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap" rel="stylesheet">
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#000;display:flex;justify-content:center;align-items:flex-start;min-height:100vh;font-family:'Share Tech Mono',monospace;padding:10px}
.frame{width:360px;background:#000;padding:6px;border:2px solid #222}
.top{height:40px;background:#0f1e2e;display:flex;align-items:center;justify-content:space-between;padding:0 12px;border-bottom:2px solid #ffb300}
.top h1{color:#00d5ff;font-size:16px;letter-spacing:0.5px}
.ibtn{width:28px;height:28px;border:2px solid #00d5ff;border-radius:50%;color:#00d5ff;display:flex;align-items:center;justify-content:center;font-weight:900;font-size:16px;text-decoration:none}
.card{background:#0f1e2e;border-radius:12px;padding:8px 14px;margin:12px 4px}
.c1{border:2px solid #ffb300}.c2{border:2px solid #00e5ff}
.label{color:#7a8a9a;font-size:12px;letter-spacing:0.5px}
.val{font-size:28px;font-weight:900;letter-spacing:1px;margin-top:2px;line-height:1.1}
.v-green{color:#00ff66}.v-yellow{color:#ffeb3b}.v-white{color:#e6f0ff}
.small{color:#7a8a9a;font-size:11px;margin-top:4px}
.bottom{border:2px solid #ffb300;border-radius:12px;margin:12px 4px;display:grid;grid-template-columns:1fr 1fr 1fr;overflow:hidden;background:#0f1e2e}
.bcol{padding:8px;background:#0f1e2e;border-right:2px solid #ffb300;display:flex;flex-direction:column;justify-content:center}
.bcol:last-child{border:0}
.bcol.label{font-size:11px;margin-bottom:4px}
.bcol.val{font-size:18px}
</style></head><body><div class="frame">
<div class="top"><h1>EarnApp Monitor</h1><a class="ibtn" href="/info">i</a></div>
<div class="card c1"><div class="label">TOTAL EVER:</div><div id="total" class="val v-green">--</div></div>
<div class="card c2"><div class="label">BALANCE / PENDING:</div><div id="bal" class="val v-yellow">--</div><div id="pend" class="small">Pending: -- | Payout la $10</div></div>
<div class="bottom">
<div class="bcol"><div class="label">TIME</div><div id="time" class="val v-white">--:--:--</div></div>
<div class="bcol"><div class="label">DEVICES</div><div id="dev" class="val v-yellow">--</div></div>
<div class="bcol"><div class="label">PAYMENTS</div><div id="pay" class="val v-green">--</div></div>
</div>
</div>
<script>
async function upd(){
 try{
  let r=await fetch('/api/data');let j=await r.json();
  document.getElementById('total').innerText='$'+j.total.toFixed(3);
  document.getElementById('bal').innerText='$'+j.balance.toFixed(3);
  document.getElementById('pend').innerText='Pending: $'+j.pending.toFixed(3)+' | Paid: $'+j.paid.toFixed(3)+' (pays '+j.payments+')';
  document.getElementById('dev').innerText=j.devices;
  document.getElementById('pay').innerText=j.payments;
  document.getElementById('time').innerText=j.time;
 }catch(e){}
}
setInterval(upd,1000);upd();
</script></body></html>
)rawliteral";

// Pagina HTML pentru System Info
const char HTML_INFO[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>System Info</title>
<link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap" rel="stylesheet">
<style>
*{box-sizing:border-box}body{background:#000;display:flex;justify-content:center;padding:10px;font-family:'Share Tech Mono',monospace;color:#fff}
.frame{width:380px;background:#000;padding:6px;border:2px solid #222}
.top{height:40px;background:#0f1e2e;display:flex;align-items:center;justify-content:space-between;padding:0 12px;border-bottom:2px solid #ffb300}
.top h1{color:#00d5ff;font-size:15px;letter-spacing:0.5px}
.ibtn{width:28px;height:28px;border:2px solid #00d5ff;border-radius:50%;color:#00d5ff;display:flex;align-items:center;justify-content:center;font-weight:900;font-size:16px;text-decoration:none}
.rbtn{width:28px;height:28px;border-radius:50%;background:#ff0000;color:#fff;display:flex;align-items:center;justify-content:center;font-weight:900;font-size:16px;text-decoration:none;border:2px solid #ff4444}
.actions{display:flex;gap:8px;align-items:center}
.card{background:#0f1e2e;border:2px solid #ffb300;border-radius:12px;padding:12px;margin:12px 4px;line-height:20px;font-size:12px}
.label{color:#ffb300;font-size:10px;margin-top:10px}.val{color:#e6f0ff}.cyan{color:#00d5ff}.yellow{color:#ffeb3b}.green{color:#00ff66}
hr{border:0;border-top:1px solid #ffb300;margin:10px 0}
.devline{color:#ccc;font-size:11px;line-height:16px}
</style></head><body><div class="frame">
<div class="top"><h1>SYSTEM INFO - EARNAPP</h1><div class="actions"><a class="rbtn" href="/reset">R</a><a class="ibtn" href="/">&#8592;</a></div></div>
<div id="info" class="card">Loading...</div>
</div>
<script>
async function u(){
 try{
  let r=await fetch('/api/system');let j=await r.json();
  let h=Math.floor(j.uptime/3600),m=Math.floor(j.uptime%3600/60),s=Math.floor(j.uptime%60);
  let uptimeStr=(h<10?'0':'')+h+':'+(m<10?'0':'')+m+':'+(s<10?'0':'')+s;
  let devHtml=''; j.devlist.forEach((d,i)=>{
    let col=d.online?'#00ff66':'#ff3333';
    let st=d.online?'ONLINE':'OFFLINE';
    devHtml+= (i+1)+'. '+d.title+' | $'+d.earned.toFixed(3)+' / $'+d.earned_total.toFixed(3)+' <span style="color:'+col+';font-weight:900">'+st+'</span><br>';
  });
  document.getElementById('info').innerHTML=
'CPU: ESP32 @ '+j.cpu+' MHz<br>Flash: '+j.flash+' MB<br>Heap Free: '+j.heap+' KB<br>Uptime: '+uptimeStr+'<br>Temp CPU: '+j.temp.toFixed(1)+' C<br>TFT: ILI9341 320x240<br>Touch: XPT2046 HSPI<br>IP: <span class="cyan">'+j.ip+'</span><br>RSSI: '+j.rssi+' dBm<br><hr><div class="label">EARNAPP:</div>Total Ever: <span class="green">$'+j.total.toFixed(3)+'</span> | Bal: <span class="yellow">$'+j.balance.toFixed(3)+'</span><br>Pending: $'+j.pending.toFixed(3)+' | Paid: $'+j.paid.toFixed(3)+' (pays '+j.payments+')<br>Devices: '+j.devices+' online<br><br><div class="label">DEVICE-URI:</div><div class="devline">'+(devHtml || 'No devices')+'</div>';

 }catch(e){document.getElementById('info').innerText='Eroare load: '+e;}
}
setInterval(u,1000);u();
</script>
</body>
</html>
)rawliteral"; // fix IDE highlight - "

// Setup initial la pornire
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- INITIALIZARE SISTEM EARNAPP MONITOR ---");
  pinMode(PIN_BACKLIGHT, OUTPUT); // Setare pin backlight ca iesire
  pinMode(PIN_LDR, INPUT); // Setare pin LDR ca intrare
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS); // Initializare SPI pentru TFT
  EEPROM.begin(EEPROM_SIZE); // Initializare EEPROM
  int flag = 0; 
  EEPROM.get(ADDR_FLAG, flag); // Citeste flag calibrare
  if (flag == 1234) { // Daca e calibrat deja
    EEPROM.get(ADDR_MINX, TS_MINX); 
    EEPROM.get(ADDR_MAXX, TS_MAXX);
    EEPROM.get(ADDR_MINY, TS_MINY); 
    EEPROM.get(ADDR_MAXY, TS_MAXY);
    calibrated = true;
  }
  tft.begin(); // Porneste TFT
  tft.setRotation(1); // Rotatie landscape
  tft.fillScreen(COLOR_BG);
  analogWrite(PIN_BACKLIGHT, 150); // Luminozitate initiala
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS); // SPI touch
  ts.begin(touchscreenSPI);
  ts.setRotation(1);
  if (ts.touched()) { delay(1000); // Daca e atins la boot intra in calibrare
  if (ts.touched()) calibrareTouch(); 
  }
  if (!calibrated) calibrareTouch(); // Calibrare fortata daca nu e calibrat
  // Ecran de asteptare conectare WiFi
  tft.fillRect(40, 90, 240, 60, COLOR_CARD_BG);
  tft.drawRect(40, 90, 240, 60, COLOR_ACCENT);
  tft.setCursor(60, 112);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.print("Conectare Wi-Fi...");
  Serial.printf("Conectare la reteaua Wi-Fi: [%s]\n", ssid);
  WiFi.begin(ssid, password); // Incepe conectarea WiFi
  int attempts = 0;
  while (WiFi.status()!= WL_CONNECTED) { // Asteapta conectarea
    delay(500); Serial.print("."); attempts++;
    if (attempts > 40) { // Timeout dupa 20 secunde
      Serial.println("\nEroare: Conectarea Wi-Fi a esuat.");
      tft.fillRect(40, 90, 240, 60, COLOR_CARD_BG);
      tft.setCursor(55, 112); 
      tft.setTextColor(COLOR_STATUS_ERR); 
      tft.setTextSize(2);
      tft.print("Wi-Fi EROARE!"); 
      break;
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi Conectat!"); Serial.print("IP ESP32: "); Serial.println(WiFi.localIP());
    configTime(7200, 3600, "pool.ntp.org", "time.nist.gov"); // Config NTP + DST Romania
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1); tzset(); // Setare fus orar
    setupWebServer(); // Porneste webserverul
  }
  drawUIStaticElements(); // Deseneaza interfata statica
  if (WiFi.status() == WL_CONNECTED) { fetchEarnAppData(); lastMillis = millis(); } // Primul fetch
  xTaskCreatePinnedToCore(backlightTask, "BacklightTask", 4096, NULL, 2, NULL, 1); // Creeaza task backlight pe core 1
}

// Loop principal
void loop() {
  server.handleClient(); // Proceseaza cereri web
  handleTouch(); // Proceseaza touch
  if (millis() - lastTimeUpdateMillis >= 1000) { // Update ceas la 1 sec
    lastTimeUpdateMillis = millis();
    if (paginaCurenta == 1) updateTimeOnly();
  }
  if (millis() - lastMillis >= interval) { // Fetch periodic API
    lastMillis = millis();
    if (WiFi.status() == WL_CONNECTED) {
      if (paginaCurenta == 1) fetchEarnAppData();
    } else {
      Serial.println("Wi-Fi deconectat. Reincerc reconectarea...");
      WiFi.begin(ssid, password);
      tft.fillRect(20, 55, 280, 25, COLOR_CARD_BG);
      tft.setCursor(20, 58); 
      tft.setTextColor(COLOR_STATUS_ERR); 
      tft.setTextSize(2);
      tft.print("Wi-Fi OFF"); 
      updateBottomInfoDisplay();
    }
  }
}

// Deseneaza elementele statice ale UI-ului principal
void drawUIStaticElements() {
  tft.fillScreen(COLOR_BG);
  tft.fillRect(0, 0, 320, 35, COLOR_CARD_BG); // Header
  tft.drawFastHLine(0, 35, 320, COLOR_ACCENT);
  tft.setCursor(15, 10); 
  tft.setTextColor(COLOR_TITLE); 
  tft.setTextSize(2); 
  tft.print("EarnApp Monitor");
  tft.fillRoundRect(10, 45, 300, 52, 6, COLOR_CARD_BG); // Card TOTAL EVER
  tft.drawRoundRect(10, 45, 300, 52, 6, COLOR_ACCENT);
  tft.setCursor(20, 49); 
  tft.setTextColor(COLOR_LABEL); 
  tft.setTextSize(1); 
  tft.print("TOTAL EVER:");
  tft.fillRoundRect(10, 105, 300, 70, 6, COLOR_CARD_BG); // Card BALANCE
  tft.drawRoundRect(10, 105, 300, 70, 6, COLOR_TITLE);
  tft.setCursor(20, 109); 
  tft.setTextColor(COLOR_LABEL); 
  tft.setTextSize(1); 
  tft.print("BALANCE / PENDING:");
  tft.fillRoundRect(10, 183, 300, 50, 6, COLOR_CARD_BG); // Zona jos TIME/DEVICES/PAYMENTS
  tft.drawRoundRect(10, 183, 300, 50, 6, COLOR_ACCENT);
  tft.drawFastVLine(110, 183, 50, COLOR_ACCENT);
  tft.drawFastVLine(210, 183, 50, COLOR_ACCENT);
  tft.setTextSize(1); 
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(18, 187); 
  tft.print("TIME");
  tft.setCursor(130, 187); 
  tft.print("DEVICES");
  tft.setCursor(225, 187); 
  tft.print("PAYMENTS");
  desenButonSysInfo(); // Deseneaza butonul i
}

// Update doar pentru ceas, fara a redesena tot
void updateTimeOnly() {
  if (paginaCurenta!= 1) return;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) { // Fallback daca NTP nu e sincronizat
    tft.fillRect(12, 200, 96, 28, COLOR_CARD_BG);
    tft.setCursor(18, 205); 
    tft.setTextColor(COLOR_TEXT); 
    tft.setTextSize(2);
    unsigned long sec = millis() / 1000;
    tft.printf("%02lu:%02lu:%02lu", (sec/3600)%24, (sec/60)%60, sec%60); 
    return;
  }
  tft.fillRect(12, 200, 96, 28, COLOR_CARD_BG);
  tft.setCursor(14, 203); 
  tft.setTextColor(COLOR_TEXT); 
  tft.setTextSize(2);
  tft.printf("%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

// Update zona de jos cu devices si payments + timp
void updateBottomInfoDisplay() {
  if (paginaCurenta!= 1) return;
  tft.fillRect(112, 200, 96, 28, COLOR_CARD_BG);
  tft.setCursor(118, 203); 
  tft.setTextColor(COLOR_VALUE_MYST); 
  tft.setTextSize(2);
  tft.printf("%d dev", g_device_count);
  tft.fillRect(212, 200, 96, 28, COLOR_CARD_BG);
  tft.setCursor(220, 203); 
  tft.setTextColor(COLOR_STATUS_OK); 
  tft.setTextSize(2);
  tft.printf("%d pay", g_payments_count);
  updateTimeOnly();
}

// --- FUNCTIE REALA ONLINE/OFFLINE - NU DEGRADAM CODUL ---
// Determina statusul real online/offline dintr-un obiect JSON
bool isRealOnline(JsonObject obj){
  if(obj["online"].is<bool>()) return obj["online"].as<bool>();
  if(obj["is_online"].is<bool>()) return obj["is_online"].as<bool>();
  if(obj["connected"].is<bool>()) return obj["connected"].as<bool>();
  if(obj["is_active"].is<bool>() && obj["status"].isNull()) return obj["is_active"].as<bool>();
  const char* s = obj["status"] | obj["state"] | "";
  String st = String(s); st.toLowerCase();
  if(st=="offline" || st=="inactive" || st=="disabled" || st=="disconnected" || st=="not_active") return false;
  if(st=="online" || st=="active" || st=="connected" || st=="running" || st=="ok") return true;
  if(obj["is_active"]==false) return false;
  return true; // daca nu e marcat offline explicit, e online REAL
}

// --- FETCH REAL - FARA FALLBACK FALS - FIX COMPILARE 3.3.12 ---
// --- FETCH REAL - FARA d["rate"] = $5.00 fals ---
// Functia principala care ia date de la EarnApp API
void fetchEarnAppData() {
  if (WiFi.status()!= WL_CONNECTED) return;
  WiFiClientSecure client;
  client.setInsecure(); // Ignora certificat SSL
  HTTPClient http;
  String cookie = String("auth=1; auth-method=google; oauth-token=")+earn_oauth_token+"; oauth-refresh-token="+earn_refresh_token+"; xsrf-token="+earn_xsrf_token+"; XSRF-TOKEN="+earn_xsrf_token2;

  String url, payload;
  int code;

  // --- Cerere 1: money - balanta si total ---
  url = "https://earnapp.com/dashboard/api/money/?appid=earnapp_dashboard";
  http.begin(client,url);
  http.addHeader("Cookie",cookie);
  http.addHeader("x-xsrf-token",earn_xsrf_token2);
  http.addHeader("User-Agent","Mozilla/5.0");
  http.addHeader("Referer","https://earnapp.com/dashboard");
  code=http.GET();
  Serial.printf("[money] %d\n",code);
  if(code==200){
    payload=http.getString();
    JsonDocument doc;
    if(!deserializeJson(doc,payload)){
      g_balance=doc["balance"]|0.0;
      g_total_ever=doc["earnings_total"]|0.0;
      g_pending=g_balance;
      g_total_paid=g_total_ever-g_balance;
      // Afisare pe TFT
      tft.fillRect(20, 62, 280, 30, COLOR_CARD_BG);
      tft.setCursor(20, 65); 
      tft.setTextColor(COLOR_STATUS_OK); 
      tft.setTextSize(3);
      tft.printf("$%.3f", g_total_ever);
      tft.fillRect(20, 125, 280, 30, COLOR_CARD_BG);
      tft.setCursor(20, 128); 
      tft.setTextColor(COLOR_VALUE_MYST); 
      tft.setTextSize(3);
      tft.printf("$%.3f", g_balance);
      tft.fillRect(20, 155, 280, 15, COLOR_CARD_BG);
      tft.setCursor(20, 158); 
      tft.setTextColor(COLOR_LABEL); 
      tft.setTextSize(1);
      tft.printf("Pend $%.3f | Paid $%.3f | Need $%.2f", g_pending, g_total_paid, 10.0-g_balance);
    }
  }
  http.end();

  // Reset lista device-uri
  g_devices_found=0; 
  g_device_count=0;
  for(int i=0;i<10;i++){ g_devices[i].title=""; 
  g_devices[i].earned=0; 
  g_devices[i].earned_total=0; 
  g_devices[i].online=false; 
  }

  // --- Cerere 2: devices ---
  url="https://earnapp.com/dashboard/api/devices/?appid=earnapp_dashboard";
  http.begin(client,url);
  http.addHeader("Cookie",cookie);
  http.addHeader("x-xsrf-token",earn_xsrf_token2);
  http.addHeader("User-Agent","Mozilla/5.0");
  code=http.GET();
  Serial.printf("[devices] %d\n",code);
  if(code==200){
    payload=http.getString();
    Serial.println(payload.substring(0,3000));
    JsonDocument doc;
    if(!deserializeJson(doc,payload)){
      JsonArray arr;
      if(doc.is<JsonArray>()) arr=doc.as<JsonArray>();
      else if(doc["devices"].is<JsonArray>()) arr=doc["devices"].as<JsonArray>();
      if(!arr.isNull()){
        int online=0;
        for(JsonObject d:arr){
          if(g_devices_found>=10) break;
          String title = d["appid_title"] | d["title"] | d["name"] | d["uuid"] | "Device";
          if(title.length()<2) continue;
          // MODIFICARE AICI - NU MAI LUAM rate = 5
          float earned = d["earned"] | d["earnings"] | d["total_earning"] | d["lifetime_earnings"] | 0.0;
          float earned_total = d["earned_total"] | d["total_earnings"] | d["lifetime_earnings"] | 0.0;
          bool isOnline = isRealOnline(d);
          g_devices[g_devices_found].title=title;
          g_devices[g_devices_found].earned=earned;
          g_devices[g_devices_found].earned_total=earned_total;
          g_devices[g_devices_found].online=isOnline;
          g_devices_found++; 
          if(isOnline) online++;
        }
        g_device_count=online;
      }
    }
  }
  http.end();

  // --- Fallback 1: user_data daca devices e gol ---
  if(g_devices_found==0){
    url="https://earnapp.com/dashboard/api/user_data/?appid=earnapp_dashboard";
    http.begin(client,url);
    http.addHeader("Cookie",cookie);
    http.addHeader("x-xsrf-token",earn_xsrf_token2);
    http.addHeader("User-Agent","Mozilla/5.0");
    code=http.GET();
    if(code==200){
      payload=http.getString();
      JsonDocument doc;
      if(!deserializeJson(doc,payload)){
        JsonArray devices=doc["devices"];
        if(!devices.isNull()){
          int online=0;
          for(JsonObject d:devices){
            if(g_devices_found>=10) break;
            String title = d["title"]|d["name"]|"Device";
            if(title.length()<2) continue;
            float earned=d["earned"]|d["earnings"]|d["total_earning"]|0.0;
            float earned_total=d["earned_total"]|d["total_earnings"]|0.0;
            bool isOnline = isRealOnline(d);
            g_devices[g_devices_found].title=title;
            g_devices[g_devices_found].earned=earned;
            g_devices[g_devices_found].earned_total=earned_total;
            g_devices[g_devices_found].online=isOnline;
            g_devices_found++; 
            if(isOnline) online++;
          }
          g_device_count=online;
        }
      }
    }
    http.end();
  }

  // --- Fallback 2: device_statuses ---
  if(g_devices_found==0){
    url="https://earnapp.com/dashboard/api/device_statuses?appid=earnapp&version=1.676.514";
    http.begin(client,url);
    http.addHeader("Cookie",cookie);
    http.addHeader("x-xsrf-token",earn_xsrf_token2);
    http.addHeader("User-Agent","Mozilla/5.0");
    code=http.GET();
    if(code==200){
      payload=http.getString();
      JsonDocument doc;
      if(!deserializeJson(doc,payload)){
        int online=0;
        if(doc.is<JsonArray>()){
          JsonArray arr=doc.as<JsonArray>();
          for(JsonObject d:arr){
            if(g_devices_found>=10) break;
            String title=d["title"]|d["name"]|d["id"]|"Device";
            bool isOnline = isRealOnline(d);
            g_devices[g_devices_found].title=title;
            g_devices[g_devices_found].earned=d["earned"]|d["earnings"]|0.0;
            g_devices[g_devices_found].earned_total=d["earned_total"]|0.0;
            g_devices[g_devices_found].online=isOnline;
            g_devices_found++; 
            if(isOnline) online++;
          }
        } else if(doc.is<JsonObject>()){
          for(JsonPair kv : doc.as<JsonObject>()){
            if(g_devices_found>=10) break;
            if(!kv.value().is<JsonObject>()) continue;
            JsonObject d=kv.value().as<JsonObject>();
            String title = d["title"].is<const char*>()? d["title"].as<String>() : String(kv.key().c_str());
            bool isOnline = isRealOnline(d);
            g_devices[g_devices_found].title=title;
            g_devices[g_devices_found].earned=d["earned"]|0.0;
            g_devices[g_devices_found].earned_total=d["earned_total"]|0.0;
            g_devices[g_devices_found].online=isOnline;
            g_devices_found++; 
            if(isOnline) online++;
          }
        }
        g_device_count=online;
      }
    }
    http.end();
  }

  // Log serial pentru device-uri
  if(g_devices_found==0){
    Serial.println("REAL: 0 devices");
  } else {
    Serial.printf("REAL Devices: %d total, %d online\n",g_devices_found,g_device_count);
    for(int i=0;i<g_devices_found;i++) Serial.printf(" %d: %s $%.4f %s\n", i+1, g_devices[i].title.c_str(), g_devices[i].earned, g_devices[i].online?"ON":"OFF");
  }

  // --- Cerere 3: transactions - numar plati ---
  url="https://earnapp.com/dashboard/api/transactions/?appid=earnapp_dashboard";
  http.begin(client,url);
  http.addHeader("Cookie",cookie);
  http.addHeader("x-xsrf-token",earn_xsrf_token2);
  code=http.GET();
  if(code==200){
    payload=http.getString();
    JsonDocument doc;
    if(!deserializeJson(doc,payload)){
      if(doc.is<JsonArray>()) g_payments_count=doc.size();
    }
  }
  http.end();
  updateBottomInfoDisplay(); // Actualizeaza afisajul de jos
}

// Deseneaza contur ecran
void desenConturEcran() { tft.drawRect(0, 0, 320, 240, COLOR_ACCENT); }

// Citeste punctul de touch si face mapare la coordonate ecran
bool readTouchPoint(TouchPoint &p) {
  if (!ts.touched()) return false;
  TS_Point pt = ts.getPoint();
  if (pt.z < 200) return false; // Presiune prea mica
  if (!calibrated) { p.x = pt.x; p.y = pt.y; }
  else {
    p.x = map(pt.x, TS_MINX, TS_MAXX, 0, 320);
    p.y = map(pt.y, TS_MINY, TS_MAXY, 0, 240);
  }
  p.touched = true; 
  delay(75); 
  return true;
}

// Gestioneaza actiuni la touch
void handleTouch() {
  TouchPoint p;
  if (!readTouchPoint(p)) return;
  if (millis() - lastTouchMillis < 400) return; // Debounce
  lastTouchMillis = millis();
  if (p.x > SYSINFO_BTN_X && p.x < SYSINFO_BTN_X + SYSINFO_BTN_W && p.y > SYSINFO_BTN_Y && p.y < SYSINFO_BTN_Y + SYSINFO_BTN_H) {
    afiseazaSystemInfo(); return; // Click pe buton info
  }
  if (p.x > 10 && p.x < 310 && p.y > 45 && p.y < 225 && paginaCurenta == 1) {
    lastMillis = 0; // Forteaza refresh
    fetchEarnAppData();
  }
  while (ts.touched()) delay(10);
}

// Calibrare touchscreen in 4 puncte
void calibrareTouch() {
  analogWrite(PIN_BACKLIGHT, 30);
  int minX = 9999, maxX = 0, minY = 9999, maxY = 0;
  tft.fillScreen(COLOR_BG); 
  tft.setTextColor(COLOR_STATUS_ERR); 
  tft.setTextSize(2);
  int puncte[4][2] = {{10, 10}, {310, 10}, {10, 230}, {310, 230}};
  for (int i = 0; i < 4; i++) {
    tft.fillScreen(COLOR_BG); 
    desenConturEcran();
    tft.setCursor(60, 100); 
    tft.println("Atinge punctul!");
    tft.fillCircle(puncte[i][0], puncte[i][1], 8, COLOR_STATUS_ERR);
    bool ok = false;
    while (!ok) {
      if (ts.touched()) {
        TS_Point pt = ts.getPoint();
        if (pt.x < minX) minX = pt.x; 
        if (pt.x > maxX) maxX = pt.x;
        if (pt.y < minY) minY = pt.y; 
        if (pt.y > maxY) maxY = pt.y;
        ok = true; 
        delay(500); 
        while (ts.touched()) delay(10);
      }
    }
  }
  TS_MINX = minX; 
  TS_MAXX = maxX; 
  TS_MINY = minY; 
  TS_MAXY = maxY; 
  calibrated = true;
  int flag = 1234;
  EEPROM.put(ADDR_FLAG, flag); 
  EEPROM.put(ADDR_MINX, TS_MINX);
  EEPROM.put(ADDR_MAXX, TS_MAXX); 
  EEPROM.put(ADDR_MINY, TS_MINY); 
  EEPROM.put(ADDR_MAXY, TS_MAXY);
  EEPROM.commit(); // Salveaza in EEPROM
  tft.fillScreen(COLOR_BG);
  tft.setCursor(40, 100); 
  tft.setTextColor(COLOR_STATUS_OK); 
  tft.println("Calibrare OK!"); 
  delay(1000);
}

// Deseneaza butonul rotund i pentru System Info
void desenButonSysInfo() {
  tft.fillRoundRect(SYSINFO_BTN_X, SYSINFO_BTN_Y, SYSINFO_BTN_W, SYSINFO_BTN_H, 8, COLOR_CARD_BG);
  tft.drawRoundRect(SYSINFO_BTN_X, SYSINFO_BTN_Y, SYSINFO_BTN_W, SYSINFO_BTN_H, 8, COLOR_TITLE);
  tft.setCursor(SYSINFO_BTN_X + 9, SYSINFO_BTN_Y + 6);
  tft.setTextSize(2); 
  tft.setTextColor(COLOR_TITLE); 
  tft.print("i");
}

// Deseneaza butonul rosu R pentru Reset
void desenButonResetTFT() {
  tft.fillCircle(RESET_BTN_X, RESET_BTN_Y, RESET_BTN_R, COLOR_STATUS_ERR);
  tft.drawCircle(RESET_BTN_X, RESET_BTN_Y, RESET_BTN_R, COLOR_TEXT);
  tft.setCursor(RESET_BTN_X - 4, RESET_BTN_Y - 6);
  tft.setTextColor(COLOR_TEXT); 
  tft.setTextSize(2); 
  tft.print("R");
}
// Functie restart ESP
void resetESP() { ESP.restart(); 
}

// Afiseaza pagina de System Info cu detalii hardware si EarnApp
void afiseazaSystemInfo() {
  paginaCurenta = 2;
  tft.fillScreen(COLOR_BG);
  desenConturEcran();
  tft.setTextSize(2);
  tft.setTextColor(COLOR_VALUE_MYST);
  tft.setCursor(10, 15);
  tft.print("SYSTEM INFO - EARNAPP");
  tft.drawFastHLine(0, 35, 320, COLOR_ACCENT);

  desenButonResetTFT(); // Arata butonul R

  unsigned long start = millis();
  while (millis() - start < 25000) { // Sta in System Info max 25 sec
    server.handleClient();
    TouchPoint rp;
    if (readTouchPoint(rp)) {
      int dx = rp.x - RESET_BTN_X; 
      int dy = rp.y - RESET_BTN_Y;
      if (dx*dx + dy*dy <= (RESET_BTN_R+5)*(RESET_BTN_R+5)) {
        // AM ATINS RESET - INCEPEM NUMARATOAREA DE 3 SEC
        unsigned long holdStart = millis();
        bool stillHolding = true;

        // deseneaza mesaj
        tft.fillRect(40, 105, 240, 40, COLOR_BG);
        tft.drawRect(40, 105, 240, 40, COLOR_STATUS_ERR);
        tft.setCursor(55, 110);
        tft.setTextColor(COLOR_TEXT);
        tft.setTextSize(2);
        tft.print("TINE 3s PENTRU");
        tft.setCursor(65, 125);
        tft.print("RESET!    ");

        while (ts.touched() && stillHolding) {
          server.handleClient();
          unsigned long elapsed = millis() - holdStart;
          
          // bara progres
          int w = map(elapsed, 0, RESET_HOLD_TIME, 0, 236);
          if (w > 236) w = 236;
          tft.fillRect(42, 142, w, 3, COLOR_STATUS_ERR);

          // countdown text
          int secLeft = 3 - (elapsed / 1000);
          if (secLeft < 0) secLeft = 0;
          tft.fillRect(160, 125, 30, 15, COLOR_BG);
          tft.setCursor(160, 125);
          tft.setTextColor(COLOR_TITLE);
          tft.print(String(secLeft) + "   ");

          if (elapsed >= RESET_HOLD_TIME) {
            tft.fillRect(40, 105, 240, 40, COLOR_STATUS_ERR);
            tft.setCursor(60, 118);
            tft.setTextColor(COLOR_TEXT);
            tft.setTextSize(2);
            tft.print("RESTART...");
            delay(500);
            resetESP();
          }
          delay(20);
        }
        // daca a dat drumul inainte de 3 sec -> anulam
        tft.fillRect(40, 105, 240, 45, COLOR_BG);
        desenConturEcran();
        tft.drawFastHLine(0, 35, 320, COLOR_ACCENT);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_VALUE_MYST);
        tft.setCursor(10, 15);
        tft.print("SYSTEM INFO - EARNAPP");
        desenButonResetTFT();
        continue; // nu iesi din System Info
      }
      // iesire din System Info la orice alta atingere
      while (ts.touched()) delay(10);
      delay(300);
      break;
    }

    int y = 45;
    tft.setTextSize(1);
    tft.setTextColor(COLOR_TEXT);

    tft.fillRect(90, y, 200, 10, COLOR_BG);
    tft.setCursor(10, y); 
    tft.print("CPU: ESP32 @");
    tft.setCursor(90, y); 
    tft.printf("%lu MHz", ESP.getCpuFreqMHz()); 
    y+=12;

    tft.fillRect(50, y, 200, 10, COLOR_BG);
    tft.setCursor(10, y); 
    tft.print("Flash:");
    tft.setCursor(50, y); 
    tft.printf("%lu MB", ESP.getFlashChipSize()/1024/1024); 
    y+=12;

    tft.fillRect(80, y, 200, 10, COLOR_BG);
    tft.setCursor(10, y); 
    tft.print("Heap Free:");
    tft.setCursor(80, y); 
    tft.printf("%lu KB", ESP.getFreeHeap()/1024); 
    y+=12;

    tft.fillRect(60, y, 200, 10, COLOR_BG);
    tft.setCursor(10, y); 
    tft.print("Uptime:");
    tft.setCursor(60, y); 
    tft.printf("%02lu:%02lu:%02lu", (millis()/3600000), (millis()/60000)%60, (millis()/1000)%60); 
    y+=12;

    g_temp_cpu = getCpuTemp();
    tft.fillRect(75, y, 200, 10, COLOR_BG);
    tft.setCursor(10, y); 
    tft.print("Temp CPU:");
    tft.setCursor(75, y); 
    tft.printf("%.1f C", g_temp_cpu); 
    y+=12;

    tft.setCursor(10, y); 
    tft.print("TFT: ILI9341 320x240"); 
    y+=12;

    tft.setCursor(10, y); 
    tft.print("Touch: XPT2046 HSPI"); 
    y+=12;

    tft.fillRect(30, y, 260, 10, COLOR_BG);
    tft.setCursor(10, y); 
    tft.print("IP:");
    tft.setCursor(30, y); 
    tft.printf("%s", WiFi.localIP().toString().c_str()); 
    y+=12;

    tft.fillRect(45, y, 200, 10, COLOR_BG);
    tft.setCursor(10, y); 
    tft.print("RSSI:");
    tft.setCursor(45, y); 
    tft.printf("%d dBm", WiFi.RSSI()); 
    y+=12;

    y+=4; 
    tft.drawFastHLine(0, y, 320, COLOR_ACCENT); 
    y+=6;

    tft.setTextColor(COLOR_LABEL); 
    tft.setCursor(10, y); 
    tft.print("EARNAPP:"); 
    y+=12;

    tft.fillRect(50, y, 260, 10, COLOR_BG);
    tft.setCursor(10, y); 
    tft.print("Total:");
    tft.setCursor(50, y); 
    tft.setTextColor(COLOR_TEXT); 
    tft.printf("$%.3f = $%.3f + $%.3f", g_total_ever, g_balance, g_total_paid); y+=12;

    tft.fillRect(60, y, 250, 10, COLOR_BG);
    tft.setCursor(10, y); 
    tft.setTextColor(COLOR_LABEL); 
    tft.print("Devices:");
    tft.setCursor(60, y);
    if(g_device_count>0) tft.setTextColor(COLOR_STATUS_OK); 
    else tft.setTextColor(COLOR_STATUS_ERR);
    tft.printf("%d online / %d total", g_device_count, g_devices_found);
    tft.setTextColor(COLOR_TEXT); 
    y+=12;

    for(int i=0;i<g_devices_found && i<4;i++){ // Afisare max 4 device-uri
      tft.fillRect(10, y, 300, 10, COLOR_BG);
      tft.setCursor(10, y);
      tft.setTextColor(COLOR_TITLE);
      tft.printf("%d. ", i+1);
      if(g_devices[i].online) tft.setTextColor(COLOR_STATUS_OK); 
      else tft.setTextColor(COLOR_STATUS_ERR);
      tft.printf("%s $%.3f %s", g_devices[i].title.c_str(), g_devices[i].earned, g_devices[i].online?"ON":"OFF");
      y+=10;
    }
    for(int i=g_devices_found;i<4;i++){
      tft.fillRect(10, y, 310, 10, COLOR_BG);
      y+=10;
    }
    delay(1000);
  }
  paginaCurenta = 1; // Revenire la pagina principala
  drawUIStaticElements(); 
  fetchEarnAppData();
}

// Configureaza rutele webserverului
void setupWebServer(){
  server.on("/", [](){ server.send_P(200,"text/html",HTML_MAIN); }); // Ruta principala
  server.on("/info", [](){ server.send_P(200,"text/html",HTML_INFO); }); // Ruta info
  server.on("/reset", HTTP_GET, [](){ // Ruta reset
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{background:#0f172a;display:flex;justify-content:center;align-items:center;height:100vh;margin:0;font-family:Arial}.box{border:3px solid #0066ff;background:#fff;padding:40px 60px;border-radius:12px;text-align:center}.box h1{color:#0066ff}</style></head><body><div class='box'><h1>ESP RESETAT</h1><p>Se restarteaza...</p></div><script>setTimeout(()=>{window.location.href='/';},3000);</script></body></html>";
    server.send(200, "text/html", html); 
    delay(1500); 
    ESP.restart();
  });
  server.on("/api/data", [](){ // API JSON pentru pagina principala
    struct tm timeinfo; 
    char tbuf[20]="--:--:--";
    if(getLocalTime(&timeinfo)) strftime(tbuf,20,"%H:%M:%S",&timeinfo);
    else { unsigned long s=millis()/1000; snprintf(tbuf,20,"%02lu:%02lu:%02lu",(s/3600)%24,(s/60)%60,s%60); 
    }
    JsonDocument doc;
    doc["total"]=g_total_ever; 
    doc["balance"]=g_balance; 
    doc["pending"]=g_pending; 
    doc["paid"]=g_total_paid;
    doc["devices"]=g_device_count; 
    doc["payments"]=g_payments_count; 
    doc["time"]=tbuf;
    String out; 
    serializeJson(doc,out); 
    server.send(200,"application/json",out);
  });
  server.on("/api/system", [](){ // API JSON pentru System Info
    float temp = getCpuTemp();
    JsonDocument doc;
    doc["cpu"]=ESP.getCpuFreqMHz(); 
    doc["flash"]=ESP.getFlashChipSize()/1024/1024; 
    doc["heap"]=ESP.getFreeHeap()/1024;
    doc["uptime"]=millis()/1000; 
    doc["temp"]=temp; 
    doc["ip"]=WiFi.localIP().toString(); 
    doc["rssi"]=WiFi.RSSI();
    doc["total"]=g_total_ever; 
    doc["balance"]=g_balance; 
    doc["pending"]=g_pending; 
    doc["paid"]=g_total_paid;
    doc["devices"]=g_device_count; 
    doc["payments"]=g_payments_count;
    JsonArray arr=doc["devlist"].to<JsonArray>();
    for(int i=0;i<g_devices_found;i++){
      JsonObject o=arr.add<JsonObject>();
      o["title"]=g_devices[i].title; 
      o["earned"]=g_devices[i].earned; 
      o["earned_total"]=g_devices[i].earned_total; 
      o["online"]=g_devices[i].online;
    }
    String out; 
    serializeJson(doc,out); 
    server.send(200,"application/json",out);
  });
  server.begin();
  Serial.println("WebServer pornit: http://" + WiFi.localIP().toString() + "/");
}