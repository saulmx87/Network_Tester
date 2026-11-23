# Tester MexWeb PRO (v2.11)

Probador de cableado estructurado UTP/RJ45 y herramienta portátil de red para ESP32 con pantalla OLED SSD1306.

## Características

- Escaneo directo de 8 hilos con mapa SVG, detección de cortes/cruces y reporte CSV.
- Clasificación Gigabit y 10/100 según disponibilidad de los pares de Gigabit.
- Modo secuencial 1-8 con colores TIA-568B.
- Generador de tono alternante de 800/1000 Hz por 30 segundos en el pin 4 TX.
- Detector de pulsos de enlace y Port Blink cada 1.2 segundos.
- Portal cautivo AP (`NETWORK_TESTER`) y operación simultánea AP + STA.
- Escaneo de redes, conexión STA y datos DHCP.
- Ping TCP con timeout de 2 segundos.
- Calculadora IPv4 CIDR `/1`-`/32`.
- Manuales TIA-568A/B, categorías, PoE y distancia máxima.

## Hardware

| OLED | ESP32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

TX RJ45: GPIO 13, 14, 27, 26, 25, 33, 32, 23.

RX RJ45: GPIO 4, 16, 17, 18, 19, 2, 5, 12.

> No conectes puertos PoE activos ni tensiones superiores a 3.3V al ESP32. Desconecta los cables UTP antes de encender o cargar el firmware por USB.

## Compilación

En Arduino IDE, instala el ESP32 board package 2.0.x (2.0.17 recomendado) y las librerías `Adafruit GFX Library` y `Adafruit SSD1306`.

Abre `Network_Tester.ino`, selecciona **DOIT ESP32 DEVKIT V1** o **ESP32 Dev Module** y carga el firmware.

Conéctate a `NETWORK_TESTER` (contraseña: `12345678`) y abre `http://192.168.4.1`.

## Uso rápido

- Conecta TX y RX en ambos extremos del cable para continuidad.
- Conecta solo RX para detectar un switch.
- Conecta solo TX para tono o Port Blink.
