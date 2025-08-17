# Sistema de Control de Alumbrado Público - San Luis

## Descripción
Sistema IoT para monitoreo y control de luminarias públicas en la ciudad de San Luis, Argentina. Utiliza ESP8266 como controlador central y proporciona una interfaz web para visualización en tiempo real del estado de las luminarias.

## Características Actuales
- ✅ Servidor web asíncrono en ESP8266
- ✅ Visualización de luminarias en mapa interactivo (Leaflet)
- ✅ Actualización en tiempo real del estado (encendida/apagada/falla)
- ✅ Panel de administración con AdminLTE
- ✅ API REST básica para gestión de luminarias

## Tecnologías
- **Hardware**: ESP8266 (NodeMCU/Wemos D1)
- **Firmware**: C++ con Arduino Framework
- **Frontend**: HTML5, JavaScript, Leaflet.js, AdminLTE
- **Comunicación**: HTTP/REST API
- **Almacenamiento**: SPIFFS para archivos estáticos

## Estructura del Proyecto
```
Control-de-Luces/
├── src/                    # Código fuente C++
│   ├── main.cpp           # Archivo principal
│   └── config.h           # Configuración del sistema
├── data/                   # Archivos SPIFFS (HTML/CSS/JS)
│   ├── index.html         # Interfaz principal
│   └── muestra_nodos.html # Vista de nodos
├── lib/                    # Librerías locales
├── docs/                   # Documentación
├── platformio.ini          # Configuración PlatformIO
├── CHANGELOG.md           # Historial de cambios
├── ROADMAP.md             # Plan de desarrollo
└── README.md              # Este archivo
```

## Requisitos
- PlatformIO IDE o Arduino IDE
- ESP8266 Board Package
- Librerías requeridas:
  - ESPAsyncTCP
  - ESPAsyncWebServer
  - ArduinoJson
  - ESP8266WiFi

## Instalación

### Usando PlatformIO (Recomendado)
1. Clonar el repositorio:
   ```bash
   git clone https://github.com/tu-usuario/Control-de-Luces.git
   cd Control-de-Luces
   ```

2. Abrir con PlatformIO y compilar:
   ```bash
   pio run
   ```

3. Subir filesystem:
   ```bash
   pio run --target uploadfs
   ```

4. Subir firmware:
   ```bash
   pio run --target upload
   ```

### Configuración WiFi
Editar `src/config.h` con tus credenciales:
```cpp
#define WIFI_SSID "tu_red_wifi"
#define WIFI_PASSWORD "tu_password"
```

## Uso
1. Alimentar el ESP8266
2. El dispositivo se conectará a la red WiFi configurada
3. Verificar IP asignada en el monitor serial
4. Acceder desde navegador: `http://[IP_ESP8266]`

## API Endpoints

| Método | Endpoint | Descripción |
|--------|----------|-------------|
| GET | `/` | Interfaz web principal |
| GET | `/estado-luces` | Obtener estado de todas las luces (JSON) |
| POST | `/actualizar-luz` | Actualizar estado de una luz |

### Formato JSON para actualizar luz:
```json
{
  "lat": -33.301726,
  "lng": -66.337752,
  "estado": "encendida"
}
```

## Estados de Luminarias
- `encendida` - Luz funcionando normalmente
- `apagada` - Luz apagada (manual o programada)
- `falla` - Luz con problemas detectados

## Desarrollo

### Compilar y subir
```bash
# Compilar
pio run

# Subir firmware
pio run -t upload

# Subir archivos SPIFFS
pio run -t uploadfs

# Monitor serial
pio device monitor
```

## Contribuir
Las contribuciones son bienvenidas. Por favor:
1. Fork el proyecto
2. Crea una rama para tu feature (`git checkout -b feature/NuevaCaracteristica`)
3. Commit tus cambios (`git commit -m 'Agregar nueva característica'`)
4. Push a la rama (`git push origin feature/NuevaCaracteristica`)
5. Abre un Pull Request

## Licencia
MIT License - Ver LICENSE para más detalles

## Autor
Sistema desarrollado para la Municipalidad de San Luis

## Soporte
Para reportar problemas o sugerencias, crear un issue en GitHub.