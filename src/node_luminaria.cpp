// =============================
// Firmware Nodo Luminaria - ESP8266
// Sistema de Control de Alumbrado Público
// Versión: 0.6.0 - Nodo Secundario
// =============================

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <EEPROM.h>

// =============================
// CONFIGURACIÓN
// =============================

// WiFi
const char* WIFI_SSID = "TU_SSID";
const char* WIFI_PASSWORD = "TU_PASSWORD";

// MQTT
const char* MQTT_SERVER = "192.168.1.100";  // IP del nodo central o broker
const int MQTT_PORT = 1883;
const char* MQTT_USER = "";  // Opcional
const char* MQTT_PASSWORD = "";  // Opcional

// Configuración del nodo
String NODE_ID = "";  // Se genera automáticamente si está vacío
const char* NODE_TYPE = "LUMINARIA";
const char* FIRMWARE_VERSION = "0.6.0";

// Pines
#define LED_PIN LED_BUILTIN     // LED de estado
#define RELAY_PIN D1            // Relé para control de luminaria
#define CURRENT_SENSOR_PIN A0   // Sensor de corriente (opcional)
#define LIGHT_SENSOR_PIN D2     // Sensor de luz ambiental (opcional)

// Tiempos
#define RECONNECT_DELAY 5000
#define TELEMETRY_INTERVAL 60000    // Enviar telemetría cada minuto
#define DISCOVERY_INTERVAL 300000    // Broadcast discovery cada 5 minutos
#define WATCHDOG_TIMEOUT 8000

// =============================
// VARIABLES GLOBALES
// =============================

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Ticker heartbeatTicker;
Ticker telemetryTicker;
Ticker watchdogTicker;

// Estado del nodo
struct NodeState {
    bool lightOn;
    uint8_t brightness;  // 0-100%
    float current;       // Consumo en amperios
    float power;         // Potencia en vatios
    uint32_t uptime;
    uint32_t lastCommand;
    String zoneId;
    bool autoMode;
    uint16_t autoOnHour;
    uint16_t autoOffHour;
} nodeState;

// Estadísticas
struct NodeStats {
    uint32_t commandsReceived;
    uint32_t telemetrySent;
    uint32_t reconnections;
    uint32_t errors;
    float totalEnergy;  // kWh acumulados
} nodeStats;

// Configuración persistente
struct NodeConfig {
    char nodeId[32];
    char zoneId[32];
    bool autoMode;
    uint16_t autoOnTime;
    uint16_t autoOffTime;
    uint8_t defaultBrightness;
    uint32_t checksum;
} nodeConfig;

// =============================
// FUNCIONES DE UTILIDAD
// =============================

String generateNodeId() {
    return "LUM_" + String(ESP.getChipId());
}

void saveConfig() {
    nodeConfig.checksum = 0;
    for (int i = 0; i < sizeof(NodeConfig) - sizeof(uint32_t); i++) {
        nodeConfig.checksum += ((uint8_t*)&nodeConfig)[i];
    }
    
    EEPROM.begin(sizeof(NodeConfig));
    EEPROM.put(0, nodeConfig);
    EEPROM.commit();
    Serial.println("Configuración guardada");
}

void loadConfig() {
    EEPROM.begin(sizeof(NodeConfig));
    EEPROM.get(0, nodeConfig);
    
    // Verificar checksum
    uint32_t checksum = 0;
    for (int i = 0; i < sizeof(NodeConfig) - sizeof(uint32_t); i++) {
        checksum += ((uint8_t*)&nodeConfig)[i];
    }
    
    if (checksum != nodeConfig.checksum) {
        Serial.println("Config inválida, usando valores por defecto");
        strcpy(nodeConfig.nodeId, generateNodeId().c_str());
        strcpy(nodeConfig.zoneId, "default");
        nodeConfig.autoMode = false;
        nodeConfig.autoOnTime = 1800;   // 18:00
        nodeConfig.autoOffTime = 600;    // 06:00
        nodeConfig.defaultBrightness = 100;
        saveConfig();
    }
    
    NODE_ID = String(nodeConfig.nodeId);
    nodeState.zoneId = String(nodeConfig.zoneId);
    nodeState.autoMode = nodeConfig.autoMode;
    nodeState.autoOnHour = nodeConfig.autoOnTime;
    nodeState.autoOffHour = nodeConfig.autoOffTime;
    nodeState.brightness = nodeConfig.defaultBrightness;
}

// =============================
// CONTROL DE LUMINARIA
// =============================

