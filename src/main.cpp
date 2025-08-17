// =============================
// Sistema de Control de Alumbrado Público
// Nodo Central - ESP8266
// Versión: 0.5.0 - Fase 3: Características Avanzadas
// =============================

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include "config.h"
#include "Logger.h"
#include "MemoryManager.h"
#include "WifiManager.h"
#include "AuthManager.h"
#include "SecurityManager.h"
#include "DatabaseManager.h"
#include "ScheduleManager.h"
#include "AlertManager.h"

// =============================
// VARIABLES GLOBALES
// =============================
AsyncWebServer server(WEB_SERVER_PORT);
Ticker heartbeatTimer;
Ticker sessionCheckTimer;
unsigned long lastUpdate = 0;

// Estructura para datos de luminaria
struct Luminaria {
  float lat;
  float lng;
  String estado;  // encendida, apagada, falla
  uint32_t ultimaActualizacion;
  uint8_t intensidad;  // 0-100% para futuro dimming
  String id;  // ID único de la luminaria
};

std::vector<Luminaria> luminarias;

// =============================
// DECLARACIÓN DE FUNCIONES
// =============================
void setupSerial();
void setupWebServer();
void setupFileSystem();
void setupMDNS();
void setupSecurity();
void sendHeartbeat();
void checkSessions();
String getLuminariasJson();
void actualizarLuminaria(float lat, float lng, String estado);
String getSystemInfo();
String getClientIdentifier(AsyncWebServerRequest *request);

// =============================
// FUNCIONES DE UTILIDAD
// =============================
String getClientIdentifier(AsyncWebServerRequest *request) {
  // Usar IP como identificador principal
  return request->client()->remoteIP().toString();
}

String getSystemInfo() {
  StaticJsonDocument<512> doc;
  doc["version"] = FIRMWARE_VERSION;
  doc["build_date"] = BUILD_DATE;
  doc["build_time"] = BUILD_TIME;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["chip_id"] = ESP.getChipId();
  doc["flash_size"] = ESP.getFlashChipSize();
  doc["uptime"] = millis() / 1000;
  doc["wifi_status"] = WiFiMgr.isConnected() ? "connected" : "disconnected";
  doc["ip"] = WiFiMgr.getIP();
  doc["rssi"] = WiFiMgr.getRSSI();
  doc["luminarias_count"] = luminarias.size();
  doc["sessions_active"] = Auth.getActiveSessionCount();
  doc["security_enabled"] = true;
  
  String result;
  serializeJson(doc, result);
  return result;
}

String getLuminariasJson() {
  StaticJsonDocument<JSON_BUFFER_SIZE> doc;
  JsonArray arr = doc.to<JsonArray>();

  for (auto &luz : luminarias) {
    if (arr.size() >= MAX_LUCES) break;
    
    JsonObject obj = arr.createNestedObject();
    obj["id"] = luz.id;
    obj["lat"] = luz.lat;
    obj["lng"] = luz.lng;
    obj["estado"] = luz.estado;
    obj["intensidad"] = luz.intensidad;
    obj["ultima_actualizacion"] = luz.ultimaActualizacion;
  }

  String result;
  serializeJson(doc, result);
  return result;
}

String generateLuminariaId(float lat, float lng) {
  char id[32];
  snprintf(id, sizeof(id), "LUM_%08X", (uint32_t)(lat * 1000000) ^ (uint32_t)(lng * 1000000));
  return String(id);
}

void actualizarLuminaria(float lat, float lng, String estado) {
  // Validar entrada
  if (!Security.validateInput(estado, INPUT_TYPE_ALPHANUM)) {
    SystemLogger.warning("Estado de luminaria inválido: " + estado, "LUCES");
    return;
  }
  
  bool actualizada = false;
  String id = generateLuminariaId(lat, lng);
  
  for (auto &luz : luminarias) {
    if (luz.id == id) {
      luz.estado = estado;
      luz.ultimaActualizacion = millis();
      actualizada = true;
      SystemLogger.info("Luminaria actualizada: " + id + " - " + estado, "LUCES");
      break;
    }
  }
  
  if (!actualizada && luminarias.size() < MAX_LUCES) {
    if (!MemManager.requestMemory(sizeof(Luminaria))) {
      SystemLogger.error("No hay memoria para agregar nueva luminaria", "LUCES");
      return;
    }
    
    Luminaria nuevaLuz;
    nuevaLuz.id = id;
    nuevaLuz.lat = lat;
    nuevaLuz.lng = lng;
    nuevaLuz.estado = estado;
    nuevaLuz.ultimaActualizacion = millis();
    nuevaLuz.intensidad = 100;
    luminarias.push_back(nuevaLuz);
    SystemLogger.info("Nueva luminaria agregada: " + id, "LUCES");
  }
}

