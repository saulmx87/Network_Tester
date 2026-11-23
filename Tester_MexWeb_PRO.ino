#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h> 

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- CONFIGURACIÓN DE RED (MODO DUAL: AP + STA) ---
const char* ssid = "NETWORK_TESTER";
const char* password = "12345678";

IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

const byte DNS_PORT = 53;
DNSServer dnsServer; 
WebServer server(80);

// --- PINES DEL TESTER ---
String colores[8] = {"B-Naranja", "Naranja", "B-Verde", "Azul", "B-Azul", "Verde", "B-Cafe", "Cafe"};
const int txPins[8] = {13, 14, 27, 26, 25, 33, 32, 23}; 
const int rxPins[8] = {4, 16, 17, 18, 19, 2, 5, 12};    

// --- MÁQUINA DE ESTADOS ---
int modoActual = 2; // 0=Espera, 1=Rápido, 2=Secuencial, 3=Tono, 5=DetectorSwitch, 6=PortBlink
int pasoSecuencia = 0;
unsigned long ultimoPaso = 0;
unsigned long tiempoInicioTono = 0;
unsigned long tiempoUltimoEnlace = 0; 

int webRxMap[8]; 
int webCalidad[8]; 
String webDiagnostico = "Iniciando...";
String webConsejo = "Esperando datos...";

