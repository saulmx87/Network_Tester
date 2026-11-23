# 🔌 Tester MexWeb PRO (v2.11)

![ESP32](https://img.shields.io/badge/Hardware-ESP32-blue)
![Framework](https://img.shields.io/badge/IDE-Arduino-00979D)
![Version](https://img.shields.io/badge/Version-2.11-brightgreen)
![License](https://img.shields.io/badge/License-MIT-yellow)

**Tester MexWeb PRO** es un probador portátil de cableado estructurado (UTP/RJ45) y suite de diagnóstico de red para técnicos e ingenieros de campo, desarrollado sobre un **ESP32** y una pantalla **OLED de 0.96"** sin módulos Ethernet externos.

El equipo levanta su propio Punto de Acceso Wi-Fi con **Portal Cautivo** para visualizar el mapeo vectorial de pines (SVG) en tiempo real desde cualquier celular o laptop, e integra herramientas de red y manuales técnicos 100% offline.

---

##  Características Principales

###  Diagnóstico Físico de Cableado (UTP / RJ45)
* **Prueba Directa (Rápida):** Escanea los 8 hilos simultáneamente, dibuja conexiones y cruces con líneas vectoriales (SVG) y estima la calidad del ponchado.
* **Certificación de Capacidad (Gigabit vs 10/100):** Determina si el cable soporta **1000 Mbps (Gigabit)** o alerta si los pares de PoE/Gigabit (4, 5, 7, 8) presentan falla mientras los pares de datos (1, 2, 3, 6) permiten enlace a **10/100 Mbps**.
* **Modo Secuencial (1–8):** Muestra los colores reales de la norma TIA-568B en reposo y cambia dinámicamente a **Verde** (OK), **Rojo** (Roto) o **Naranja** (Cruzado) al paso del pulso.
* **Generador de Tono (Sirena):** Emite un tono alternante (800 Hz / 1000 Hz) en el hilo 4 (Azul) durante 30 segundos para rastreo con sonda inductiva.
* **Detector de Switch (Link Pulses):** Escucha pasivamente pulsos eléctricos (*Fast Link Pulses*) en el puerto RX con retención de 4 segundos para confirmar si el cable llega a un puerto activo.
* **Port Blink:** Inyecta pulsos rítmicos cada 1.2 segundos por los 8 hilos para provocar el parpadeo del LED en el puerto del switch y ubicar el cable en el rack.
* **Reporte CSV:** Genera y descarga el diagnóstico de los 8 pines en formato `.csv`.

###  Herramientas de Red y Utilidades Offline
* **Portal Cautivo Automático:** Al conectarse al Wi-Fi del tester, la interfaz web se abre automáticamente sin escribir la dirección IP.
* **Modo Dual Wi-Fi (`AP + STA`):** Escanea redes cercanas, se conecta temporalmente a la red del cliente sin desconectar tu celular del tester, ejecuta un **Ping TCP rápido** (timeout de 2 segundos) y muestra los parámetros entregados por **DHCP** (IP local, Máscara de Subred, Puerta de Enlace y DNS primario).
* **Calculadora de Subneteo IPv4:** Calcula máscara, wildcard, hosts totales y hosts usables a partir del prefijo CIDR (`/1` a `/32`).
* **Manuales de Campo Integrados:** Guía visual a color **TIA-568A / TIA-568B**, tabla de categorías (**Cat5e, Cat6, Cat6A, Cat8**), estándares **PoE (802.3af / 802.3at / 802.3bt)** y regla de los 100 metros.

---

##  Lista de Materiales (BOM)

| Cantidad | Componente | Especificación |
| :---: | :--- | :--- |
| 1 | Microcontrolador ESP32 | DevKit V1 (30 o 38 pines) |
| 1 | Pantalla OLED 0.96" | Controlador SSD1306, I2C, 128×64, dirección `0x3C` |
| 2 | Conectores RJ45 Hembra | PCB o Keystone (1 para TX, 1 para RX) |
| 1 | Alimentación | Powerbank 5V o batería 18650 con módulo TP4056 |
| *Opcional* | Resistencias 220Ω – 330Ω (×8) | En serie en los pines TX como protección contra cortocircuitos |

---

##  Diagrama de Conexiones (Pinout)

### 1. Pantalla OLED 0.96" (I2C)
| Pin OLED | Pin ESP32 |
| :--- | :--- |
| **VCC** | `3.3V` |
| **GND** | `GND` |
| **SDA** | `GPIO 21` |
| **SCL** | `GPIO 22` |

### 2. Puerto RJ45 — Transmisor (`TX`)
| Pin RJ45 | Color (T568B) | Pin ESP32 |
| :---: | :--- | :---: |
| **1** | Blanco-Naranja | `GPIO 13` |
| **2** | Naranja | `GPIO 14` |
| **3** | Blanco-Verde | `GPIO 27` |
| **4** | Azul *(Salida de Tono)* | `GPIO 26` |
| **5** | Blanco-Azul | `GPIO 25` |
| **6** | Verde | `GPIO 33` |
| **7** | Blanco-Café | `GPIO 32` |
| **8** | Café | `GPIO 23` |

### 3. Puerto RJ45 — Receptor (`RX`)
| Pin RJ45 | Color (T568B) | Pin ESP32 |
| :---: | :--- | :---: |
| **1** | Blanco-Naranja | `GPIO 4` |
| **2** | Naranja | `GPIO 16` |
| **3** | Blanco-Verde | `GPIO 17` |
| **4** | Azul | `GPIO 18` |
| **5** | Blanco-Azul | `GPIO 19` |
| **6** | Verde | `GPIO 2` |
| **7** | Blanco-Café | `GPIO 5` |
| **8** | Café | `GPIO 12` |

> ** Advertencias de Hardware**
> * **NO conectar a puertos con PoE activo (24V / 48V):** Los pines GPIO del ESP32 soportan un máximo de **3.3V**. Conectarlo directamente a una línea con alimentación PoE dañará permanentemente la placa.
> * **Pines de arranque (*Strapping Pins*):** `GPIO 2` y `GPIO 12` (asignados a RX) intervienen en el inicio del ESP32. **Desconecta cualquier cable UTP** antes de encender el equipo o flashear el firmware por USB.

---

##  Instalación y Carga del Firmware

### Requisitos en Arduino IDE
1. **Gestor de Tarjetas:** Instalar `esp32` by Espressif Systems en versión **2.0.x** (recomendado `2.0.17` por compatibilidad con `ledcSetup` y `ledcAttachPin`).
2. **Librerías externas (Gestor de Librerías):**
   * `Adafruit GFX Library`
   * `Adafruit SSD1306`
3. **Librerías incluidas en el core ESP32:** `Wire.h`, `WiFi.h`, `WebServer.h`, `DNSServer.h`.

### Configuración de Credenciales
Antes de compilar, verifica las credenciales del Punto de Acceso al inicio del archivo `.ino`:

```cpp
const char* ssid = "NETWORK_TESTER";
const char* password = "12345678";