// =============================
// CONFIGURACIÓN SERIAL
// =============================
void setupSerial() {
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println();
  Serial.println("========================================");
  Serial.println("Sistema de Control de Alumbrado Público");
  Serial.println("Versión: " + String(FIRMWARE_VERSION));
  Serial.println("Build: " + String(BUILD_DATE) + " " + String(BUILD_TIME));
  Serial.println("========================================");
}

// =============================
// CONFIGURACIÓN SISTEMA DE ARCHIVOS
// =============================
void setupFileSystem() {
  if (!LittleFS.begin()) {
    SystemLogger.error("Error al montar LittleFS", "SYSTEM");
    SystemLogger.warning("Formateando LittleFS...", "SYSTEM");
    LittleFS.format();
    if (!LittleFS.begin()) {
      SystemLogger.error("Error crítico con LittleFS", "SYSTEM");
      return;
    }
  }
  SystemLogger.info("Sistema de archivos LittleFS montado", "SYSTEM");
}

// =============================
// CONFIGURACIÓN mDNS
// =============================
void setupMDNS() {
  if (MDNS.begin(MDNS_NAME)) {
    MDNS.addService("http", "tcp", WEB_SERVER_PORT);
    SystemLogger.info("mDNS iniciado: http://" + String(MDNS_NAME) + ".local", "SYSTEM");
  } else {
    SystemLogger.warning("Error al iniciar mDNS", "SYSTEM");
  }
}

// =============================
// CONFIGURACIÓN SEGURIDAD
// =============================
void setupSecurity() {
  // Inicializar AuthManager
  Auth.begin();
  
  // Inicializar SecurityManager
  Security.begin();
  
  // Programar chequeo de sesiones
  sessionCheckTimer.attach(30, checkSessions);  // Cada 30 segundos
  
  SystemLogger.info("Sistema de seguridad configurado", "SECURITY");
}

