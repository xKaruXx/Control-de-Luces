#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <Ticker.h>
#include "config.h"
#include "Logger.h"

enum WifiState {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_AP_MODE,
    WIFI_STATE_ERROR
};

class WifiManager {
private:
    Ticker reconnectTimer;
    WifiState currentState = WIFI_STATE_DISCONNECTED;
    unsigned long lastConnectAttempt = 0;
    unsigned long connectionStartTime = 0;
    uint8_t reconnectAttempts = 0;
    const uint8_t MAX_RECONNECT_ATTEMPTS = 10;
    bool apModeEnabled = false;
    
    // Estadísticas
    uint32_t totalConnections = 0;
    uint32_t totalDisconnections = 0;
    uint32_t totalReconnects = 0;
    unsigned long connectedTime = 0;
    unsigned long lastConnectedTime = 0;
    
    // Callbacks
    void (*onConnectCallback)() = nullptr;
    void (*onDisconnectCallback)() = nullptr;
    
    static void reconnectTimerCallback();
    void handleReconnect();
    
public:
    WifiManager();
    
    bool begin(const char* ssid, const char* password);
    void loop();
    
    bool connect();
    void disconnect();
    bool reconnect();
    
    void enableAPMode();
    void disableAPMode();
    
    // Estado y estadísticas
    WifiState getState() { return currentState; }
    bool isConnected() { return currentState == WIFI_STATE_CONNECTED; }
    int8_t getRSSI() { return WiFi.RSSI(); }
    String getIP() { return WiFi.localIP().toString(); }
    String getMAC() { return WiFi.macAddress(); }
    uint8_t getReconnectAttempts() { return reconnectAttempts; }
    
    String getWifiStats();
    String getConnectionInfo();
    
    // Callbacks
    void onConnect(void (*callback)()) { onConnectCallback = callback; }
    void onDisconnect(void (*callback)()) { onDisconnectCallback = callback; }
    
    // Configuración
    void setAutoReconnect(bool enable);
    void setReconnectInterval(uint32_t interval);
};

extern WifiManager WiFiMgr;

#endif // WIFI_MANAGER_H