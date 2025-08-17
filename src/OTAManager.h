#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "config.h"
#include "Logger.h"
#include "MQTTManager.h"

// Configuración OTA
#define OTA_PORT 8266
#define OTA_PASSWORD "ota_secure_password"
#define OTA_HOSTNAME_PREFIX "ESP_OTA_"
#define OTA_CHECK_INTERVAL 3600000  // Verificar actualizaciones cada hora
#define OTA_MAX_RETRIES 3

// Estados OTA
enum OTAState {
    OTA_IDLE,
    OTA_CHECKING,
    OTA_AVAILABLE,
    OTA_DOWNLOADING,
    OTA_INSTALLING,
    OTA_SUCCESS,
    OTA_ERROR
};

// Tipos de actualización
enum OTAType {
    OTA_FIRMWARE,
    OTA_FILESYSTEM,
    OTA_CONFIG
};

// Información de versión
struct VersionInfo {
    String current;
    String available;
    String changelog;
    String downloadUrl;
    uint32_t size;
    String md5;
    bool mandatory;
};

// Callback types
typedef std::function<void(OTAState state, int progress)> OTAProgressCallback;
typedef std::function<void(bool success, String message)> OTACompleteCallback;

class OTAManager {
private:
    OTAState state;
    VersionInfo versionInfo;
    String updateServerUrl;
    String deviceId;
    bool autoUpdate;
    bool arduinoOTAEnabled;
    
    // Callbacks
    OTAProgressCallback progressCallback;
    OTACompleteCallback completeCallback;
    
    // Estadísticas
    uint32_t lastCheckTime;
    uint32_t updateAttempts;
    uint32_t successfulUpdates;
    uint32_t failedUpdates;
    
    // Métodos privados
    bool checkForUpdates();
    bool downloadAndInstall(const String& url, OTAType type);
    bool verifyUpdate(const String& md5);
    void notifyProgress(int progress);
    void notifyComplete(bool success, const String& message);
    bool parseVersionInfo(const String& json);
    String getUpdateUrl(OTAType type);
    
public:
    OTAManager();
    
    // Inicialización
    bool begin(const String& serverUrl = "");
    void setDeviceId(const String& id);
    void enableAutoUpdate(bool enable = true);
    
    // ArduinoOTA (para desarrollo)
    void enableArduinoOTA(bool enable = true);
    void setupArduinoOTA();
    void handleArduinoOTA();
    
    // HTTP OTA (para producción)
    bool checkUpdate();
    bool startUpdate(OTAType type = OTA_FIRMWARE);
    bool startUpdate(const String& url, const String& md5 = "");
    void cancelUpdate();
    
    // Información de versión
    String getCurrentVersion() { return FIRMWARE_VERSION; }
    String getAvailableVersion() { return versionInfo.available; }
    bool isUpdateAvailable() { return state == OTA_AVAILABLE; }
    VersionInfo getVersionInfo() { return versionInfo; }
    
    // MQTT OTA
    void publishOTAStatus();
    void handleMQTTOTACommand(const JsonDocument& command);
    bool requestNodeUpdate(const String& nodeId, const String& version);
    
    // Callbacks
    void onProgress(OTAProgressCallback callback);
    void onComplete(OTACompleteCallback callback);
    
    // Estado
    OTAState getState() { return state; }
    String getStateString();
    bool isUpdating() { return state == OTA_DOWNLOADING || state == OTA_INSTALLING; }
    
    // Rollback
    bool canRollback();
    bool rollback();
    void markUpdateSuccessful();
    
    // Estadísticas
    String getStatistics();
    uint32_t getLastCheckTime() { return lastCheckTime; }
    uint32_t getUpdateAttempts() { return updateAttempts; }
    
    // Loop
    void loop();
};

// Instancia global
extern OTAManager OTA;

// === FUNCIONES AUXILIARES ===

class OTAServer {
private:
    AsyncWebServer* server;
    bool running;
    String uploadPath;
    File uploadFile;
    
public:
    OTAServer(AsyncWebServer* webServer);
    
    bool begin();
    void stop();
    
    // Endpoints
    void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    void handleUpdateStatus(AsyncWebServerRequest *request);
    void handleVersionCheck(AsyncWebServerRequest *request);
    
    // Utilidades
    bool validateFirmware(uint8_t* data, size_t len);
    String generateUpdateResponse(bool success, const String& message);
};

// === CONFIGURACIÓN DE SEGURIDAD OTA ===

class OTASecurity {
private:
    String updateToken;
    uint32_t tokenExpiry;
    std::vector<String> authorizedDevices;
    
public:
    OTASecurity();
    
    // Token management
    String generateToken();
    bool validateToken(const String& token);
    void setTokenExpiry(uint32_t seconds);
    
    // Device authorization
    void authorizeDevice(const String& deviceId);
    void revokeDevice(const String& deviceId);
    bool isDeviceAuthorized(const String& deviceId);
    
    // Signature verification
    bool verifySignature(const uint8_t* data, size_t len, const String& signature);
    String calculateMD5(const uint8_t* data, size_t len);
};

extern OTASecurity OTASecure;

#endif // OTA_MANAGER_H