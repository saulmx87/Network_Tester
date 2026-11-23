#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const char* AP_SSID = "NETWORK_TESTER";
const char* AP_PASSWORD = "12345678";
const char* AP_HOSTNAME = "network-tester";

const uint8_t TX_PINS[8] = {13, 14, 27, 26, 25, 33, 32, 23};
const uint8_t RX_PINS[8] = {4, 16, 17, 18, 19, 2, 5, 12};

Adafruit_SSD1306 display(128, 64, &Wire, -1);
WebServer server(80);
DNSServer dnsServer;

const byte DNS_PORT = 53;
uint32_t lastOledUpdate = 0;
uint8_t sequentialPin = 0;
bool toneActive = false;
uint32_t toneUntil = 0;
bool portBlinkActive = false;
uint32_t portBlinkNext = 0;
bool portBlinkLevel = false;
uint32_t switchLastPulse = 0;
bool switchDetected = false;

struct CableResult {
  int status[8];
  int8_t directMap[8];
  uint8_t brokenCount;
  uint8_t crossedCount;
  bool gigabit;
} cableResult;

void setAllTx(bool level) {
  for (uint8_t i = 0; i < 8; i++) digitalWrite(TX_PINS[i], level ? HIGH : LOW);
}

void configurePins() {
  for (uint8_t i = 0; i < 8; i++) {
    pinMode(TX_PINS[i], OUTPUT);
    digitalWrite(TX_PINS[i], LOW);
    pinMode(RX_PINS[i], INPUT);
  }
}

void scanCable() {
  memset(cableResult.status, 0, sizeof(cableResult.status));
  memset(cableResult.directMap, -1, sizeof(cableResult.directMap));
  cableResult.brokenCount = 0;
  cableResult.crossedCount = 0;

  setAllTx(LOW);
  delay(20);

  for (uint8_t tx = 0; tx < 8; tx++) {
    digitalWrite(TX_PINS[tx], HIGH);
    delayMicroseconds(300);
    bool received[8];
    for (uint8_t rx = 0; rx < 8; rx++) {
      received[rx] = digitalRead(RX_PINS[rx]);
    }
    digitalWrite(TX_PINS[tx], LOW);

    int matches = 0;
    for (uint8_t rx = 0; rx < 8; rx++) if (received[rx]) matches++;
    if (matches == 0) {
      cableResult.status[tx] = 1;
      cableResult.brokenCount++;
    } else {
      for (uint8_t rx = 0; rx < 8; rx++) {
        if (received[rx]) cableResult.directMap[tx] = rx;
      }
      if (matches == 1 && cableResult.directMap[tx] != tx) {
        cableResult.status[tx] = 2;
        cableResult.crossedCount++;
      } else if (matches > 1) {
        cableResult.status[tx] = 3;
        cableResult.crossedCount++;
      }
    }
  }

  bool gigabitPoEReady = true;
  for (uint8_t i = 0; i < 8; i++) {
    if (i == 3 || i == 4 || i == 6 || i == 7) {
      if (cableResult.status[i] == 1 || cableResult.status[i] == 2 || cableResult.status[i] == 3)
        gigabitPoEReady = false;
    }
  }
  cableResult.gigabit = gigabitPoEReady;
}

void startTone() {
  ledcSetup(0, 800, 10);
  ledcAttachPin(TX_PINS[3], 0);
  toneActive = true;
  toneUntil = millis() + 30000;
  ledcWriteTone(0, 800);
}

void updateTone() {
  if (!toneActive) return;
  uint32_t now = millis();
  static uint32_t nextToneToggle = 0;
  if (now > toneUntil) {
    toneActive = false;
    ledcWriteTone(0, 0);
    ledcDetachPin(TX_PINS[3]);
  } else if (now >= nextToneToggle) {
    nextToneToggle = now + 500;
    uint32_t freq = ((now / 500) & 1) ? 800 : 1000;
    ledcWriteTone(0, freq);
  }
}

void startPortBlink() {
  portBlinkActive = true;
  portBlinkNext = millis();
  portBlinkLevel = true;
}

void updatePortBlink() {
  if (!portBlinkActive) return;
  uint32_t now = millis();
  if (now >= portBlinkNext) {
    portBlinkLevel = !portBlinkLevel;
    setAllTx(portBlinkLevel);
    portBlinkNext = now + 1200;
  }
}

bool detectSwitchPulse() {
  bool pulse = false;
  for (uint8_t i = 0; i < 8; i++) {
    if (digitalRead(RX_PINS[i]) == LOW) {
      pulse = true;
      break;
    }
  }
  if (pulse) switchLastPulse = millis();
  switchDetected = (millis() - switchLastPulse) < 4000;
  return switchDetected;
}

