#include "OTAManager.h"
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

OTAManager OTA;
OTASecurity OTASecure;

// === OTA MANAGER ===

OTAManager::OTAManager() {
    state = OTA_IDLE;
    autoUpdate = false;
    arduinoOTAEnabled = false;
    lastCheckTime = 0;
    updateAttempts = 0;
    successfulUpdates = 0;
    failedUpdates = 0;
}

bool OTAManager::begin(const String& serverUrl) {
    SystemLogger.info("Iniciando OTA Manager", "OTA");
    
    updateServerUrl = serverUrl;
    if (updateServerUrl.length() == 0) {
        updateServerUrl = "http://updates.example.com/esp8266";
    }
    
    deviceId = "ESP_" + String(ESP.getChipId());
    versionInfo.current = FIRMWARE_VERSION;
    
    // Configurar callbacks de ESPhttpUpdate
    ESPhttpUpdate.onStart([]() {
        SystemLogger.info("Iniciando actualización OTA", "OTA");
    });
    
    ESPhttpUpdate.onEnd([]() {
        SystemLogger.info("Actualización OTA completada", "OTA");
    });
    
    ESPhttpUpdate.onProgress([this](int current, int total) {
        int progress = (current * 100) / total;
        this->notifyProgress(progress);
        
        static int lastProgress = 0;
        if (progress - lastProgress >= 10) {
            SystemLogger.info("Progreso OTA: " + String(progress) + "%", "OTA");
            lastProgress = progress;
        }
    });
    
    ESPhttpUpdate.onError([this](int error) {
        SystemLogger.error("Error OTA: " + String(error) + " - " + ESPhttpUpdate.getLastErrorString(), "OTA");
        this->state = OTA_ERROR;
        this->failedUpdates++;
    });
    
    // Si está habilitado, configurar ArduinoOTA para desarrollo
    if (arduinoOTAEnabled) {
        setupArduinoOTA();
    }
    
    SystemLogger.info("OTA Manager iniciado - Versión actual: " + versionInfo.current, "OTA");
    return true;
}

void OTAManager::setupArduinoOTA() {
    ArduinoOTA.setPort(OTA_PORT);
    ArduinoOTA.setHostname((OTA_HOSTNAME_PREFIX + String(ESP.getChipId())).c_str());
    ArduinoOTA.setPassword(OTA_PASSWORD);
    
    ArduinoOTA.onStart([this]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        SystemLogger.info("ArduinoOTA iniciando actualización de " + type, "OTA");
        this->state = OTA_DOWNLOADING;
    });
    
    ArduinoOTA.onEnd([this]() {
        SystemLogger.info("ArduinoOTA completado", "OTA");
        this->state = OTA_SUCCESS;
        this->successfulUpdates++;
    });
    
    ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
        int percent = (progress * 100) / total;
        this->notifyProgress(percent);
    });
    
    ArduinoOTA.onError([this](ota_error_t error) {
        String errorMsg = "Error ArduinoOTA: ";
        switch(error) {
            case OTA_AUTH_ERROR: errorMsg += "Auth Failed"; break;
            case OTA_BEGIN_ERROR: errorMsg += "Begin Failed"; break;
            case OTA_CONNECT_ERROR: errorMsg += "Connect Failed"; break;
            case OTA_RECEIVE_ERROR: errorMsg += "Receive Failed"; break;
            case OTA_END_ERROR: errorMsg += "End Failed"; break;
            default: errorMsg += "Unknown Error";
        }
        SystemLogger.error(errorMsg, "OTA");
        this->state = OTA_ERROR;
        this->failedUpdates++;
    });
    
    ArduinoOTA.begin();
    arduinoOTAEnabled = true;
    SystemLogger.info("ArduinoOTA habilitado en puerto " + String(OTA_PORT), "OTA");
}

void OTAManager::handleArduinoOTA() {
    if (arduinoOTAEnabled) {
        ArduinoOTA.handle();
    }
}

bool OTAManager::checkUpdate() {
    if (state == OTA_DOWNLOADING || state == OTA_INSTALLING) {
        SystemLogger.warning("Actualización en progreso, no se puede verificar", "OTA");
        return false;
    }
    
    state = OTA_CHECKING;
    SystemLogger.info("Verificando actualizaciones...", "OTA");
    
    WiFiClient client;
    HTTPClient http;
    
    String url = updateServerUrl + "/version.json?device=" + deviceId + "&current=" + versionInfo.current;
    
    http.begin(client, url);
    http.addHeader("User-Agent", "ESP8266-OTA/" + String(FIRMWARE_VERSION));
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        if (parseVersionInfo(payload)) {
            if (versionInfo.available != versionInfo.current) {
                state = OTA_AVAILABLE;
                SystemLogger.info("Nueva versión disponible: " + versionInfo.available, "OTA");
                
                // Publicar en MQTT si está conectado
                if (MQTT.isConnected()) {
                    publishOTAStatus();
                }
                
                http.end();
                return true;
            } else {
                state = OTA_IDLE;
                SystemLogger.info("Sistema actualizado - Versión: " + versionInfo.current, "OTA");
            }
        }
    } else {
        SystemLogger.error("Error verificando actualización: HTTP " + String(httpCode), "OTA");
        state = OTA_ERROR;
    }
    
    http.end();
    lastCheckTime = millis();
    return false;
}