// =============================
// CONFIGURACIÓN SERVIDOR WEB
// =============================
void setupWebServer() {
  // Redirigir raíz a login si no hay autenticación
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->hasHeader("Authorization")) {
      request->redirect("/login.html");
      return;
    }
    request->send(LittleFS, "/index.html", "text/html");
  });
  
  // Servir archivos estáticos (público)
  server.serveStatic("/login.html", LittleFS, "/login.html");
  server.serveStatic("/demo.html", LittleFS, "/demo.html");
  
  // Servir páginas protegidas con autenticación
  server.on("/configuracion.html", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->hasHeader("Authorization")) {
      request->redirect("/login.html");
      return;
    }
    request->send(LittleFS, "/configuracion.html", "text/html");
  });
  
  // === APIs DE AUTENTICACIÓN (Públicas) ===
  
  // API: Login
  server.on("/api/auth/login", HTTP_POST, [](AsyncWebServerRequest *request){},
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String clientId = getClientIdentifier(request);
      
      // Rate limiting
      CHECK_RATE_LIMIT(clientId);
      
      // Validar JSON
      if (!Security.validateJSON(String((char*)data))) {
        request->send(400, "application/json", "{\"error\":\"JSON inválido\"}");
        return;
      }
      
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, data);
      
      if (error || !doc.containsKey("username") || !doc.containsKey("password")) {
        request->send(400, "application/json", "{\"error\":\"Datos incompletos\"}");
        return;
      }
      
      String username = doc["username"].as<String>();
      String password = doc["password"].as<String>();
      
      // Validar entradas
      if (!Security.validateInput(username, INPUT_TYPE_ALPHANUM) ||
          !Security.validateLength(password, 100)) {
        request->send(400, "application/json", "{\"error\":\"Datos inválidos\"}");
        return;
      }
      
      // Intentar login
      String token = Auth.login(username, password, clientId);
      
      if (token.length() > 0) {
        StaticJsonDocument<256> response;
        response["token"] = token;
        response["role"] = Auth.getUserRole(token);
        response["username"] = username;
        
        String jsonResponse;
        serializeJson(response, jsonResponse);
        request->send(200, "application/json", jsonResponse);
      } else {
        request->send(401, "application/json", "{\"error\":\"Credenciales inválidas\"}");
      }
    });
  
  // API: Logout
  server.on("/api/auth/logout", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!request->hasHeader("Authorization")) {
      request->send(401, "application/json", "{\"error\":\"No autorizado\"}");
      return;
    }
    
    String token = request->header("Authorization");
    token.replace("Bearer ", "");
    
    if (Auth.logout(token)) {
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
      request->send(400, "application/json", "{\"error\":\"Token inválido\"}");
    }
  });
  
  // API: Validar token
  server.on("/api/auth/validate", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->hasHeader("Authorization")) {
      request->send(401, "application/json", "{\"error\":\"No autorizado\"}");
      return;
    }
    
    String token = request->header("Authorization");
    token.replace("Bearer ", "");
    
    if (Auth.validateToken(token)) {
      StaticJsonDocument<256> response;
      response["valid"] = true;
      response["username"] = Auth.getCurrentUser(token);
      response["role"] = Auth.getUserRole(token);
      
      String jsonResponse;
      serializeJson(response, jsonResponse);
      request->send(200, "application/json", jsonResponse);
    } else {
      request->send(200, "application/json", "{\"valid\":false}");
    }
  });
  
  // === APIs PROTEGIDAS ===
  
  // Archivos estáticos protegidos
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_VIEWER);
    request->send(LittleFS, "/index.html", "text/html");
  });
  
  server.on("/diagnostico.html", HTTP_GET, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_OPERATOR);
    request->send(LittleFS, "/diagnostico.html", "text/html");
  });
  
  // API: Información del sistema (requiere autenticación)
  server.on("/api/system/info", HTTP_GET, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_VIEWER);
    request->send(200, "application/json", getSystemInfo());
  });
  
  // API: Reiniciar sistema (solo admin)
  server.on("/api/system/restart", HTTP_POST, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_ADMIN);
    SystemLogger.warning("Reinicio solicitado por: " + Auth.getCurrentUser(request->header("Authorization")), "SYSTEM");
    request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Reiniciando...\"}");
    delay(1000);
    ESP.restart();
  });
  
  // API: Estado de luminarias (requiere viewer)
  server.on("/estado-luces", HTTP_GET, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_VIEWER);
    request->send(200, "application/json", getLuminariasJson());
  });
  
  // API: Actualizar luminaria (requiere operator)
  server.on("/actualizar-luz", HTTP_POST, [](AsyncWebServerRequest *request){},
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      REQUIRE_AUTH(request, ROLE_OPERATOR);
      
      String clientId = getClientIdentifier(request);
      CHECK_RATE_LIMIT(clientId);
      
      // Validar JSON
      if (!Security.validateJSON(String((char*)data))) {
        request->send(400, "application/json", "{\"error\":\"JSON inválido\"}");
        return;
      }
      
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, data);
      
      if (error) {
        request->send(400, "application/json", "{\"error\":\"JSON inválido\"}");
        return;
      }

      if (!doc.containsKey("lat") || !doc.containsKey("lng") || !doc.containsKey("estado")) {
        request->send(400, "application/json", "{\"error\":\"Faltan parámetros\"}");
        return;
      }

      float lat = doc["lat"];
      float lng = doc["lng"];
      String estado = doc["estado"].as<String>();
      
      // Validar estado
      if (estado != "encendida" && estado != "apagada" && estado != "falla") {
        request->send(400, "application/json", "{\"error\":\"Estado inválido\"}");
        return;
      }
      
      actualizarLuminaria(lat, lng, estado);
      
      // Log de auditoría
      String user = Auth.getCurrentUser(request->header("Authorization"));
      SystemLogger.info("Luminaria actualizada por " + user + ": " + generateLuminariaId(lat, lng), "AUDIT");
      
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
  
  // === APIs DE ADMINISTRACIÓN (Solo Admin) ===
  
  // API: Gestión de usuarios
  server.on("/api/admin/users", HTTP_GET, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_ADMIN);
    request->send(200, "application/json", Auth.getAuthStats());
  });
  
  server.on("/api/admin/users/add", HTTP_POST, [](AsyncWebServerRequest *request){},
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      REQUIRE_AUTH(request, ROLE_ADMIN);
      
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, data);
      
      if (error || !doc.containsKey("username") || !doc.containsKey("password") || !doc.containsKey("role")) {
        request->send(400, "application/json", "{\"error\":\"Datos incompletos\"}");
        return;
      }
      
      String username = doc["username"].as<String>();
      String password = doc["password"].as<String>();
      int role = doc["role"];
      
      // Validar entradas
      if (!Security.validateInput(username, INPUT_TYPE_ALPHANUM)) {
        request->send(400, "application/json", "{\"error\":\"Username inválido\"}");
        return;
      }
      
      if (Auth.addUser(username, password, (UserRole)role)) {
        request->send(200, "application/json", "{\"status\":\"ok\"}");
      } else {
        request->send(400, "application/json", "{\"error\":\"No se pudo crear usuario\"}");
      }
    });
  
  // API: Sesiones activas
  server.on("/api/admin/sessions", HTTP_GET, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_ADMIN);
    request->send(200, "application/json", Auth.getAllSessions());
  });
  
  // API: Estadísticas de seguridad
  server.on("/api/admin/security/stats", HTTP_GET, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_ADMIN);
    request->send(200, "application/json", Security.getSecurityStats());
  });
  
  // API: Crear backup
  server.on("/api/admin/backup", HTTP_POST, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_ADMIN);
    
    if (Security.createBackup()) {
      request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Backup creado\"}");
    } else {
      request->send(500, "application/json", "{\"error\":\"Error al crear backup\"}");
    }
  });
  
  // API: Restaurar backup
  server.on("/api/admin/restore", HTTP_POST, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_ADMIN);
    
    if (Security.restoreBackup()) {
      request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Backup restaurado\"}");
    } else {
      request->send(500, "application/json", "{\"error\":\"Error al restaurar backup\"}");
    }
  });
  
  // === APIs DE MONITOREO (Operator+) ===
  
  // API: WiFi stats
  server.on("/api/wifi/stats", HTTP_GET, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_OPERATOR);
    request->send(200, "application/json", WiFiMgr.getWifiStats());
  });
  
  // API: Memory stats
  server.on("/api/memory/stats", HTTP_GET, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_OPERATOR);
    request->send(200, "application/json", MemManager.getMemoryStats());
  });
  
  // API: Logs recientes
  server.on("/api/logs/recent", HTTP_GET, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_OPERATOR);
    request->send(200, "application/json", SystemLogger.getRecentLogs(50));
  });
  
  // === APIs FASE 3: CARACTERÍSTICAS AVANZADAS ===
  
  // API: Programaciones horarias
  server.on("/api/schedules", HTTP_GET, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_VIEWER);
    request->send(200, "application/json", Database.getSchedulesJson());
  });
  
  server.on("/api/schedules/add", HTTP_POST, [](AsyncWebServerRequest *request){},
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      REQUIRE_AUTH(request, ROLE_OPERATOR);
      
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, data);
      
      if (error) {
        request->send(400, "application/json", "{\"error\":\"JSON inválido\"}");
        return;
      }
      
      String name = doc["name"].as<String>();
      uint8_t hourOn = doc["hourOn"];
      uint8_t minuteOn = doc["minuteOn"];
      uint8_t hourOff = doc["hourOff"];
      uint8_t minuteOff = doc["minuteOff"];
      uint8_t daysOfWeek = doc["daysOfWeek"];
      
      uint32_t id = Database.addSchedule(name, hourOn, minuteOn, hourOff, minuteOff, daysOfWeek);
      
      StaticJsonDocument<128> response;
      response["id"] = id;
      response["status"] = "ok";
      String jsonResponse;
      serializeJson(response, jsonResponse);
      request->send(200, "application/json", jsonResponse);
    });
  
  // API: Habilitar/deshabilitar programación
  server.on("/api/schedules/*/enable", HTTP_POST, [](AsyncWebServerRequest *request){},
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      REQUIRE_AUTH(request, ROLE_OPERATOR);
      
      String path = request->url();
      int id = path.substring(15, path.indexOf("/enable")).toInt();
      
      StaticJsonDocument<128> doc;
      DeserializationError error = deserializeJson(doc, data);
      
      if (error || !doc.containsKey("enabled")) {
        request->send(400, "application/json", "{\"error\":\"Datos inválidos\"}");
        return;
      }
      
      bool enabled = doc["enabled"];
      Database.enableSchedule(id, enabled);
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
  
  // API: Eliminar programación
  server.on("/api/schedules/*", HTTP_DELETE, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_OPERATOR);
    
    String path = request->url();
    int id = path.substring(15).toInt();
    
    if (Database.deleteSchedule(id)) {
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
      request->send(404, "application/json", "{\"error\":\"Programación no encontrada\"}");
    }
  });
  
  // API: Zonas
  server.on("/api/zones", HTTP_GET, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_VIEWER);
    request->send(200, "application/json", Database.getZonesJson());
  });
  
  server.on("/api/zones/add", HTTP_POST, [](AsyncWebServerRequest *request){},
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      REQUIRE_AUTH(request, ROLE_OPERATOR);
      
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, data);
      
      if (error) {
        request->send(400, "application/json", "{\"error\":\"JSON inválido\"}");
        return;
      }
      
      String name = doc["name"].as<String>();
      String description = doc["description"].as<String>();
      
      uint32_t id = Database.addZone(name, description);
      
      StaticJsonDocument<128> response;
      response["id"] = id;
      response["status"] = "ok";
      String jsonResponse;
      serializeJson(response, jsonResponse);
      request->send(200, "application/json", jsonResponse);
    });
  
  // API: Alertas
  server.on("/api/alerts", HTTP_GET, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_VIEWER);
    request->send(200, "application/json", Alerts.getAlertsJson());
  });
  
  server.on("/api/alerts/*/acknowledge", HTTP_POST, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_OPERATOR);
    
    String path = request->url();
    int id = path.substring(12, path.indexOf("/acknowledge")).toInt();
    
    String token = request->header("Authorization");
    token.replace("Bearer ", "");
    String username = Auth.getUsernameFromToken(token);
    
    if (Alerts.acknowledgeAlert(id, username)) {
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
      request->send(404, "application/json", "{\"error\":\"Alerta no encontrada\"}");
    }
  });
  
  // API: Estadísticas de consumo
  server.on("/api/consumption/stats", HTTP_GET, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_VIEWER);
    
    StaticJsonDocument<512> doc;
    float totalPower = 0;
    float totalEnergy24h = 0;
    
    for (const auto& luz : luminarias) {
      if (luz.estado == "encendida") {
        totalPower += 50;  // 50W por luminaria (ejemplo)
      }
    }
    
    // Calcular energía últimas 24h (simplificado)
    totalEnergy24h = (totalPower * 12) / 1000;  // kWh (asumiendo 12h encendidas)
    
    doc["total_power"] = totalPower;
    doc["total_energy_24h"] = totalEnergy24h;
    doc["luminarias_on"] = 0;
    doc["luminarias_off"] = 0;
    doc["luminarias_fault"] = 0;
    
    for (const auto& luz : luminarias) {
      if (luz.estado == "encendida") doc["luminarias_on"] = doc["luminarias_on"].as<int>() + 1;
      else if (luz.estado == "apagada") doc["luminarias_off"] = doc["luminarias_off"].as<int>() + 1;
      else if (luz.estado == "falla") doc["luminarias_fault"] = doc["luminarias_fault"].as<int>() + 1;
    }
    
    String result;
    serializeJson(doc, result);
    request->send(200, "application/json", result);
  });
  
  // API: Exportar datos
  server.on("/api/export/*", HTTP_GET, [](AsyncWebServerRequest *request){
    REQUIRE_AUTH(request, ROLE_OPERATOR);
    
    String path = request->url();
    String type = path.substring(12);
    String format = request->arg("format");
    
    if (format == "csv") {
      String csv = Database.exportToCSV(type);
      request->send(200, "text/csv", csv);
    } else {
      String json = Database.exportToJSON(type);
      request->send(200, "application/json", json);
    }
  });
  
  // Manejo de rutas no encontradas
  server.onNotFound([](AsyncWebServerRequest *request){
    SystemLogger.warning("Ruta no encontrada: " + request->url() + " desde " + getClientIdentifier(request), "WEB");
    request->send(404, "application/json", "{\"error\":\"Ruta no encontrada\"}");
  });
  
  // CORS headers
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
  
  server.begin();
  SystemLogger.info("Servidor web iniciado en puerto " + String(WEB_SERVER_PORT) + " con seguridad habilitada", "WEB");
}