void updateOled() {
  uint32_t now = millis();
  if (now - lastOledUpdate < 500) return;
  lastOledUpdate = now;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("MEXWEB PRO v2.11");
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  display.setCursor(0, 14);
  display.print(WiFi.softAPgetStationNum());
  display.print(" clients ");
  display.print(WiFi.softAPIP().toString());

  display.setCursor(0, 28);
  display.print(switchDetected ? "SWITCH: LINK " : "SWITCH: ... ");

  display.setCursor(0, 40);
  display.print("Cable: ");
  if (cableResult.brokenCount) {
    display.print(cableResult.brokenCount);
    display.print(" roto");
  } else if (cableResult.crossedCount) {
    display.print(cableResult.crossedCount);
    display.print(" cruz");
  } else {
    display.print(cableResult.gigabit ? "Gigabit" : "Cable OK");
  }

  display.setCursor(0, 52);
  display.print(toneActive ? "TONO ACTIVO" : portBlinkActive ? "PORT BLINK" : "Directo/SVG");
  display.display();
}

String htmlPage() {
  String p;
  p.reserve(18000);
  p += F("<!doctype html><html><head><meta charset='utf-8'>");
  p += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  p += F("<title>Tester MexWeb PRO</title><style>");
  p += F("body{font-family:system-ui;margin:0;background:#0b1220;color:#e8eef8}");
  p += F("header{background:#087ea4;padding:14px;text-align:center}main{padding:12px}");
  p += F("nav{display:flex;gap:6px;flex-wrap:wrap}button,input,select{padding:9px;border-radius:7px;border:1px solid #46556a}");
  p += F("button{background:#087ea4;color:white}.card{background:#151f31;border-radius:10px;padding:12px;margin:10px 0}");
  p += F(".grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px}");
  p += F("svg text{fill:#fff;font-size:10px}.ok{stroke:#42d392}.bad{stroke:#ff5d5d}.cross{stroke:#ffad32}");
  p += F("table{border-collapse:collapse;width:100%}td,th{border:1px solid #46556a;padding:6px}");
  p += F("</style></head><body><header><h2>Tester MexWeb PRO v2.11</h2></header><main>");
  p += F("<nav><button onclick=\"api('/scan')\">Escaneo directo</button>");
  p += F("<button onclick=\"api('/sequential')\">Modo 1-8</button>");
  p += F("<button onclick=\"api('/tone')\">Tono 30 s</button>");
  p += F("<button onclick=\"api('/blink')\">Port Blink</button>");
  p += F("<button onclick=\"api('/switch')\">Detectar Switch</button>");
  p += F("<a href='/csv'><button>Descargar CSV</button></a></nav>");
  p += F("<div class='card'><h3>Mapeo de hilos</h3><svg id='map' width='100%' height='220' viewBox='0 0 500 220'></svg></div>");
  p += F("<div class='grid'><div class='card'><h3>Estado</h3><pre id='status'>Listo</pre></div>");
  p += F("<div class='card'><h3>Ping TCP</h3><input id='host' value='1.1.1.1'><input id='port' value='80' type='number'><br><br>");
  p += F("<button onclick=\"ping()\">Ping TCP</button><pre id='pingOut'></pre></div></div>");
  p += F("<div class='grid'><div class='card'><h3>Wi-Fi cercanas</h3><button onclick=\"scanWifi()\">Escanear</button><pre id='wifiOut'></pre></div>");
  p += F("<div class='card'><h3>Conectar STA</h3><input id='ssid' placeholder='SSID'><input id='pass' type='password' placeholder='Password'><br><br>");
  p += F("<button onclick=\"connectWifi()\">Conectar</button><pre id='staOut'></pre></div></div>");
  p += F("<div class='card'><h3>Calculadora IPv4 / CIDR</h3><input id='ip' value='192.168.1.10'><input id='cidr' value='24' type='number' min='1' max='32'> ");
  p += F("<button onclick=\"subnet()\">Calcular</button><pre id='subOut'></pre></div>");
  p += F("<div class='card'><h3>Manuales</h3>");
  p += F("<b>TIA-568B:</b> 1 W-O, 2 O, 3 W-G, 4 B, 5 W-B, 6 G, 7 W-Bn, 8 Bn<br>");
  p += F("TIA-568A: 1 W-G, 2 G, 3 W-O, 4 B, 5 W-B, 6 O, 7 W-Bn, 8 Bn<br>");
  p += F("Categorías: Cat5e (1Gb/s), Cat6 (1Gb/s/10Gb hasta 55m), Cat6A (10Gb/100m), Cat8 (25/40Gb). ");
  p += F("PoE: 802.3af 15.4W, 802.3at 30W, 802.3bt 60/90W. Regla: 100 m canal máximo.</div>");
  p += F("<script>");
  p += F("const colors=['#ff8d2a','#ff5d5d','#51b749','#1e3a8a','#68b1d8','#2d9c55','#a86b35','#70411c'];");
  p += F("function api(u){fetch(u).then(r=>r.json()).then(d=>{document.getElementById('status').textContent=JSON.stringify(d,null,2);draw(d.map||[])}).catch(e=>alert(e))}");
  p += F("function draw(m){const s=document.getElementById('map');s.innerHTML='';for(let i=0;i<8;i++){s.insertAdjacentHTML('beforeend',`<text x='15' y='${35+i*22}'>TX ${i+1}</text><text x='430' y='${35+i*22}'>RX ${i+1}</text>`);}");
  p += F("for(let i=0;i<m.length;i++){let r=Number(m[i]);let y1=30+i*22,y2=r>=0?30+r*22:30+i*22;let cls=r===i?'ok':r<0?'bad':'cross';let line=`<line class='${cls}' x1='90' y1='${y1}' x2='410' y2='${y2}' stroke-width='3'/>`;s.insertAdjacentHTML('beforeend',line);}");
  p += F("}");
  p += F("function ping(){fetch('/ping?host='+encodeURIComponent(document.getElementById('host').value)+'&port='+document.getElementById('port').value).then(r=>r.text()).then(t=>pingOut.textContent=t)}");
  p += F("function scanWifi(){wifiOut.textContent='Buscando...';fetch('/wifiscan').then(r=>r.text()).then(t=>wifiOut.textContent=t)}");
  p += F("function connectWifi(){fetch('/wificonnect?ssid='+encodeURIComponent(ssid.value)+'&pass='+encodeURIComponent(pass.value)).then(r=>r.text()).then(t=>staOut.textContent=t)}");
  p += F("function subnet(){fetch('/subnet?ip='+encodeURIComponent(ip.value)+'&cidr='+cidr.value).then(r=>r.text()).then(t=>subOut.textContent=t)}");
  p += F("api('/scan');</script></main></body></html>");
  return p;
}