bool OTAManager::parseVersionInfo(const String& json) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        SystemLogger.error("Error parseando versión JSON: " + String(error.c_str()), "OTA");
        return false;
    }
    
    versionInfo.available = doc["version"].as<String>();
    versionInfo.changelog = doc["changelog"].as<String>();
    versionInfo.downloadUrl = doc["url"].as<String>();
    versionInfo.size = doc["size"];
    versionInfo.md5 = doc["md5"].as<String>();
    versionInfo.mandatory = doc["mandatory"] | false;
    
    return true;
}

bool OTAManager::startUpdate(OTAType type) {
    if (state != OTA_AVAILABLE) {
        SystemLogger.error("No hay actualización disponible", "OTA");
        return false;
    }
    
    String url = (type == OTA_FIRMWARE) ? versionInfo.downloadUrl : getUpdateUrl(type);
    return startUpdate(url, versionInfo.md5);
}

bool OTAManager::startUpdate(const String& url, const String& md5) {
    if (url.length() == 0) {
        SystemLogger.error("URL de actualización vacía", "OTA");
        return false;
    }
    
    state = OTA_DOWNLOADING;
    updateAttempts++;
    
    SystemLogger.info("Iniciando actualización desde: " + url, "OTA");
    
    // Notificar inicio
    notifyComplete(false, "Iniciando actualización...");
    
    WiFiClient client;
    t_httpUpdate_return ret;
    
    if (md5.length() > 0) {
        ret = ESPhttpUpdate.update(client, url, md5);
    } else {
        ret = ESPhttpUpdate.update(client, url);
    }
    
    switch(ret) {
        case HTTP_UPDATE_FAILED:
            state = OTA_ERROR;
            failedUpdates++;
            notifyComplete(false, ESPhttpUpdate.getLastErrorString());
            SystemLogger.error("Actualización fallida: " + ESPhttpUpdate.getLastErrorString(), "OTA");
            return false;
            
        case HTTP_UPDATE_NO_UPDATES:
            state = OTA_IDLE;
            notifyComplete(false, "No hay actualizaciones");
            SystemLogger.info("No se requiere actualización", "OTA");
            return false;
            
        case HTTP_UPDATE_OK:
            state = OTA_SUCCESS;
            successfulUpdates++;
            notifyComplete(true, "Actualización exitosa");
            SystemLogger.info("Actualización completada exitosamente", "OTA");
            
            // El ESP se reiniciará automáticamente
            return true;
    }
    
    return false;
}

void OTAManager::publishOTAStatus() {
    if (!MQTT.isConnected()) return;
    
    StaticJsonDocument<512> doc;
    doc["nodeId"] = deviceId;
    doc["current_version"] = versionInfo.current;
    doc["available_version"] = versionInfo.available;
    doc["update_available"] = (state == OTA_AVAILABLE);
    doc["state"] = getStateString();
    doc["last_check"] = lastCheckTime;
    doc["attempts"] = updateAttempts;
    doc["successful"] = successfulUpdates;
    doc["failed"] = failedUpdates;
    
    MQTT.publish("luces/ota/status/" + deviceId, doc, true);
}

void OTAManager::handleMQTTOTACommand(const JsonDocument& command) {
    String action = command["action"].as<String>();
    
    SystemLogger.info("Comando OTA recibido: " + action, "OTA");
    
    if (action == "check") {
        checkUpdate();
    }
    else if (action == "update") {
        if (state == OTA_AVAILABLE) {
            String type = command["type"] | "firmware";
            OTAType otaType = (type == "filesystem") ? OTA_FILESYSTEM : OTA_FIRMWARE;
            startUpdate(otaType);
        }
    }
    else if (action == "status") {
        publishOTAStatus();
    }
    else if (action == "rollback") {
        if (canRollback()) {
            rollback();
        }
    }
}

bool OTAManager::requestNodeUpdate(const String& nodeId, const String& version) {
    if (!MQTT.isConnected()) return false;
    
    StaticJsonDocument<256> doc;
    doc["command"] = "ota_update";
    doc["version"] = version;
    doc["url"] = updateServerUrl + "/firmware/" + version + ".bin";
    doc["mandatory"] = false;
    
    return MQTT.publishCommand(nodeId, "ota_update", doc);
}

void OTAManager::notifyProgress(int progress) {
    if (progressCallback) {
        progressCallback(state, progress);
    }
    
    // Publicar progreso en MQTT
    if (MQTT.isConnected() && progress % 10 == 0) {
        StaticJsonDocument<128> doc;
        doc["nodeId"] = deviceId;
        doc["progress"] = progress;
        doc["state"] = getStateString();
        
        MQTT.publish("luces/ota/progress/" + deviceId, doc, false);
    }
}

