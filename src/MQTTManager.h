#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <functional>
#include <vector>
#include <map>
#include "config.h"
#include "Logger.h"

// Configuración MQTT
#define MQTT_MAX_PACKET_SIZE 512
#define MQTT_KEEPALIVE 60
#define MQTT_RECONNECT_DELAY 5000
#define MQTT_QOS_0 0
#define MQTT_QOS_1 1
#define MQTT_QOS_2 2

// Topics base
#define MQTT_BASE_TOPIC "luces"
#define MQTT_DISCOVERY_TOPIC "luces/discovery"
#define MQTT_COMMAND_TOPIC "luces/cmd"
#define MQTT_STATUS_TOPIC "luces/status"
#define MQTT_TELEMETRY_TOPIC "luces/telemetry"
#define MQTT_CONFIG_TOPIC "luces/config"
#define MQTT_OTA_TOPIC "luces/ota"

// Tipos de nodo
enum NodeType {
    NODE_CENTRAL,     // Nodo central (servidor)
    NODE_LUMINARIA,   // Nodo luminaria individual
    NODE_GATEWAY,     // Gateway de zona
    NODE_SENSOR       // Nodo sensor
};

// Estados de conexión
enum MQTTState {
    MQTT_DISCONNECTED,
    MQTT_CONNECTING,
    MQTT_CONNECTED,
    MQTT_ERROR
};

// Estructura de mensaje MQTT
struct MQTTMessage {
    String topic;
    String payload;
    uint8_t qos;
    bool retained;
    uint32_t timestamp;
};

// Estructura de nodo
struct NodeInfo {
    String nodeId;
    NodeType type;
    String ip;
    String mac;
    String version;
    uint32_t lastSeen;
    bool online;
    JsonDocument metadata;
};

// Callbacks
typedef std::function<void(const String& topic, const String& payload)> MessageCallback;
typedef std::function<void(const NodeInfo& node)> DiscoveryCallback;
typedef std::function<void(bool connected)> ConnectionCallback;

class MQTTManager {
private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    
    // Estado
    MQTTState state;
    String clientId;
    String nodeId;
    NodeType nodeType;
    bool autoDiscovery;
    
    // Configuración
    String brokerIP;
    uint16_t brokerPort;
    String username;
    String password;
    
    // Callbacks
    std::map<String, MessageCallback> topicCallbacks;
    std::vector<DiscoveryCallback> discoveryCallbacks;
    std::vector<ConnectionCallback> connectionCallbacks;
    
    // Nodos descubiertos
    std::map<String, NodeInfo> discoveredNodes;
    
    // Timers
    uint32_t lastReconnectAttempt;
    uint32_t lastDiscoveryBroadcast;
    uint32_t lastHeartbeat;
    
    // Topics suscritos
    std::vector<String> subscribedTopics;
    
    // Buffer de mensajes
    std::vector<MQTTMessage> outgoingQueue;
    std::vector<MQTTMessage> incomingQueue;
    
    // Métodos privados
    void handleMessage(char* topic, byte* payload, unsigned int length);
    void processDiscoveryMessage(const String& payload);
    void broadcastDiscovery();
    void sendHeartbeat();
    bool reconnect();
    void processOutgoingQueue();
    void processIncomingQueue();
    String generateClientId();
    String buildTopic(const String& subtopic);
    
public:
    MQTTManager();
    
    // Inicialización
    bool begin(const String& broker, uint16_t port = 1883);
    bool begin(const String& broker, uint16_t port, const String& user, const String& pass);
    void setNodeInfo(const String& id, NodeType type);
    void enableAutoDiscovery(bool enable = true);
    
    // Conexión
    bool connect();
    bool connect(const String& clientId);
    void disconnect();
    bool isConnected();
    MQTTState getState() { return state; }
    
    // Publicación
    bool publish(const String& topic, const String& payload, bool retained = false);
    bool publish(const String& topic, const JsonDocument& doc, bool retained = false);
    bool publishStatus(const String& status);
    bool publishTelemetry(const JsonDocument& telemetry);
    bool publishCommand(const String& target, const String& command, const JsonDocument& params);
    
    // Suscripción
    bool subscribe(const String& topic, MessageCallback callback);
    bool subscribe(const String& topic, uint8_t qos = 0);
    bool unsubscribe(const String& topic);
    void onMessage(const String& topic, MessageCallback callback);
    
    // Discovery
    void onNodeDiscovered(DiscoveryCallback callback);
    std::vector<NodeInfo> getDiscoveredNodes();
    NodeInfo getNodeInfo(const String& nodeId);
    bool pingNode(const String& nodeId);
    void requestNodeStatus(const String& nodeId);
    
    // Callbacks
    void onConnect(ConnectionCallback callback);
    void onDisconnect(ConnectionCallback callback);
    
    // Topics organizados por zona
    bool subscribeToZone(const String& zoneId);
    bool publishToZone(const String& zoneId, const String& message);
    
    // Comandos globales
    bool broadcastCommand(const String& command, const JsonDocument& params);
    bool sendCommandToNode(const String& nodeId, const String& command, const JsonDocument& params);
    
    // OTA
    bool publishOTANotification(const String& version, const String& url);
    bool requestOTAUpdate(const String& nodeId, const String& version);
    
    // Gestión
    void loop();
    void setKeepAlive(uint16_t seconds);
    void setQoS(uint8_t qos);
    uint16_t getMaxPacketSize() { return MQTT_MAX_PACKET_SIZE; }
    
    // Estadísticas
    uint32_t getMessagesSent();
    uint32_t getMessagesReceived();
    uint32_t getBytesTransferred();
    String getStatistics();
    
    // Debug
    void enableDebug(bool enable);
    void printStatus();
};

// Instancia global
extern MQTTManager MQTT;

// === MQTT BROKER LOCAL ===
// Clase para ejecutar un broker MQTT básico (opcional)
class SimpleMQTTBroker {
private:
    uint16_t port;
    bool running;
    std::map<String, std::vector<WiFiClient>> topicSubscribers;
    
public:
    SimpleMQTTBroker(uint16_t port = 1883);
    bool begin();
    void stop();
    void handleClient();
    bool isRunning() { return running; }
};

#endif // MQTT_MANAGER_H