void setLight(bool on, uint8_t brightness = 100) {
    nodeState.lightOn = on;
    nodeState.brightness = constrain(brightness, 0, 100);
    
    if (on) {
        // Si tuviéramos PWM, ajustaríamos el brillo
        // Por ahora solo encender/apagar
        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(LED_PIN, LOW);  // LED invertido
        Serial.println("Luz encendida al " + String(brightness) + "%");
    } else {
        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(LED_PIN, HIGH);
        Serial.println("Luz apagada");
    }
    
    // Actualizar consumo
    updatePowerConsumption();
}

void updatePowerConsumption() {
    if (nodeState.lightOn) {
        // Leer sensor de corriente si está disponible
        int sensorValue = analogRead(CURRENT_SENSOR_PIN);
        nodeState.current = (sensorValue / 1024.0) * 5.0;  // Conversión simplificada
        nodeState.power = nodeState.current * 220;  // Asumiendo 220V
    } else {
        nodeState.current = 0;
        nodeState.power = 0;
    }
}

float readLightSensor() {
    // Leer sensor de luz ambiental
    // Retornar valor en lux (simplificado)
    return analogRead(LIGHT_SENSOR_PIN) / 10.24;  // 0-100 lux aproximado
}

// =============================
// MQTT CALLBACKS
// =============================

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    // Convertir payload a string
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    String msg = String(message);
    String topicStr = String(topic);
    
    Serial.println("Mensaje recibido: " + topicStr);
    Serial.println("Payload: " + msg);
    
    // Parsear JSON
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, msg);
    
    if (error) {
        Serial.println("Error parseando JSON: " + String(error.c_str()));
        return;
    }
    
    // Procesar comandos
    if (topicStr.endsWith("/cmd/" + NODE_ID) || topicStr.endsWith("/cmd/all")) {
        handleCommand(doc);
    }
    // Procesar comandos de zona
    else if (topicStr.endsWith("/zone/" + nodeState.zoneId)) {
        handleZoneCommand(doc);
    }
    // Procesar OTA
    else if (topicStr.endsWith("/ota/request")) {
        handleOTARequest(doc);
    }
}

void handleCommand(JsonDocument& doc) {
    String command = doc["command"].as<String>();
    JsonObject params = doc["params"];
    
    nodeStats.commandsReceived++;
    nodeState.lastCommand = millis();
    
    Serial.println("Comando: " + command);
    
    if (command == "on") {
        uint8_t brightness = params["brightness"] | 100;
        setLight(true, brightness);
        sendStatus();
    }
    else if (command == "off") {
        setLight(false);
        sendStatus();
    }
    else if (command == "toggle") {
        setLight(!nodeState.lightOn);
        sendStatus();
    }
    else if (command == "set_brightness") {
        uint8_t brightness = params["brightness"];
        setLight(nodeState.lightOn, brightness);
        sendStatus();
    }
    else if (command == "set_zone") {
        String newZone = params["zone"].as<String>();
        nodeState.zoneId = newZone;
        strcpy(nodeConfig.zoneId, newZone.c_str());
        saveConfig();
        
        // Suscribirse a nueva zona
        String zoneTopic = "luces/zone/" + newZone + "/#";
        mqttClient.subscribe(zoneTopic.c_str());
        sendStatus();
    }
    else if (command == "set_auto") {
        nodeState.autoMode = params["enabled"];
        nodeState.autoOnHour = params["on_time"] | 1800;
        nodeState.autoOffHour = params["off_time"] | 600;
        
        nodeConfig.autoMode = nodeState.autoMode;
        nodeConfig.autoOnTime = nodeState.autoOnHour;
        nodeConfig.autoOffTime = nodeState.autoOffHour;
        saveConfig();
        sendStatus();
    }
    else if (command == "get_status") {
        sendStatus();
    }
    else if (command == "get_telemetry") {
        sendTelemetry();
    }
    else if (command == "restart") {
        Serial.println("Reiniciando...");
        ESP.restart();
    }
    else if (command == "factory_reset") {
        EEPROM.begin(sizeof(NodeConfig));
        for (int i = 0; i < sizeof(NodeConfig); i++) {
            EEPROM.write(i, 0);
        }
        EEPROM.commit();
        ESP.restart();
    }
}

void handleZoneCommand(JsonDocument& doc) {
    String command = doc["command"].as<String>();
    
    Serial.println("Comando de zona: " + command);
    
    if (command == "all_on") {
        setLight(true);
    }
    else if (command == "all_off") {
        setLight(false);
    }
    else if (command == "set_brightness") {
        uint8_t brightness = doc["brightness"];
        setLight(nodeState.lightOn, brightness);
    }
}