// =============================
// FUNCIONES DE MONITOREO
// =============================
void sendHeartbeat() {
  SystemLogger.debug("Heartbeat - Luminarias: " + String(luminarias.size()) + 
                    ", Heap: " + String(ESP.getFreeHeap()) + 
                    ", Uptime: " + String(millis()/1000) + "s" +
                    ", Sesiones: " + String(Auth.getActiveSessionCount()), "HEARTBEAT");
  
  // Alimentar watchdog
  Security.feedWatchdog();
  
  // Flush logs
  SystemLogger.flush();
}

void checkSessions() {
  Auth.checkSessions();
}

// =============================
// CALLBACKS WiFi
// =============================
void onWiFiConnect() {
  SystemLogger.info("WiFi conectado - Configurando servicios", "WIFI");
  setupMDNS();
}

void onWiFiDisconnect() {
  SystemLogger.warning("WiFi desconectado - Servicios en espera", "WIFI");
}

// =============================
// SETUP PRINCIPAL
// =============================
void setup() {
  setupSerial();
  
  // Inicializar sistema de logs
  SystemLogger.begin();
  SystemLogger.info("=== INICIO DEL SISTEMA v" + String(FIRMWARE_VERSION) + " ===", "SYSTEM");
  
  // Inicializar gestor de memoria
  MemManager.begin();
  
  // Configurar sistema de archivos
  setupFileSystem();
  
  // Inicializar seguridad
  setupSecurity();
  
  // === FASE 3: Inicializar nuevos componentes ===
  
  // Inicializar base de datos
  Database.begin();
  SystemLogger.info("Base de datos inicializada", "SYSTEM");
  
  // Inicializar gestor de tiempo
  Time.begin();
  
  // Inicializar scheduler
  Scheduler.begin();
  Scheduler.setCallback([](ScheduleAction action, String target, int value) {
    SystemLogger.info("Acción programada: " + String(action) + " en " + target, "SCHEDULE");
    
    // Ejecutar acción sobre luminarias
    if (action == ACTION_TURN_ON) {
      for (auto& luz : luminarias) {
        if (target == "all" || luz.id == target) {
          luz.estado = "encendida";
          luz.intensidad = value;
        }
      }
    } else if (action == ACTION_TURN_OFF) {
      for (auto& luz : luminarias) {
        if (target == "all" || luz.id == target) {
          luz.estado = "apagada";
          luz.intensidad = 0;
        }
      }
    }
  });
  
  // Crear programaciones por defecto
  Scheduler.createDefaultSchedules();
  
  // Inicializar sistema de alertas
  Alerts.begin();
  Alerts.registerCallback([](const Alert& alert) {
    SystemLogger.info("Nueva alerta: " + alert.message, "ALERT");
  });
  
  // Configurar WiFi con callbacks
  WiFiMgr.onConnect(onWiFiConnect);
  WiFiMgr.onDisconnect(onWiFiDisconnect);
  WiFiMgr.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Configurar servidor web
  setupWebServer();
  
  // Configurar LED de estado
  pinMode(LED_STATUS_PIN, OUTPUT);
  digitalWrite(LED_STATUS_PIN, LOW);
  
  // Configurar heartbeat
  heartbeatTimer.attach_ms(HEARTBEAT_INTERVAL, sendHeartbeat);
  
  // Agregar algunas luminarias de ejemplo
  actualizarLuminaria(DEFAULT_LAT, DEFAULT_LNG, "encendida");
  actualizarLuminaria(DEFAULT_LAT + 0.001, DEFAULT_LNG + 0.001, "apagada");
  actualizarLuminaria(DEFAULT_LAT - 0.001, DEFAULT_LNG - 0.001, "falla");
  
  // Registrar luminarias en base de datos
  for (const auto& luz : luminarias) {
    Database.logEvent(luz.id, EVENT_STATE_CHANGE, "Estado inicial: " + luz.estado, "SYSTEM");
  }
  
  SystemLogger.info("=== SISTEMA v" + String(FIRMWARE_VERSION) + " INICIADO ===", "SYSTEM");
  SystemLogger.info("Fase 3: Características Avanzadas ACTIVAS", "SYSTEM");
  SystemLogger.info("- Base de datos local ✓", "SYSTEM");
  SystemLogger.info("- Programación horaria ✓", "SYSTEM");
  SystemLogger.info("- Sistema de alertas ✓", "SYSTEM");
  SystemLogger.info("- Gestión de zonas ✓", "SYSTEM");
  SystemLogger.info("Acceder a: http://" + WiFiMgr.getIP() + "/login.html", "SYSTEM");
  SystemLogger.warning("Usuarios por defecto: admin/admin123, operator/oper123, viewer/view123", "SECURITY");
  SystemLogger.warning("CAMBIAR CONTRASEÑAS EN PRODUCCIÓN", "SECURITY");
}