// --- INTERFAZ WEB HTML/CSS ---
const char* paginaWeb = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Tester MexWeb PRO</title>
  <style>
    body { font-family: 'Segoe UI', sans-serif; background-color: #121212; color: #fff; text-align: center; margin: 0; padding: 10px; user-select: none; }
    h1 { color: #00ffcc; font-size: 20px; margin-bottom: 10px;}
    .card { background-color: #1e1e1e; border-radius: 10px; padding: 15px; margin-bottom: 15px; box-shadow: 0 4px 8px rgba(0,0,0,0.5); }
    .card-accion { background-color: #2a2a2a; border-left: 5px solid #00ffcc; border-radius: 5px; padding: 15px; margin-bottom: 15px; box-shadow: 0 4px 8px rgba(0,0,0,0.8); }
    
    .grafico-cables { position: relative; display: flex; justify-content: space-around; margin-top: 15px; margin-bottom: 15px; padding: 10px 0;}
    #lienzo-lineas { position: absolute; top: 0; left: 0; width: 100%; height: 100%; pointer-events: none; z-index: 1; }
    .columna { display: flex; flex-direction: column; gap: 12px; align-items: center; z-index: 2; }
    
    .btn-hilo { width: 35px; height: 35px; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-weight: bold; transition: 0.1s; border: 2px solid #222; box-shadow: 0 0 5px rgba(255,255,255,0.1);}
    
    .cb-0 { background: repeating-linear-gradient(45deg, #fff, #fff 5px, #ff8c00 5px, #ff8c00 10px); color: black; }
    .cb-1 { background: #ff8c00; color: black; }
    .cb-2 { background: repeating-linear-gradient(45deg, #fff, #fff 5px, #00a000 5px, #00a000 10px); color: black; }
    .cb-3 { background: #0055ff; color: white; }
    .cb-4 { background: repeating-linear-gradient(45deg, #fff, #fff 5px, #0055ff 5px, #0055ff 10px); color: black; }
    .cb-5 { background: #00a000; color: white; }
    .cb-6 { background: repeating-linear-gradient(45deg, #fff, #fff 5px, #8b4513 5px, #8b4513 10px); color: black; }
    .cb-7 { background: #8b4513; color: white; }

    .apagado { opacity: 1; } 
    .ok { background: #00ff00 !important; color: black !important; box-shadow: 0 0 15px #00ff00; border: none;}
    .fail { background: #ff3333 !important; color: white !important; box-shadow: 0 0 15px #ff3333; border: none;}
    .cross { background: #ff9900 !important; color: black !important; box-shadow: 0 0 15px #ff9900; border: none;}
    
    .btn { color: white; border: none; padding: 10px 15px; border-radius: 5px; cursor: pointer; font-size: 14px; width: 100%; margin-top: 8px; font-weight: bold;}
    .btn-rapido { background-color: #28a745; }
    .btn-seq { background-color: #9c27b0; }
    .btn-tono { background-color: #007bff; }
    .btn-stop { background-color: #dc3545; font-size: 16px; padding: 15px;}
    .btn-extra { background-color: #555; }
    .btn-guia { background-color: #17a2b8; }
    .btn-normas { background-color: #6f42c1; } 
    .btn-subnet { background-color: #e83e8c; }
    .btn-wifi { background-color: #ff9900; color: black;}
    .btn-switch { background-color: #17a2b8; }
    .btn-blink { background-color: #ffc107; color: black; } 
    .btn:hover { opacity: 0.8; }
    
    #alertas { color: #ff9900; font-weight: bold; margin-top: 10px; font-size: 13px; text-align: left; line-height: 1.5;}
    #consejo { color: #fff; font-size: 15px; font-weight: bold; text-align: left; margin-top: 5px; }
    .grid-botones { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }

    .seccion-oculta { display: none; text-align: left; font-size: 13px; color: #ddd; margin-top: 15px; padding-top: 15px; border-top: 1px solid #444;}
    
    .cb { display: inline-block; width: 14px; height: 14px; border-radius: 3px; margin-right: 8px; vertical-align: -2px; border: 1px solid #555;}
    .cb-bn { background: repeating-linear-gradient(45deg, #fff, #fff 3px, #ff8c00 3px, #ff8c00 6px); }
    .cb-n { background: #ff8c00; }
    .cb-bv { background: repeating-linear-gradient(45deg, #fff, #fff 3px, #00a000 3px, #00a000 6px); }
    .cb-v { background: #00a000; }
    .cb-ba { background: repeating-linear-gradient(45deg, #fff, #fff 3px, #0055ff 3px, #0055ff 6px); }
    .cb-a { background: #0055ff; }
    .cb-bc { background: repeating-linear-gradient(45deg, #fff, #fff 3px, #8b4513 3px, #8b4513 6px); }
    .cb-c { background: #8b4513; }
    
    .guia-tabla { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 15px; }
    .guia-titulo { color: #00ffcc; font-weight: bold; margin-bottom: 8px; border-bottom: 1px solid #00ffcc; padding-bottom: 3px;}
    
    table { width: 100%; border-collapse: collapse; margin-bottom: 15px; font-size: 13px; text-align: left; }
    th { color: #00ffcc; border-bottom: 1px solid #00ffcc; padding: 4px; }
    td { border-bottom: 1px solid #444; padding: 6px 4px; }
    
    input[type=text], input[type=number], input[type=password], select { background: #333; color: white; border: 1px solid #00ffcc; padding: 8px; width: 100%; box-sizing: border-box; margin-bottom: 10px; border-radius: 4px; font-weight: bold;}
    input:focus, select:focus { outline: none; box-shadow: 0 0 5px #00ffcc; }
  </style>
  <script>
    let nombresColores = ["B-Naranja", "Naranja", "B-Verde", "Azul", "B-Azul", "Verde", "B-Cafe", "Cafe"];
    
    setInterval(function() {
      fetch('/datos').then(response => response.json()).then(data => {
        let modoGlobal = data.modo;
        document.getElementById('diagnostico').innerText = data.diagnostico;
        document.getElementById('consejo').innerText = data.consejo;
        let alertasHTML = '';
        
        let svg = document.getElementById('lienzo-lineas');
        while (svg.firstChild) { svg.removeChild(svg.firstChild); } 
        
        if (modoGlobal == 0 || modoGlobal == 3 || modoGlobal == 5 || modoGlobal == 6) {
          for(let i=0; i<8; i++) {
            document.getElementById('tx'+i).className = 'btn-hilo cb-' + i + ' apagado';
            document.getElementById('rx'+i).className = 'btn-hilo cb-' + i + ' apagado';
          }
          if(modoGlobal == 3) document.getElementById('alertas').innerHTML = "🔊 Sirena activa en hilo Azul...";
          else if(modoGlobal == 5) document.getElementById('alertas').innerHTML = "🔍 Escuchando pulsos de red en el cable...";
          else if(modoGlobal == 6) document.getElementById('alertas').innerHTML = "💡 Obligando a parpadear el puerto del Switch...";
          else document.getElementById('alertas').innerHTML = "Modo de espera.";
          return;
        }

        let rectCont = document.getElementById('contenedor-grafico').getBoundingClientRect();

        if (modoGlobal == 1) { 
          for(let i=0; i<8; i++) {
            let rxVal = data.rx_map[i];
            let cal = data.calidad[i];
            
            document.getElementById('tx'+i).className = 'btn-hilo ' + ((rxVal == -1) ? 'fail' : 'ok');
            if(rxVal !== -1 && rxVal !== i) document.getElementById('tx'+i).className = 'btn-hilo cross';
            
            if(rxVal === -1) { alertasHTML += `🔴 ROTO: ${nombresColores[i]} sin conexión.<br>`; } 
            else if(rxVal !== i) { alertasHTML += `🟠 CRUZADO: ${nombresColores[i]} ➔ ${nombresColores[rxVal]}.<br>`; } 
            else { alertasHTML += `✅ ${nombresColores[i]}: Calidad ${cal}%<br>`; }

            if (rxVal !== -1 && rxVal !== -2) {
              let btnTx = document.getElementById('tx'+i); let btnRx = document.getElementById('rx'+rxVal);
              let rectTx = btnTx.getBoundingClientRect(); let rectRx = btnRx.getBoundingClientRect();
              let x1 = rectTx.left - rectCont.left + (rectTx.width / 2); let y1 = rectTx.top - rectCont.top + (rectTx.height / 2);
              let x2 = rectRx.left - rectCont.left + (rectRx.width / 2); let y2 = rectRx.top - rectCont.top + (rectRx.height / 2);
              let isCross = (rxVal !== i);
              let line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
              line.setAttribute('x1', x1); line.setAttribute('y1', y1); line.setAttribute('x2', x2); line.setAttribute('y2', y2);
              line.setAttribute('stroke', isCross ? '#ff9900' : '#00ff00'); line.setAttribute('stroke-width', isCross ? 3 : 2);
              if (isCross) line.setAttribute('stroke-dasharray', '5,5');
              svg.appendChild(line);
            }
          }
          for(let j=0; j<8; j++) {
            let estadoRX = 'fail';
            for(let k=0; k<8; k++) { if (data.rx_map[k] == j) estadoRX = (k == j) ? 'ok' : 'cross'; }
            document.getElementById('rx'+j).className = 'btn-hilo ' + estadoRX;
          }
        } 
        else if (modoGlobal == 2) { 
          let p = data.paso_actual;
          let rxDestino = data.rx_map[p];
          let cal = data.calidad[p];

          for(let i=0; i<8; i++) {
            document.getElementById('tx'+i).className = 'btn-hilo cb-' + i + ' apagado';
            document.getElementById('rx'+i).className = 'btn-hilo cb-' + i + ' apagado';
          }

          if (rxDestino == -1) {
            document.getElementById('tx'+p).className = 'btn-hilo fail'; document.getElementById('rx'+p).className = 'btn-hilo fail'; 
            alertasHTML = `🔴 ROTO: ${nombresColores[p]} está trozado.`;
          } else if (rxDestino != p) {
            document.getElementById('tx'+p).className = 'btn-hilo cross'; document.getElementById('rx'+rxDestino).className = 'btn-hilo cross'; 
            alertasHTML = `🟠 CRUZADO: ${nombresColores[p]} ➔ ${nombresColores[rxDestino]}.`;
          } else {
            document.getElementById('tx'+p).className = 'btn-hilo ok'; document.getElementById('rx'+p).className = 'btn-hilo ok'; 
            alertasHTML = `✅ ${nombresColores[p]} | Calidad: ${cal}%`;
          }
          
          if (rxDestino !== -1 && rxDestino !== -2) {
             let btnTx = document.getElementById('tx'+p); let btnRx = document.getElementById('rx'+rxDestino);
             let rectTx = btnTx.getBoundingClientRect(); let rectRx = btnRx.getBoundingClientRect();
             let x1 = rectTx.left - rectCont.left + (rectTx.width / 2); let y1 = rectTx.top - rectCont.top + (rectTx.height / 2);
             let x2 = rectRx.left - rectCont.left + (rectRx.width / 2); let y2 = rectRx.top - rectCont.top + (rectRx.height / 2);
             let isCross = (rxDestino !== p);
             let line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
             line.setAttribute('x1', x1); line.setAttribute('y1', y1); line.setAttribute('x2', x2); line.setAttribute('y2', y2);
             line.setAttribute('stroke', isCross ? '#ff9900' : '#00ff00'); line.setAttribute('stroke-width', isCross ? 4 : 3);
             if (isCross) line.setAttribute('stroke-dasharray', '5,5');
             svg.appendChild(line);
          }
        }
        document.getElementById('alertas').innerHTML = alertasHTML;
      });
    }, 250); 
    
    function mandarComando(cmd) { fetch('/cmd?accion=' + cmd); }
    function verReporte() { window.open('/reporte'); }
    function toggleSection(id) {
      let x = document.getElementById(id);
      x.style.display = (x.style.display === 'none' || x.style.display === '') ? 'block' : 'none';
    }

    // CALCULADORA DE SUBNETEO
    function calcularSubnet() {
      let pref = parseInt(document.getElementById("input-prefijo").value);
      if(isNaN(pref) || pref < 1 || pref > 32) {
        document.getElementById("res-mask").innerText = "-"; document.getElementById("res-wild").innerText = "-";
        document.getElementById("res-hosts").innerText = "-"; document.getElementById("res-usables").innerText = "-"; return;
      }
      let hosts = Math.pow(2, 32 - pref); let usables = (pref >= 31) ? 0 : hosts - 2;
      let mask = (0xFFFFFFFF << (32 - pref)) >>> 0;
      let m1 = (mask >>> 24) & 255; let m2 = (mask >>> 16) & 255; let m3 = (mask >>> 8) & 255; let m4 = mask & 255;
      let wild = (~mask) >>> 0; let w1 = (wild >>> 24) & 255; let w2 = (wild >>> 16) & 255; let w3 = (wild >>> 8) & 255; let w4 = wild & 255;
      
      document.getElementById("res-mask").innerText = `${m1}.${m2}.${m3}.${m4}`;
      document.getElementById("res-wild").innerText = `${w1}.${w2}.${w3}.${w4}`;
      document.getElementById("res-hosts").innerText = hosts.toLocaleString();
      document.getElementById("res-usables").innerText = usables.toLocaleString();
    }

    // FUNCIONES WI-FI Y PING
    function escanearRedes() {
      document.getElementById("btn-escanear").innerText = "⏳ Escaneando (3s)...";
      fetch('/scan_wifi').then(r => r.json()).then(data => {
        let sel = document.getElementById("select-wifi");
        sel.innerHTML = "";
        data.forEach(red => {
          let opt = document.createElement("option");
          opt.value = red.ssid; opt.text = red.ssid + " (" + red.rssi + "dBm)";
          sel.appendChild(opt);
        });
        document.getElementById("btn-escanear").innerText = "🔄 Rescanear";
      });
    }

    function conectarYPing() {
      let ssid = document.getElementById("select-wifi").value;
      let pass = document.getElementById("input-pass").value;
      let ip = document.getElementById("input-ip").value;
      if(!ssid || !ip) { alert("Selecciona una red y escribe una IP válida."); return; }
      
      let res = document.getElementById("res-ping");
      res.style.color = "#ff9900"; res.innerHTML = `Conectando a ${ssid} y probando ping (Max 4 segs)...`;
      
      fetch(`/conectar_ping?ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}&ip=${encodeURIComponent(ip)}`)
      .then(r => r.text()).then(texto => {
        if(texto.includes("Error") || texto.includes("rechazó") || texto.includes("no responde")) {
          res.style.color = "#ff3333";
        } else {
          res.style.color = "#00ff00";
        }
        res.innerHTML = texto;
      });
    }
  </script>
</head>
<body>
  <h1>Tester MexWeb V2.11 LITE</h1>
  
  <div class="card">
    <div id="diagnostico" style="font-weight: bold; font-size: 16px; margin-bottom: 5px;">Conectando...</div>
    <div class="grafico-cables" id="contenedor-grafico">
      <svg id="lienzo-lineas" xmlns="http://www.w3.org/2000/svg"></svg>
      <div class="columna"><div style="font-size:12px; color:#888;">TX</div><script>for(let i=0; i<8; i++) document.write(`<div id="tx${i}" class="btn-hilo cb-${i} apagado">${i+1}</div>`);</script></div>
      <div class="columna"><div style="font-size:12px; color:#888;">RX</div><script>for(let i=0; i<8; i++) document.write(`<div id="rx${i}" class="btn-hilo cb-${i} apagado">${i+1}</div>`);</script></div>
    </div>
    <div id="alertas"></div>
  </div>

  <div class="card-accion">
    <div style="font-size:12px; color:#aaa; text-align:left;">Sugerencia de Acción:</div>
    <div id="consejo">Cargando datos...</div>
  </div>

  <div class="card">
    <div class="grid-botones">
      <button class="btn btn-rapido" onclick="mandarComando('rapido')">Directo (Gráfico)</button>
      <button class="btn btn-seq" onclick="mandarComando('secuencial')">Secuencia 1-8</button>
    </div>
    
    <div class="grid-botones" style="margin-top: 8px;">
      <button class="btn btn-switch" onclick="mandarComando('switch')">🔌 Detectar Switch (Link)</button>
      <button class="btn btn-blink" onclick="mandarComando('blink')">💡 Parpadear LED Puerto</button>
    </div>

    <div class="grid-botones" style="margin-top: 8px;">
      <button class="btn btn-tono" onclick="mandarComando('tono')">Activar Sirena</button>
      <button class="btn btn-stop" onclick="mandarComando('apagar_tono')">APAGAR/PARO</button>
    </div>
  </div>

  <div class="card">
    <h3 style="margin-top:0; font-size: 15px;">Herramientas de Red</h3>
    
    <button class="btn btn-wifi" onclick="toggleSection('seccion-wifi'); escanearRedes();" style="margin-bottom: 8px;">📡 Conexión Wi-Fi y Ping TCP</button>
    <div id="seccion-wifi" class="seccion-oculta">
      <div class="guia-titulo">1. Conexión a Red Cliente</div>
      <button class="btn btn-extra" id="btn-escanear" onclick="escanearRedes()" style="margin-bottom:10px;">🔄 Buscar Redes</button>
      <select id="select-wifi"><option value="">Escaneando redes...</option></select>
      <input type="password" id="input-pass" placeholder="Contraseña de la red">
      <br><br>
      
      <div class="guia-titulo">2. Ping Rápido y DHCP Info</div>
      <input type="text" id="input-ip" placeholder="IP destino (Ej. 8.8.8.8 o 192.168.1.254)">
      <button class="btn btn-rapido" onclick="conectarYPing()">🔌 Conectar y Probar Ping</button>
      <div id="res-ping" style="margin-top:15px; font-size:14px; line-height: 1.4; text-align: left; background:#222; padding:10px; border-radius:5px;">
        Los datos de conexión aparecerán aquí.
      </div>
    </div>
    
    <button class="btn btn-subnet" onclick="toggleSection('seccion-subnet')" style="margin-bottom:8px;">🧮 Calculadora de Subneteo</button>
    <div id="seccion-subnet" class="seccion-oculta">
      <div class="guia-titulo">Calculadora IPv4</div>
      <div style="margin-bottom:12px; display:flex; align-items:center; justify-content:space-between;">
        <span style="font-size: 14px;">Prefijo CIDR: <b style="color:#00ffcc;">/</b></span>
        <input type="number" id="input-prefijo" min="1" max="32" placeholder="24" oninput="calcularSubnet()" style="width:70px;">
      </div>
      <table>
        <tr><td>Máscara:</td><td id="res-mask" style="color:#00ffcc; font-weight:bold;">-</td></tr>
        <tr><td>Wildcard (Inv):</td><td id="res-wild" style="color:#ff9900; font-weight:bold;">-</td></tr>
        <tr><td>Hosts Totales:</td><td id="res-hosts" style="color:#fff; font-weight:bold;">-</td></tr>
        <tr><td>Hosts Usables:</td><td id="res-usables" style="color:#00ff00; font-weight:bold;">-</td></tr>
      </table>
    </div>

    <div class="grid-botones">
      <button class="btn btn-guia" onclick="toggleSection('seccion-guia')">📖 Guía TIA-568</button>
      <button class="btn btn-normas" onclick="toggleSection('seccion-normas')">📚 Cat y PoE</button>
    </div>
    <button class="btn btn-extra" onclick="verReporte()" style="margin-top:8px;">Descargar Reporte CSV</button>

    <div id="seccion-guia" class="seccion-oculta">
      <div class="guia-tabla">
        <div>
          <div class="guia-titulo">T568B (Más Común)</div>
          <div><span class="cb cb-bn"></span>1. B-Naranja</div>
          <div><span class="cb cb-n"></span>2. Naranja</div>
          <div><span class="cb cb-bv"></span>3. B-Verde</div>
          <div><span class="cb cb-a"></span>4. Azul</div>
          <div><span class="cb cb-ba"></span>5. B-Azul</div>
          <div><span class="cb cb-v"></span>6. Verde</div>
          <div><span class="cb cb-bc"></span>7. B-Café</div>
          <div><span class="cb cb-c"></span>8. Café</div>
        </div>
        <div>
          <div class="guia-titulo">T568A</div>
          <div><span class="cb cb-bv"></span>1. B-Verde</div>
          <div><span class="cb cb-v"></span>2. Verde</div>
          <div><span class="cb cb-bn"></span>3. B-Naranja</div>
          <div><span class="cb cb-a"></span>4. Azul</div>
          <div><span class="cb cb-ba"></span>5. B-Azul</div>
          <div><span class="cb cb-n"></span>6. Naranja</div>
          <div><span class="cb cb-bc"></span>7. B-Café</div>
          <div><span class="cb cb-c"></span>8. Café</div>
        </div>
      </div>
      <div class="guia-titulo" style="margin-top:5px;">Tipos de Conexión</div>
      <div style="margin-bottom: 8px; line-height: 1.4;">
        <strong style="color: #00ff00;">Cable Directo (Straight-through):</strong><br>
        Para equipos diferentes (PC a Switch). Usa <b style="color:#00ffcc;">T568B</b> en ambos lados.
      </div>
      <div style="line-height: 1.4;">
        <strong style="color: #00ccff;">Cable Cruzado (Crossover):</strong><br>
        Para equipos iguales (PC a PC). Usa <b style="color:#00ffcc;">T568B</b> a <b style="color:#00ffcc;">T568A</b>.
      </div>
    </div>
    <div id="seccion-normas" class="seccion-oculta">
      <div class="guia-titulo">Categorías UTP y Distancias</div>
      <table>
        <tr><th>Cat</th><th>Velocidad</th><th>Frec.</th><th>Max Dist.</th></tr>
        <tr><td>5e</td><td>1 Gbps</td><td>100 MHz</td><td>100m</td></tr>
        <tr><td>6</td><td>1 Gbps<br><i style="font-size:10px;color:#aaa">10 Gbps</i></td><td>250 MHz</td><td>100m<br><i style="font-size:10px;color:#aaa">Hasta 55m</i></td></tr>
        <tr><td>6A</td><td>10 Gbps</td><td>500 MHz</td><td>100m</td></tr>
        <tr><td>8</td><td>25/40 Gbps</td><td>2000 MHz</td><td>30m</td></tr>
      </table>
      <div class="guia-titulo">Estándares PoE (Alimentación)</div>
      <table>
        <tr><th>Nombre</th><th>IEEE</th><th>Max. Watts</th></tr>
        <tr><td>PoE</td><td>802.3af</td><td>15.4 W</td></tr>
        <tr><td>PoE+</td><td>802.3at</td><td>30.0 W</td></tr>
        <tr><td>PoE++</td><td>802.3bt</td><td>60 W / 100 W</td></tr>
      </table>
      <div class="guia-titulo">Regla de los 100 Metros</div>
      <div style="margin-bottom: 8px; line-height: 1.4;">
        La norma limita el canal completo a <b>100 metros</b>:<br>
        • <b>90 Metros</b> (Max) para el Cableado Fijo Sólido (Permanent Link) en paredes.<br>
        • <b>10 Metros</b> (Max) en total para los cordones flexibles (Patch Cords) de los extremos.
      </div>
    </div>
  </div>
</body>
</html>
)rawliteral";

// --- FUNCIONES HARDWARE ---
void apagarTonoLimpiamente() {
  modoActual = 0; webDiagnostico = "En Espera...";
  ledcWriteTone(0, 0); ledcDetachPin(txPins[3]); 
  for (int i = 0; i < 8; i++) { pinMode(txPins[i], OUTPUT); digitalWrite(txPins[i], LOW); }
}

// --- ENDPOINTS DEL SERVIDOR WEB ---
void manejarRaiz() { server.send(200, "text/html", paginaWeb); }

void manejarDatos() {
  String json = "{\"rx_map\":[";
  for (int i = 0; i < 8; i++) { json += String(webRxMap[i]); if (i < 7) json += ","; }
  json += "], \"calidad\":[";
  for (int i = 0; i < 8; i++) { json += String(webCalidad[i]); if (i < 7) json += ","; }
  json += "], \"modo\": " + String(modoActual) + ", \"paso_actual\": " + String(pasoSecuencia);
  json += ", \"diagnostico\": \"" + webDiagnostico + "\", \"consejo\": \"" + webConsejo + "\"}";
  server.send(200, "application/json", json);
}

void manejarComando() {
  String accion = server.arg("accion");
  if (accion == "apagar_tono" || accion == "apagar") { apagarTonoLimpiamente(); } 
  else if (accion == "rapido") { apagarTonoLimpiamente(); modoActual = 1; } 
  else if (accion == "secuencial") { apagarTonoLimpiamente(); modoActual = 2; pasoSecuencia = 0; ultimoPaso = millis(); } 
  else if (accion == "tono") { apagarTonoLimpiamente(); modoActual = 3; tiempoInicioTono = millis(); ledcAttachPin(txPins[3], 0); webDiagnostico = "Emitiendo Tono Sirena..."; }
  else if (accion == "switch") { apagarTonoLimpiamente(); modoActual = 5; tiempoUltimoEnlace = 0; webDiagnostico = "Buscando Switch..."; }
  else if (accion == "blink") { apagarTonoLimpiamente(); modoActual = 6; webDiagnostico = "Port Blink Activo"; } 
  server.send(200, "text/plain", "Comando OK");
}

void manejarScanWifi() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; ++i) { json += "{\"ssid\":\"" + WiFi.SSID(i) + "\", \"rssi\":" + String(WiFi.RSSI(i)) + "}"; if (i < n - 1) json += ","; }
  json += "]";
  server.send(200, "application/json", json);
}

void manejarPingTCP() {
  String objSSID = server.arg("ssid");
  String objPass = server.arg("pass");
  String objIP = server.arg("ip");

  WiFi.begin(objSSID.c_str(), objPass.c_str());
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 8) { delay(500); intentos++; }

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    client.setTimeout(2);
    unsigned long t1 = millis();
    
    String netInfo = "<br><br><b style='color:#00ffcc;'>--- Datos Asignados (DHCP) ---</b><br>";
    netInfo += "<b>IP Local:</b> " + WiFi.localIP().toString() + "<br>";
    netInfo += "<b>Máscara:</b> " + WiFi.subnetMask().toString() + "<br>";
    netInfo += "<b>Puerta de Enlace:</b> " + WiFi.gatewayIP().toString() + "<br>";
    netInfo += "<b>DNS Primario:</b> " + WiFi.dnsIP().toString();

    if (client.connect(objIP.c_str(), 80) || client.connect(objIP.c_str(), 443) || client.connect(objIP.c_str(), 53)) {
      unsigned long pingTime = millis() - t1; client.stop();
      server.send(200, "text/plain", "✅ <b>ÉXITO. Enlace Activo.</b><br>Ping TCP IP Destino: " + objIP + "<br>Tiempo de respuesta: " + String(pingTime) + " ms" + netInfo);
    } else {
      server.send(200, "text/plain", "⚠️ <b>Conectado a Wi-Fi.</b> La IP destino " + objIP + " no responde." + netInfo);
    }
  } else { 
    server.send(200, "text/plain", "❌ <b>Error:</b> No se conectó a " + objSSID + ". Revisa la contraseña o cobertura."); 
  }
}

void manejarReporte() {
  String csv = "Pin TX,Color,Pin RX Destino,Estado,Calidad Ponchado (%)\n";
  for (int i = 0; i < 8; i++) {
    String estado = "OK";
    if (webRxMap[i] == -1) estado = "ROTO";
    else if (webRxMap[i] != i) estado = "CRUZADO";
    csv += String(i+1) + "," + colores[i] + "," + String(webRxMap[i]+1) + "," + estado + "," + String(webCalidad[i]) + "%\n";
  }
  server.sendHeader("Content-Disposition", "attachment; filename=\"Certificacion_MexWeb.csv\"");
  server.send(200, "text/csv", csv);
}

void setup() {
  Serial.begin(115200);
  ledcSetup(0, 1000, 8); 
  for (int i = 0; i < 8; i++) { pinMode(txPins[i], OUTPUT); digitalWrite(txPins[i], LOW); pinMode(rxPins[i], INPUT_PULLDOWN); }
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);
  
  dnsServer.start(DNS_PORT, "*", local_IP);

  display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(7, 10); display.println("MexWeb V2.11 PRO");
  display.setCursor(0, 25); display.println("Red Dual Creada OK!"); 
  display.setCursor(0, 40); display.print("IP:   "); display.println(WiFi.softAPIP());
  
  server.on("/", manejarRaiz);
  server.on("/datos", manejarDatos);
  server.on("/cmd", manejarComando);
  server.on("/scan_wifi", manejarScanWifi); 
  server.on("/conectar_ping", manejarPingTCP); 
  server.on("/reporte", manejarReporte);
  
  server.onNotFound([]() { server.sendHeader("Location", "http://192.168.4.1/", true); server.send(302, "text/plain", ""); });
  server.begin();
  display.display();
  delay(4000); 
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient(); 
  unsigned long tiempoActual = millis();

  // --- MODO 0: ESPERA ---
  if (modoActual == 0) {
    webConsejo = "Selecciona un modo de prueba en los botones de abajo.";
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(1, 0); display.println("TESTER MexWeb V2.11"); display.drawLine(0, 10, 128, 10, WHITE); 
    display.setCursor(0, 22); display.print("TX: 1 2 3 4 5 6 7 8");
    display.setCursor(0, 36); display.print("RX: 1 2 3 4 5 6 7 8");
    display.setCursor(0, 54); display.println("ESTATUS: EN ESPERA");
    display.display(); delay(100); return;
  }

  // --- MODO 6: PORT BLINK (TODOS LOS HILOS) ---
  if (modoActual == 6) {
    bool parpadeo = ((tiempoActual / 1200) % 2) == 0; 
    
    for (int i = 0; i < 8; i++) {
      digitalWrite(txPins[i], parpadeo ? HIGH : LOW);
    }
    
    webConsejo = "ESTATUS: 💡 Forzando Link-Up/Down en el puerto del Switch.";
    
    display.clearDisplay(); display.setCursor(1, 0); display.println("PORT BLINK ACTIVO"); display.drawLine(0, 10, 128, 10, WHITE); 
    display.setCursor(0, 30); display.println(parpadeo ? ">> TX ON (8 HILOS) <<" : "   TX OFF         ");
    display.display(); delay(100); return;
  }

  // --- MODO 5: DETECTOR DE SWITCH CON RETENCIÓN ---
  if (modoActual == 5) {
    int pulsosDetectados = 0;
    bool ultimoEstado = LOW;
    unsigned long startListen = millis();
    
    while (millis() - startListen < 800) {
      bool estadoActual = digitalRead(rxPins[0]) || digitalRead(rxPins[1]) || digitalRead(rxPins[2]) || digitalRead(rxPins[5]);
      if (estadoActual != ultimoEstado) { pulsosDetectados++; ultimoEstado = estadoActual; }
    }
    
    if (pulsosDetectados > 3) { tiempoUltimoEnlace = millis(); }
    
    if (millis() - tiempoUltimoEnlace < 4000 && tiempoUltimoEnlace != 0) {
      webDiagnostico = "Switch Detectado";
      webConsejo = "ESTATUS: ✅ Enlace Activo. Reteniendo señal de Switch.";
    } else {
      webDiagnostico = "Sin Enlace";
      webConsejo = "ESTATUS: ❌ Buscando señal... Cable suelto o equipo apagado.";
    }
    
    display.clearDisplay(); display.setCursor(1, 0); display.println("DETECTOR DE SWITCH"); display.drawLine(0, 10, 128, 10, WHITE); 
    display.setCursor(0, 30); display.println(webDiagnostico);
    display.display(); delay(10); return;
  }

  // --- MODO 3: TONO SIRENA ---
  if (modoActual == 3) {
    webConsejo = "ESTATUS: Sirena activa. Rastrea el cable con el pollo.";
    unsigned long tiempoSonando = millis() - tiempoInicioTono;
    if (tiempoSonando > 30000) { apagarTonoLimpiamente(); } 
    else {
      if ((tiempoSonando / 250) % 2 == 0) ledcWriteTone(0, 800); else ledcWriteTone(0, 1000);
      display.clearDisplay(); display.setCursor(1, 0); display.println("EMITIENDO TONO"); 
      display.setCursor(0, 30); display.println("Rastrea hilo AZUL");
      display.display(); delay(50);
    } return;
  }

  // --- MODO 1 y 2: ESCANEO Y CALIDAD ---
  int numMalos = 0; bool hayCruce = false;
  bool estadoParpadeo = ((tiempoActual / 250) % 2) == 0;

  if (modoActual == 1) { 
    for (int i = 0; i < 8; i++) {
      digitalWrite(txPins[i], HIGH); unsigned long t1 = micros(); webRxMap[i] = -1; 
      while(micros() - t1 < 1000) { for (int j = 0; j < 8; j++) { if (digitalRead(rxPins[j]) == HIGH) { webRxMap[i] = j; break; } } if(webRxMap[i] != -1) break; }
      unsigned long tiempoViaje = micros() - t1; digitalWrite(txPins[i], LOW);
      if (webRxMap[i] == -1) webCalidad[i] = 0; else if (tiempoViaje < 25) webCalidad[i] = 100; else webCalidad[i] = 70; 
      if (webRxMap[i] == -1) numMalos++; else if (webRxMap[i] != i) hayCruce = true;
    }
  } 
  else if (modoActual == 2) { 
    if (tiempoActual - ultimoPaso > 800) { pasoSecuencia++; if (pasoSecuencia > 7) pasoSecuencia = 0; ultimoPaso = tiempoActual; }
    digitalWrite(txPins[pasoSecuencia], HIGH); unsigned long t1 = micros(); webRxMap[pasoSecuencia] = -1;
    while(micros() - t1 < 1000) { for (int j = 0; j < 8; j++) { if (digitalRead(rxPins[j]) == HIGH) { webRxMap[pasoSecuencia] = j; break; } } if(webRxMap[pasoSecuencia] != -1) break; }
    unsigned long tiempoViaje = micros() - t1; digitalWrite(txPins[pasoSecuencia], LOW);
    if (webRxMap[pasoSecuencia] == -1) webCalidad[pasoSecuencia] = 0; else if (tiempoViaje < 25) webCalidad[pasoSecuencia] = 100; else webCalidad[pasoSecuencia] = 70;
    for (int i = 0; i < 8; i++) { if (i != pasoSecuencia) webRxMap[i] = -2; } 
    if (webRxMap[pasoSecuencia] == -1) numMalos++; else if (webRxMap[pasoSecuencia] != pasoSecuencia) hayCruce = true;
  }

  webConsejo = "";
  if (numMalos == 8 && !hayCruce) { 
    webConsejo = "Conecta el cable en ambos puertos (TX y RX) para iniciar el diagnóstico."; 
  } else if (numMalos == 0 && !hayCruce) { 
    webConsejo = "ESTATUS: Transmitiendo 100%. Cable certificado para GIGABIT (1000 Mbps)."; 
  } else if (webRxMap[0]==0 && webRxMap[1]==1 && webRxMap[2]==2 && webRxMap[5]==5 && !hayCruce) {
    webConsejo = "ALERTA: Hilos PoE/Gbit rotos. El cable SOLO funcionará a 10/100 Mbps (Fast Ethernet).";
  } else {
    webConsejo = "ACCIÓN: REVISAR ";
    bool primero = true;
    for (int i=0; i<8; i++) {
      if (webRxMap[i] == -1 || (webRxMap[i] != i && webRxMap[i] != -2)) {
        if (!primero) webConsejo += ", "; webConsejo += colores[i]; primero = false;
      }
    } webConsejo += ".";
  }

  // --- DIBUJAR PANTALLA OLED COMPLETA ---
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE, BLACK); 
  display.setCursor(1, 0); display.println(modoActual == 1 ? "Prueba Rapida" : "Prueba Secuencial"); 
  display.drawLine(0, 10, 128, 10, WHITE); 
  
  display.setCursor(0, 22); display.print("TX: ");
  for (int i=0; i<8; i++) {
    if ((modoActual == 1 && webRxMap[i] == -1) || (modoActual == 2 && webRxMap[pasoSecuencia] == -1 && i == pasoSecuencia)) {
      if(estadoParpadeo) display.print("X "); else display.print("  ");
    } else {
      if (modoActual == 2 && i == pasoSecuencia) { display.setTextColor(BLACK, WHITE); display.print(i+1); display.setTextColor(WHITE, BLACK); } else display.print(i+1); display.print(" ");
    }
  }

  display.setCursor(0, 36); display.print("RX: ");
  for (int j=0; j<8; j++) {
    bool rxRecibe = false; for(int k=0; k<8; k++) { if(webRxMap[k] == j) rxRecibe = true; }
    if ((modoActual == 1 && !rxRecibe) || (modoActual == 2 && webRxMap[pasoSecuencia] == -1 && j == pasoSecuencia)) {
      if(estadoParpadeo) display.print("X "); else display.print("  ");
    } else {
      if (modoActual == 2 && webRxMap[pasoSecuencia] == j) { display.setTextColor(BLACK, WHITE); display.print(j+1); display.setTextColor(WHITE, BLACK); } else display.print(j+1); display.print(" ");
    }
  }

  display.setCursor(0, 54);
  if (modoActual == 1) { 
    if (numMalos == 8) { display.println("CABLE DESCONECTADO"); }
    else if (numMalos == 0 && !hayCruce) { display.println("ESTATUS: GIGABIT OK"); }
    else if (webRxMap[0]==0 && webRxMap[1]==1 && webRxMap[2]==2 && webRxMap[5]==5 && !hayCruce) { display.println("ESTATUS: SOLO 10/100"); }
    else if (numMalos > 0) { display.println("ESTATUS: CABLE ROTO"); }
    else if (hayCruce) { display.println("ESTATUS: CRUZADO"); }
    else { display.println("ESTATUS: FALLA"); }
  } else { display.print("ESTATUS: Checando "); display.println(pasoSecuencia + 1); }

  display.display(); delay(15); 
}