void handleOTARequest(JsonDocument& doc) {
    String version = doc["version"].as<String>();
    String url = doc["url"].as<String>();
    
    Serial.println("OTA disponible: " + version);
    Serial.println("URL: " + url);
    
    // Aquí implementarías la actualización OTA
    // Por ahora solo lo reportamos
    sendOTAStatus("available", version);
}

// =============================
// MQTT PUBLICACIÓN
// =============================

void sendDiscovery() {
    StaticJsonDocument<512> doc;
    doc["nodeId"] = NODE_ID;
    doc["type"] = NODE_TYPE;
    doc["version"] = FIRMWARE_VERSION;
    doc["ip"] = WiFi.localIP().toString();
    doc["mac"] = WiFi.macAddress();
    doc["rssi"] = WiFi.RSSI();
    doc["zone"] = nodeState.zoneId;
    doc["capabilities"]["dimming"] = false;  // Por ahora no tenemos PWM
    doc["capabilities"]["current_sensor"] = true;
    doc["capabilities"]["light_sensor"] = true;
    doc["capabilities"]["auto_mode"] = true;
    
    String topic = "luces/discovery";
    String payload;
    serializeJson(doc, payload);
    
    mqttClient.publish(topic.c_str(), payload.c_str());
    Serial.println("Discovery enviado");
}

void sendStatus() {
    StaticJsonDocument<256> doc;
    doc["nodeId"] = NODE_ID;
    doc["online"] = true;
    doc["light"] = nodeState.lightOn;
    doc["brightness"] = nodeState.brightness;
    doc["auto_mode"] = nodeState.autoMode;
    doc["zone"] = nodeState.zoneId;
    doc["uptime"] = millis() / 1000;
    
    String topic = "luces/status/" + NODE_ID;
    String payload;
    serializeJson(doc, payload);
    
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
    Serial.println("Estado enviado");
}

void sendTelemetry() {
    updatePowerConsumption();
    
    StaticJsonDocument<512> doc;
    doc["nodeId"] = NODE_ID;
    doc["timestamp"] = millis();
    doc["power"]["current"] = nodeState.current;
    doc["power"]["watts"] = nodeState.power;
    doc["power"]["total_kwh"] = nodeStats.totalEnergy;
    doc["sensors"]["light_level"] = readLightSensor();
    doc["sensors"]["temperature"] = 25;  // Placeholder
    doc["stats"]["commands"] = nodeStats.commandsReceived;
    doc["stats"]["telemetry"] = nodeStats.telemetrySent;
    doc["stats"]["reconnections"] = nodeStats.reconnections;
    doc["stats"]["errors"] = nodeStats.errors;
    doc["memory"]["heap"] = ESP.getFreeHeap();
    doc["memory"]["fragmentation"] = ESP.getHeapFragmentation();
    
    String topic = "luces/telemetry/" + NODE_ID;
    String payload;
    serializeJson(doc, payload);
    
    mqttClient.publish(topic.c_str(), payload.c_str());
    nodeStats.telemetrySent++;
    
    Serial.println("Telemetría enviada");
}

void sendAlert(String type, String message) {
    StaticJsonDocument<256> doc;
    doc["nodeId"] = NODE_ID;
    doc["type"] = type;
    doc["message"] = message;
    doc["timestamp"] = millis();
    
    String topic = "luces/alert/" + NODE_ID;
    String payload;
    serializeJson(doc, payload);
    
    mqttClient.publish(topic.c_str(), payload.c_str());
    Serial.println("Alerta enviada: " + message);
}

void sendOTAStatus(String status, String details) {
    StaticJsonDocument<256> doc;
    doc["nodeId"] = NODE_ID;
    doc["status"] = status;
    doc["details"] = details;
    doc["current_version"] = FIRMWARE_VERSION;
    
    String topic = "luces/ota/status/" + NODE_ID;
    String payload;
    serializeJson(doc, payload);
    
    mqttClient.publish(topic.c_str(), payload.c_str());
}

// =============================
// MQTT CONEXIÓN
// =============================

void connectMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Conectando a MQTT...");
        
        // Mensaje LWT (Last Will and Testament)
        String willTopic = "luces/status/" + NODE_ID;
        String willMessage = "{\"nodeId\":\"" + NODE_ID + "\",\"online\":false}";
        
        if (mqttClient.connect(NODE_ID.c_str(), MQTT_USER, MQTT_PASSWORD,
                               willTopic.c_str(), 1, true, willMessage.c_str())) {
            Serial.println(" conectado!");
            
            // Suscribirse a topics
            mqttClient.subscribe(("luces/cmd/" + NODE_ID + "/#").c_str());
            mqttClient.subscribe("luces/cmd/all/#");
            mqttClient.subscribe(("luces/zone/" + nodeState.zoneId + "/#").c_str());
            mqttClient.subscribe("luces/ota/request");
            
            // Enviar estado inicial
            sendStatus();
            sendDiscovery();
            
            nodeStats.reconnections++;
        } else {
            Serial.print(" fallo, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" reintentando en 5 segundos");
            delay(5000);
        }
    }
}

