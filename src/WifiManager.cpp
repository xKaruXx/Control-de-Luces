#include "WifiManager.h"

WifiManager WiFiMgr;
static WifiManager* wifiInstance = nullptr;

WifiManager::WifiManager() {
    wifiInstance = this;
}

void WifiManager::reconnectTimerCallback() {
    if (wifiInstance) {
        wifiInstance->handleReconnect();
    }
}

bool WifiManager::begin(const char* ssid, const char* password) {
    SystemLogger.info("Iniciando WifiManager", "WIFI");
    
    WiFi.mode(WIFI_STA);
    WiFi.hostname(HOSTNAME);
    WiFi.setAutoReconnect(false);  // Manejamos reconexión manualmente
    
    // Configurar eventos WiFi
    WiFi.onStationModeGotIP([this](const WiFiEventStationModeGotIP& event) {
        currentState = WIFI_STATE_CONNECTED;
        reconnectAttempts = 0;
        totalConnections++;
        lastConnectedTime = millis();
        
        SystemLogger.info("WiFi conectado - IP: " + event.ip.toString(), "WIFI");
        SystemLogger.info("RSSI: " + String(WiFi.RSSI()) + " dBm", "WIFI");
        
        if (onConnectCallback) onConnectCallback();
    });
    
    WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected& event) {
        if (currentState == WIFI_STATE_CONNECTED) {
            connectedTime += millis() - lastConnectedTime;
            totalDisconnections++;
        }
        
        currentState = WIFI_STATE_DISCONNECTED;
        SystemLogger.warning("WiFi desconectado. Razón: " + String(event.reason), "WIFI");
        
        if (onDisconnectCallback) onDisconnectCallback();
        
        // Programar reconexión
        if (reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
            reconnectTimer.once(WIFI_RECONNECT_INTERVAL / 1000, reconnectTimerCallback);
        } else {
            SystemLogger.error("Máximo de intentos de reconexión alcanzado", "WIFI");
            currentState = WIFI_STATE_ERROR;
        }
    });
    
    // Intentar conectar
    WiFi.begin(ssid, password);
    currentState = WIFI_STATE_CONNECTING;
    connectionStartTime = millis();
    
    SystemLogger.info("Conectando a SSID: " + String(ssid), "WIFI");
    
    // Esperar conexión
    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        return true;
    } else {
        Serial.println();
        SystemLogger.error("No se pudo conectar a WiFi", "WIFI");
        currentState = WIFI_STATE_DISCONNECTED;
        return false;
    }
}

void WifiManager::loop() {
    // Verificar estado de conexión
    if (currentState == WIFI_STATE_CONNECTING) {
        if (millis() - connectionStartTime > WIFI_CONNECT_TIMEOUT) {
            SystemLogger.warning("Timeout de conexión WiFi", "WIFI");
            currentState = WIFI_STATE_DISCONNECTED;
            reconnectAttempts++;
        }
    }
    
    // Manejar modo AP si está habilitado
    if (apModeEnabled && currentState == WIFI_STATE_ERROR) {
        enableAPMode();
    }
}

bool WifiManager::connect() {
    if (currentState == WIFI_STATE_CONNECTED) return true;
    
    currentState = WIFI_STATE_CONNECTING;
    connectionStartTime = millis();
    lastConnectAttempt = millis();
    
    WiFi.reconnect();
    return true;
}

void WifiManager::disconnect() {
    SystemLogger.info("Desconectando WiFi", "WIFI");
    WiFi.disconnect(true);
    currentState = WIFI_STATE_DISCONNECTED;
}

bool WifiManager::reconnect() {
    SystemLogger.info("Intentando reconectar WiFi. Intento: " + String(reconnectAttempts + 1), "WIFI");
    totalReconnects++;
    return connect();
}

void WifiManager::handleReconnect() {
    if (currentState != WIFI_STATE_CONNECTED && reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
        reconnectAttempts++;
        reconnect();
    }
}

void WifiManager::enableAPMode() {
    if (apModeEnabled) return;
    
    SystemLogger.info("Habilitando modo AP", "WIFI");
    
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("ESP8266-Config", "12345678");
    
    IPAddress apIP = WiFi.softAPIP();
    SystemLogger.info("Modo AP activo. IP: " + apIP.toString(), "WIFI");
    
    currentState = WIFI_STATE_AP_MODE;
    apModeEnabled = true;
}

void WifiManager::disableAPMode() {
    if (!apModeEnabled) return;
    
    SystemLogger.info("Deshabilitando modo AP", "WIFI");
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apModeEnabled = false;
}

String WifiManager::getWifiStats() {
    StaticJsonDocument<1024> doc;
    
    doc["state"] = currentState;
    doc["connected"] = isConnected();
    doc["ssid"] = WiFi.SSID();
    doc["ip"] = WiFi.localIP().toString();
    doc["mac"] = WiFi.macAddress();
    doc["rssi"] = WiFi.RSSI();
    doc["channel"] = WiFi.channel();
    doc["hostname"] = WiFi.hostname();
    
    // Estadísticas
    doc["total_connections"] = totalConnections;
    doc["total_disconnections"] = totalDisconnections;
    doc["total_reconnects"] = totalReconnects;
    doc["reconnect_attempts"] = reconnectAttempts;
    doc["uptime_seconds"] = (connectedTime + (isConnected() ? millis() - lastConnectedTime : 0)) / 1000;
    
    // Modo AP
    doc["ap_enabled"] = apModeEnabled;
    if (apModeEnabled) {
        doc["ap_ip"] = WiFi.softAPIP().toString();
        doc["ap_clients"] = WiFi.softAPgetStationNum();
    }
    
    String result;
    serializeJson(doc, result);
    return result;
}

String WifiManager::getConnectionInfo() {
    if (!isConnected()) {
        return "{\"error\":\"No conectado\"}";
    }
    
    StaticJsonDocument<512> doc;
    doc["ssid"] = WiFi.SSID();
    doc["bssid"] = WiFi.BSSIDstr();
    doc["ip"] = WiFi.localIP().toString();
    doc["subnet"] = WiFi.subnetMask().toString();
    doc["gateway"] = WiFi.gatewayIP().toString();
    doc["dns1"] = WiFi.dnsIP().toString();
    doc["dns2"] = WiFi.dnsIP(1).toString();
    doc["rssi"] = WiFi.RSSI();
    doc["signal_strength"] = map(WiFi.RSSI(), -100, -40, 0, 100);
    
    String result;
    serializeJson(doc, result);
    return result;
}

void WifiManager::setAutoReconnect(bool enable) {
    WiFi.setAutoReconnect(enable);
}

void WifiManager::setReconnectInterval(uint32_t interval) {
    // Implementar si es necesario cambiar el intervalo dinámicamente
}