String jsonStatus(const char* mode) {
  String j = "{";
  j += "\"mode\":\""; j += mode; j += "\",";
  j += "\"broken\":"; j += cableResult.brokenCount; j += ",";
  j += "\"crossed\":"; j += cableResult.crossedCount; j += ",";
  j += "\"gigabit\":"; j += cableResult.gigabit ? "true" : "false"; j += ",";
  j += "\"map\":[";
  for (uint8_t i = 0; i < 8; i++) {
    j += cableResult.directMap[i];
    if (i < 7) j += ",";
  }
  j += "]}";
  return j;
}

bool ipParts(String ip, uint32_t out[4]) {
  int last = -1;
  for (uint8_t i = 0; i < 4; i++) {
    int next = ip.indexOf('.', last + 1);
    if (i < 3 && next < 0) return false;
    if (i == 3) next = ip.length();
    String part = ip.substring(last + 1, next);
    out[i] = part.toInt();
    if (out[i] > 255) return false;
    last = next;
  }
  return true;
}

uint32_t ip32(uint32_t p[4]) {
  return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

void handleRoot() { server.send(200, "text/html", htmlPage()); }

void handleScan() {
  scanCable();
  server.send(200, "application/json", jsonStatus("direct"));
}

void handleSequential() {
  sequentialPin = (sequentialPin + 1) & 7;
  scanCable();
  String j = jsonStatus("sequential");
  j.remove(j.length() - 1);
  j += ",\"activePin\":"; j += (sequentialPin + 1); j += "}";
  server.send(200, "application/json", j);
}

void handleTone() {
  startTone();
  server.send(200, "application/json", "{\"mode\":\"tone\",\"seconds\":30}");
}

void handleBlink() {
  portBlinkActive = !portBlinkActive;
  if (portBlinkActive) startPortBlink();
  else setAllTx(LOW);
  server.send(200, "application/json", "{\"mode\":\"blink\",\"active\":" + String(portBlinkActive ? "true" : "false") + "}");
}

void handleSwitch() {
  switchDetected = detectSwitchPulse();
  server.send(200, "application/json", "{\"mode\":\"switch\",\"detected\":" + String(switchDetected ? "true" : "false") + "}");
}

void handlePing() {
  String host = server.arg("host");
  int port = server.arg("port").toInt();
  if (host.length() == 0) port = 0;
  if (port < 1 || port > 65535) port = 80;
  WiFiClient client;
  uint32_t start = millis();
  bool ok = client.connect(host.c_str(), port, 2000);
  uint32_t elapsed = millis() - start;
  String result = ok ? "TCP OK (" + String(elapsed) + " ms) " + host + ":" + String(port)
                     : "TCP TIMEOUT / unreachable " + host + ":" + String(port);
  client.stop();
  server.send(200, "text/plain", result);
}

void handleWifiScan() {
  int count = WiFi.scanNetworks();
  String out = "Redes Wi-Fi (" + String(count) + ")\n";
  for (int i = 0; i < count; i++) {
    out += WiFi.SSID(i) + "  " + String(WiFi.RSSI(i)) + " dBm  " +
           (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Secure") + "\n";
  }
  WiFi.scanDelete();
  server.send(200, "text/plain", out);
}

void handleWifiConnect() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  if (!ssid.length()) {
    server.send(400, "text/plain", "SSID requerido");
    return;
  }
  WiFi.begin(ssid.c_str(), pass.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(200);
  if (WiFi.status() == WL_CONNECTED) {
    String out = "Conectado\nIP: " + WiFi.localIP().toString();
    out += "\nMáscara: " + WiFi.subnetMask().toString();
    out += "\nGateway: " + WiFi.gatewayIP().toString();
    out += "\nDNS: " + WiFi.dnsIP().toString();
    server.send(200, "text/plain", out);
  } else {
    server.send(200, "text/plain", "No se pudo conectar");
  }
}

void handleSubnet() {
  uint32_t p[4];
  int cidr = server.arg("cidr").toInt();
  if (cidr < 1 || cidr > 32 || !ipParts(server.arg("ip"), p)) {
    server.send(400, "text/plain", "IP o prefijo invalido");
    return;
  }
  uint32_t ip = ip32(p);
  uint32_t mask = cidr == 0 ? 0 : 0xFFFFFFFF << (32 - cidr);
  uint32_t wildcard = ~mask;
  uint32_t network = ip & mask;
  uint32_t broadcast = network | wildcard;
  uint64_t total = 1ULL << (32 - cidr);
  uint64_t usable = cidr >= 31 ? (cidr == 31 ? 2 : 1) : total - 2;
  char firstLast[160];
  IPAddress nw(network);
  IPAddress bc(broadcast);
  IPAddress ma(mask);
  IPAddress wc(wildcard);
  snprintf(firstLast, sizeof(firstLast),
           "Red: %u.%u.%u.%u\nBroadcast: %u.%u.%u.%u\nMáscara: %u.%u.%u.%u\nWildcard: %u.%u.%u.%u\nHosts totales: %llu\nHosts utilizables: %llu",
           nw[0], nw[1], nw[2], nw[3], bc[0], bc[1], bc[2], bc[3],
           ma[0], ma[1], ma[2], ma[3], wc[0], wc[1], wc[2], wc[3],
           (unsigned long long)total, (unsigned long long)usable);
  server.send(200, "text/plain", firstLast);
}

void handleCsv() {
  String csv = "Pin,Estado,Mapeo TX-RX\n";
  for (uint8_t i = 0; i < 8; i++) {
    csv += String(i + 1) + ",";
    csv += cableResult.status[i] == 0 ? "OK" : cableResult.status[i] == 1 ? "ROTO" :
           cableResult.status[i] == 2 ? "CRUZADO" : "CORTOCIRCUITO";
    csv += "," + (cableResult.directMap[i] >= 0 ? String(cableResult.directMap[i] + 1) : "NINGUNO") + "\n";
  }
  server.sendHeader("Content-Disposition", "attachment; filename=network_tester.csv");
  server.send(200, "text/csv", csv);
}

void setup() {
  Serial.begin(115200);
  configurePins();
  memset(&cableResult, 0, sizeof(cableResult));
  for (uint8_t i = 0; i < 8; i++) cableResult.directMap[i] = -1;

  Wire.begin(21, 22);
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Tester MexWeb PRO");
    display.println("Iniciando...");
    display.display();
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(AP_HOSTNAME);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/sequential", HTTP_GET, handleSequential);
  server.on("/tone", HTTP_GET, handleTone);
  server.on("/blink", HTTP_GET, handleBlink);
  server.on("/switch", HTTP_GET, handleSwitch);
  server.on("/ping", HTTP_GET, handlePing);
  server.on("/wifiscan", HTTP_GET, handleWifiScan);
  server.on("/wificonnect", HTTP_GET, handleWifiConnect);
  server.on("/subnet", HTTP_GET, handleSubnet);
  server.on("/csv", HTTP_GET, handleCsv);
  server.onNotFound(handleRoot);
  server.begin();
  scanCable();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  updateTone();
  updatePortBlink();
  detectSwitchPulse();
  updateOled();
  delay(5);
}