void OTAManager::notifyComplete(bool success, const String& message) {
    if (completeCallback) {
        completeCallback(success, message);
    }
    
    // Publicar resultado en MQTT
    if (MQTT.isConnected()) {
        StaticJsonDocument<256> doc;
        doc["nodeId"] = deviceId;
        doc["success"] = success;
        doc["message"] = message;
        doc["version"] = success ? versionInfo.available : versionInfo.current;
        
        MQTT.publish("luces/ota/complete/" + deviceId, doc, false);
    }
}

String OTAManager::getStateString() {
    switch(state) {
        case OTA_IDLE: return "idle";
        case OTA_CHECKING: return "checking";
        case OTA_AVAILABLE: return "available";
        case OTA_DOWNLOADING: return "downloading";
        case OTA_INSTALLING: return "installing";
        case OTA_SUCCESS: return "success";
        case OTA_ERROR: return "error";
        default: return "unknown";
    }
}

bool OTAManager::canRollback() {
    // Verificar si hay una versión anterior guardada
    // Esto requeriría un sistema de particiones dual
    // Por ahora retornamos false
    return false;
}

bool OTAManager::rollback() {
    if (!canRollback()) {
        SystemLogger.error("Rollback no disponible", "OTA");
        return false;
    }
    
    // Implementar rollback con sistema de particiones dual
    SystemLogger.info("Iniciando rollback...", "OTA");
    
    // TODO: Implementar rollback real
    
    return false;
}

void OTAManager::markUpdateSuccessful() {
    // Marcar la actualización como exitosa
    // Esto evitaría rollback automático en caso de bootloop
    SystemLogger.info("Actualización marcada como exitosa", "OTA");
    
    // Guardar en EEPROM o archivo
    // TODO: Implementar persistencia
}

String OTAManager::getStatistics() {
    StaticJsonDocument<512> doc;
    doc["current_version"] = versionInfo.current;
    doc["available_version"] = versionInfo.available;
    doc["state"] = getStateString();
    doc["last_check"] = lastCheckTime;
    doc["auto_update"] = autoUpdate;
    doc["attempts"] = updateAttempts;
    doc["successful"] = successfulUpdates;
    doc["failed"] = failedUpdates;
    doc["uptime"] = millis();
    
    String result;
    serializeJson(doc, result);
    return result;
}

void OTAManager::loop() {
    // Manejar ArduinoOTA si está habilitado
    handleArduinoOTA();
    
    // Verificar actualizaciones periódicamente si está en auto
    if (autoUpdate && millis() - lastCheckTime > OTA_CHECK_INTERVAL) {
        checkUpdate();
        
        // Si hay actualización y es mandatoria, instalar automáticamente
        if (state == OTA_AVAILABLE && versionInfo.mandatory) {
            SystemLogger.warning("Actualización mandatoria detectada, instalando...", "OTA");
            startUpdate(OTA_FIRMWARE);
        }
    }
}

void OTAManager::onProgress(OTAProgressCallback callback) {
    progressCallback = callback;
}

void OTAManager::onComplete(OTACompleteCallback callback) {
    completeCallback = callback;
}

void OTAManager::enableAutoUpdate(bool enable) {
    autoUpdate = enable;
    SystemLogger.info("Auto-actualización " + String(enable ? "habilitada" : "deshabilitada"), "OTA");
}

void OTAManager::enableArduinoOTA(bool enable) {
    if (enable && !arduinoOTAEnabled) {
        setupArduinoOTA();
    }
    arduinoOTAEnabled = enable;
}

String OTAManager::getUpdateUrl(OTAType type) {
    String base = updateServerUrl + "/";
    switch(type) {
        case OTA_FIRMWARE:
            return base + "firmware.bin";
        case OTA_FILESYSTEM:
            return base + "filesystem.bin";
        case OTA_CONFIG:
            return base + "config.json";
        default:
            return "";
    }
}

// === OTA SECURITY ===

OTASecurity::OTASecurity() {
    tokenExpiry = 3600;  // 1 hora por defecto
}

String OTASecurity::generateToken() {
    // Generar token aleatorio
    updateToken = "";
    for (int i = 0; i < 32; i++) {
        updateToken += String(random(0, 16), HEX);
    }
    return updateToken;
}

bool OTASecurity::validateToken(const String& token) {
    return (token.length() > 0 && token == updateToken);
}

void OTASecurity::authorizeDevice(const String& deviceId) {
    authorizedDevices.push_back(deviceId);
}

bool OTASecurity::isDeviceAuthorized(const String& deviceId) {
    for (const auto& id : authorizedDevices) {
        if (id == deviceId) return true;
    }
    return false;
}

String OTASecurity::calculateMD5(const uint8_t* data, size_t len) {
    // Calcular MD5 (simplificado)
    // En producción usar una librería de MD5 real
    return "mock_md5_hash";
}