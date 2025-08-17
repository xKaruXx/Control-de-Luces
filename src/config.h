#ifndef CONFIG_H
#define CONFIG_H

// =============================
// CONFIGURACIÓN WiFi
// =============================
#define WIFI_SSID "TU_SSID"
#define WIFI_PASSWORD "TU_PASSWORD"
#define WIFI_CONNECT_TIMEOUT 20000  // Timeout conexión WiFi en ms
#define WIFI_RECONNECT_INTERVAL 30000  // Intervalo de reconexión en ms

// =============================
// CONFIGURACIÓN SERVIDOR WEB
// =============================
#define WEB_SERVER_PORT 80
#define JSON_BUFFER_SIZE 2048
#define MAX_LUCES 100  // Máximo número de luminarias

// =============================
// CONFIGURACIÓN SERIAL
// =============================
#define SERIAL_BAUD_RATE 115200
#define DEBUG_MODE true  // Activar/desactivar mensajes debug

// =============================
// CONFIGURACIÓN DE RED
// =============================
#define HOSTNAME "ESP8266-ControlLuces"
#define MDNS_NAME "luces"  // Acceso via http://luces.local

// =============================
// CONFIGURACIÓN DE ACTUALIZACIONES
// =============================
#define UPDATE_INTERVAL 5000  // Intervalo actualización en ms
#define HEARTBEAT_INTERVAL 60000  // Heartbeat cada minuto

// =============================
// CONFIGURACIÓN DE LUCES
// =============================
#define DEFAULT_LAT -33.301726
#define DEFAULT_LNG -66.337752
#define DEFAULT_ZOOM 13

// =============================
// CONFIGURACIÓN DE SEGURIDAD
// =============================
#define ENABLE_AUTH false  // Activar autenticación (futuro)
#define API_KEY "cambiar_clave_secreta"  // API Key para requests
#define SESSION_TIMEOUT 1800000  // 30 minutos

// =============================
// CONFIGURACIÓN DE LOGS
// =============================
#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_INFO 3
#define LOG_LEVEL_DEBUG 4
#define CURRENT_LOG_LEVEL LOG_LEVEL_INFO

// =============================
// CONFIGURACIÓN DE MEMORIA
// =============================
#define WATCHDOG_TIMEOUT 8000  // Watchdog timeout en ms
#define FREE_HEAP_THRESHOLD 10000  // Umbral mínimo de heap libre

// =============================
// CONFIGURACIÓN OTA (Over-The-Air)
// =============================
#define ENABLE_OTA false
#define OTA_PASSWORD "ota_password"
#define OTA_PORT 8266

// =============================
// PINES GPIO (si se usan)
// =============================
#define LED_STATUS_PIN LED_BUILTIN
#define BUTTON_RESET_PIN D3

// =============================
// MACROS DE DEBUG
// =============================
#if DEBUG_MODE
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(x, ...) Serial.printf(x, __VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x, ...)
#endif

// =============================
// MACROS DE LOG
// =============================
#define LOG_ERROR(msg) if(CURRENT_LOG_LEVEL >= LOG_LEVEL_ERROR) { Serial.print("[ERROR] "); Serial.println(msg); }
#define LOG_WARNING(msg) if(CURRENT_LOG_LEVEL >= LOG_LEVEL_WARNING) { Serial.print("[WARN] "); Serial.println(msg); }
#define LOG_INFO(msg) if(CURRENT_LOG_LEVEL >= LOG_LEVEL_INFO) { Serial.print("[INFO] "); Serial.println(msg); }
#define LOG_DEBUG(msg) if(CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG) { Serial.print("[DEBUG] "); Serial.println(msg); }

// =============================
// CONFIGURACIÓN MQTT
// =============================
#define MQTT_BROKER_IP "192.168.1.100"  // IP del broker MQTT (puede ser local o externo)
#define MQTT_BROKER_PORT 1883
#define MQTT_USER ""  // Usuario MQTT (opcional)
#define MQTT_PASSWORD ""  // Contraseña MQTT (opcional)
#define MQTT_ENABLE true  // Habilitar/deshabilitar MQTT

// =============================
// VERSIÓN DEL FIRMWARE
// =============================
#define FIRMWARE_VERSION "0.7.0"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

#endif // CONFIG_H