// =============================
// WIFI
// =============================

void setupWiFi() {
    Serial.println();
    Serial.print("Conectando a WiFi: ");
    Serial.println(WIFI_SSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    
    Serial.println();
    Serial.println("WiFi conectado");
    Serial.println("IP: " + WiFi.localIP().toString());
    Serial.println("MAC: " + WiFi.macAddress());
    
    digitalWrite(LED_PIN, HIGH);  // LED apagado (invertido)
}

// =============================
// TIMERS Y CALLBACKS
// =============================

void heartbeatCallback() {
    // Heartbeat simple
    String topic = "luces/heartbeat/" + NODE_ID;
    String payload = "{\"nodeId\":\"" + NODE_ID + "\",\"timestamp\":" + String(millis()) + "}";
    mqttClient.publish(topic.c_str(), payload.c_str());
}

void telemetryCallback() {
    sendTelemetry();
    
    // Actualizar energía acumulada
    if (nodeState.lightOn) {
        nodeStats.totalEnergy += (nodeState.power / 1000.0) * (TELEMETRY_INTERVAL / 3600000.0);
    }
}

void watchdogCallback() {
    // Watchdog simple - si no se alimenta, reiniciar
    ESP.restart();
}

void checkAutoMode() {
    if (!nodeState.autoMode) return;
    
    // Obtener hora actual (simplificado - en producción usar NTP)
    uint32_t now = millis() / 1000;
    uint16_t currentHour = (now / 3600) % 24;
    uint16_t currentTime = currentHour * 100;  // HHMM formato
    
    // Verificar si debe encender
    if (!nodeState.lightOn && currentTime >= nodeState.autoOnHour) {
        setLight(true);
        sendAlert("auto", "Encendido automático");
    }
    // Verificar si debe apagar
    else if (nodeState.lightOn && currentTime >= nodeState.autoOffHour && currentTime < nodeState.autoOnHour) {
        setLight(false);
        sendAlert("auto", "Apagado automático");
    }
}

// =============================
// SETUP
// =============================

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("========================================");
    Serial.println("Nodo Luminaria - v" + String(FIRMWARE_VERSION));
    Serial.println("========================================");
    
    // Configurar pines
    pinMode(LED_PIN, OUTPUT);
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LIGHT_SENSOR_PIN, INPUT);
    digitalWrite(LED_PIN, HIGH);  // LED apagado
    digitalWrite(RELAY_PIN, LOW);  // Luz apagada
    
    // Cargar configuración
    loadConfig();
    Serial.println("Node ID: " + NODE_ID);
    Serial.println("Zona: " + nodeState.zoneId);
    
    // Conectar WiFi
    setupWiFi();
    
    // Configurar MQTT
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(onMqttMessage);
    mqttClient.setBufferSize(512);
    
    // Conectar MQTT
    connectMQTT();
    
    // Configurar timers
    heartbeatTicker.attach(30, heartbeatCallback);  // Cada 30 segundos
    telemetryTicker.attach(60, telemetryCallback);  // Cada minuto
    
    // Estado inicial
    nodeState.lightOn = false;
    nodeState.brightness = 100;
    nodeState.uptime = 0;
    
    Serial.println("Sistema iniciado correctamente");
}

// =============================
// LOOP PRINCIPAL
// =============================

void loop() {
    // Mantener conexión MQTT
    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop();
    
    // Verificar modo automático cada minuto
    static unsigned long lastAutoCheck = 0;
    if (millis() - lastAutoCheck > 60000) {
        checkAutoMode();
        lastAutoCheck = millis();
    }
    
    // Enviar discovery periódicamente
    static unsigned long lastDiscovery = 0;
    if (millis() - lastDiscovery > DISCOVERY_INTERVAL) {
        sendDiscovery();
        lastDiscovery = millis();
    }
    
    // Alimentar watchdog
    static unsigned long lastWatchdog = 0;
    if (millis() - lastWatchdog > 1000) {
        ESP.wdtFeed();
        lastWatchdog = millis();
    }
    
    // Parpadear LED si está desconectado
    if (!mqttClient.connected()) {
        static unsigned long lastBlink = 0;
        if (millis() - lastBlink > 500) {
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
            lastBlink = millis();
        }
    }
    
    yield();
}