// =============================
// LOOP PRINCIPAL
// =============================
void loop() {
  // Actualizar gestor WiFi
  WiFiMgr.loop();
  
  // Actualizar mDNS
  if (WiFiMgr.isConnected()) {
    MDNS.update();
  }
  
  // Verificar memoria y seguridad
  if (millis() - lastUpdate > UPDATE_INTERVAL) {
    lastUpdate = millis();
    
    // Chequear memoria
    MemManager.check();
    
    // Alimentar watchdog
    Security.feedWatchdog();
    
    // === FASE 3: Nuevas verificaciones ===
    
    // Verificar condiciones de alerta
    static unsigned long lastAlertCheck = 0;
    if (millis() - lastAlertCheck > 30000) {  // Cada 30 segundos
      lastAlertCheck = millis();
      
      // Verificar estado de luminarias
      for (const auto& luz : luminarias) {
        // Verificar fallas
        if (luz.estado == "falla") {
          Alerts.checkLuminariaFailure(luz.id, luz.estado);
        }
        
        // Verificar consumo (simulado)
        if (luz.estado == "encendida") {
          float consumption = 50 + random(-10, 10);  // 50W ± 10W
          Alerts.checkConsumption(luz.id, consumption);
          
          // Registrar consumo en base de datos
          Database.recordConsumption(luz.id, consumption, consumption * 0.001);  // kWh
        }
        
        // Verificar timeout (simulado)
        if (millis() - luz.ultimaActualizacion > 300000) {  // 5 minutos
          Alerts.createAlert(ALERT_OFFLINE, SEVERITY_WARNING, 
                           luz.id, "Luminaria sin respuesta", 
                           "Última actualización hace " + String((millis() - luz.ultimaActualizacion)/1000) + " segundos");
        }
      }
      
      // Verificar salud del sistema
      Alerts.checkSystemHealth();
    }
    
    // Limpiar base de datos periódicamente
    static unsigned long lastDbCleanup = 0;
    if (millis() - lastDbCleanup > 3600000) {  // Cada hora
      lastDbCleanup = millis();
      Database.cleanOldEvents(7);  // Mantener 7 días
      SystemLogger.info("Limpieza de base de datos completada", "DB");
    }
    
    // Parpadear LED
    digitalWrite(LED_STATUS_PIN, !digitalRead(LED_STATUS_PIN));
  }
  
  // Watchdog y yield
  ESP.wdtFeed();
  yield();
}