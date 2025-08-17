#include "MQTTManager.h"

MQTTManager MQTT;

// Constructor
MQTTManager::MQTTManager() : mqttClient(wifiClient) {
    state = MQTT_DISCONNECTED;
    nodeType = NODE_CENTRAL;
    autoDiscovery = true;
    brokerPort = 1883;
    lastReconnectAttempt = 0;
    lastDiscoveryBroadcast = 0;
    lastHeartbeat = 0;
}

// === INICIALIZACIÓN ===

bool MQTTManager::begin(const String& broker, uint16_t port) {
    brokerIP = broker;
    brokerPort = port;
    
    SystemLogger.info("Iniciando MQTT Manager - Broker: " + broker + ":" + String(port), "MQTT");
    
    // Configurar cliente
    mqttClient.setServer(brokerIP.c_str(), brokerPort);
    mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
    
    // Configurar callback principal
    mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->handleMessage(topic, payload, length);
    });
    
    // Generar ID único
    clientId = generateClientId();
    nodeId = clientId;  // Por defecto usar clientId como nodeId
    
    SystemLogger.info("MQTT Client ID: " + clientId, "MQTT");
    
    return connect();
}

bool MQTTManager::begin(const String& broker, uint16_t port, const String& user, const String& pass) {
    username = user;
    password = pass;
    return begin(broker, port);
}

void MQTTManager::setNodeInfo(const String& id, NodeType type) {
    nodeId = id;
    nodeType = type;
    
    String typeStr = "";
    switch(type) {
        case NODE_CENTRAL: typeStr = "CENTRAL"; break;
        case NODE_LUMINARIA: typeStr = "LUMINARIA"; break;
        case NODE_GATEWAY: typeStr = "GATEWAY"; break;
        case NODE_SENSOR: typeStr = "SENSOR"; break;
    }
    
    SystemLogger.info("Nodo configurado - ID: " + id + ", Tipo: " + typeStr, "MQTT");
}

String MQTTManager::generateClientId() {
    return String("ESP_") + String(ESP.getChipId()) + "_" + String(millis());
}

// === CONEXIÓN ===

bool MQTTManager::connect() {
    return connect(clientId);
}

bool MQTTManager::connect(const String& id) {
    if (WiFi.status() != WL_CONNECTED) {
        SystemLogger.error("WiFi no conectado, no se puede conectar a MQTT", "MQTT");
        state = MQTT_ERROR;
        return false;
    }
    
    state = MQTT_CONNECTING;
    SystemLogger.info("Conectando a MQTT broker...", "MQTT");
    
    bool connected = false;
    
    // Preparar mensaje LWT (Last Will and Testament)
    String willTopic = buildTopic("status/" + nodeId);
    String willMessage = "{\"online\":false,\"reason\":\"unexpected_disconnect\"}";
    
    if (username.length() > 0) {
        connected = mqttClient.connect(id.c_str(), username.c_str(), password.c_str(),
                                      willTopic.c_str(), 1, true, willMessage.c_str());
    } else {
        connected = mqttClient.connect(id.c_str(), willTopic.c_str(), 1, true, willMessage.c_str());
    }
    
    if (connected) {
        state = MQTT_CONNECTED;
        SystemLogger.info("Conectado a MQTT broker", "MQTT");
        
        // Publicar estado online
        publishStatus("online");
        
        // Suscribirse a topics básicos
        subscribe(buildTopic("cmd/" + nodeId + "/#"));
        subscribe(buildTopic("cmd/all/#"));
        subscribe(MQTT_DISCOVERY_TOPIC);
        
        // Broadcast discovery si está habilitado
        if (autoDiscovery) {
            broadcastDiscovery();
        }
        
        // Notificar callbacks
        for (auto& callback : connectionCallbacks) {
            callback(true);
        }
        
        return true;
    } else {
        state = MQTT_ERROR;
        SystemLogger.error("Fallo al conectar a MQTT. Estado: " + String(mqttClient.state()), "MQTT");
        
        // Notificar callbacks
        for (auto& callback : connectionCallbacks) {
            callback(false);
        }
        
        return false;
    }
}

void MQTTManager::disconnect() {
    if (mqttClient.connected()) {
        publishStatus("offline");
        mqttClient.disconnect();
    }
    state = MQTT_DISCONNECTED;
    SystemLogger.info("Desconectado de MQTT", "MQTT");
}

bool MQTTManager::isConnected() {
    return mqttClient.connected();
}

bool MQTTManager::reconnect() {
    if (!mqttClient.connected()) {
        uint32_t now = millis();
        if (now - lastReconnectAttempt > MQTT_RECONNECT_DELAY) {
            lastReconnectAttempt = now;
            SystemLogger.info("Intentando reconectar a MQTT...", "MQTT");
            return connect();
        }
    }
    return false;
}

// === PUBLICACIÓN ===

bool MQTTManager::publish(const String& topic, const String& payload, bool retained) {
    if (!mqttClient.connected()) {
        // Agregar a cola si no está conectado
        MQTTMessage msg;
        msg.topic = topic;
        msg.payload = payload;
        msg.qos = 0;
        msg.retained = retained;
        msg.timestamp = millis();
        outgoingQueue.push_back(msg);
        
        SystemLogger.warning("MQTT desconectado, mensaje en cola: " + topic, "MQTT");
        return false;
    }
    
    bool result = mqttClient.publish(topic.c_str(), payload.c_str(), retained);
    
    if (result) {
        SystemLogger.debug("Publicado: " + topic + " = " + payload.substring(0, 50), "MQTT");
    } else {
        SystemLogger.error("Error al publicar: " + topic, "MQTT");
    }
    
    return result;
}

bool MQTTManager::publish(const String& topic, const JsonDocument& doc, bool retained) {
    String payload;
    serializeJson(doc, payload);
    return publish(topic, payload, retained);
}

bool MQTTManager::publishStatus(const String& status) {
    StaticJsonDocument<256> doc;
    doc["nodeId"] = nodeId;
    doc["status"] = status;
    doc["timestamp"] = millis();
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["heap"] = ESP.getFreeHeap();
    
    return publish(buildTopic("status/" + nodeId), doc, true);
}

bool MQTTManager::publishTelemetry(const JsonDocument& telemetry) {
    return publish(buildTopic("telemetry/" + nodeId), telemetry, false);
}

bool MQTTManager::publishCommand(const String& target, const String& command, const JsonDocument& params) {
    StaticJsonDocument<512> doc;
    doc["from"] = nodeId;
    doc["command"] = command;
    doc["params"] = params;
    doc["timestamp"] = millis();
    
    return publish(buildTopic("cmd/" + target), doc, false);
}

// === SUSCRIPCIÓN ===

bool MQTTManager::subscribe(const String& topic, MessageCallback callback) {
    if (callback) {
        topicCallbacks[topic] = callback;
    }
    return subscribe(topic, MQTT_QOS_1);
}

bool MQTTManager::subscribe(const String& topic, uint8_t qos) {
    if (!mqttClient.connected()) {
        SystemLogger.warning("MQTT desconectado, no se puede suscribir a: " + topic, "MQTT");
        return false;
    }
    
    bool result = mqttClient.subscribe(topic.c_str(), qos);
    
    if (result) {
        subscribedTopics.push_back(topic);
        SystemLogger.info("Suscrito a: " + topic, "MQTT");
    } else {
        SystemLogger.error("Error al suscribirse a: " + topic, "MQTT");
    }
    
    return result;
}

bool MQTTManager::unsubscribe(const String& topic) {
    if (!mqttClient.connected()) {
        return false;
    }
    
    bool result = mqttClient.unsubscribe(topic.c_str());
    
    if (result) {
        // Remover de la lista
        subscribedTopics.erase(
            std::remove(subscribedTopics.begin(), subscribedTopics.end(), topic),
            subscribedTopics.end()
        );
        
        // Remover callback si existe
        topicCallbacks.erase(topic);
        
        SystemLogger.info("Desuscrito de: " + topic, "MQTT");
    }
    
    return result;
}

void MQTTManager::onMessage(const String& topic, MessageCallback callback) {
    topicCallbacks[topic] = callback;
}

// === MANEJO DE MENSAJES ===

void MQTTManager::handleMessage(char* topic, byte* payload, unsigned int length) {
    // Convertir payload a String
    char* buffer = new char[length + 1];
    memcpy(buffer, payload, length);
    buffer[length] = '\0';
    String payloadStr = String(buffer);
    delete[] buffer;
    
    String topicStr = String(topic);
    
    SystemLogger.debug("Mensaje recibido: " + topicStr + " = " + payloadStr.substring(0, 100), "MQTT");
    
    // Agregar a cola de entrada
    MQTTMessage msg;
    msg.topic = topicStr;
    msg.payload = payloadStr;
    msg.timestamp = millis();
    incomingQueue.push_back(msg);
    
    // Procesar discovery
    if (topicStr == MQTT_DISCOVERY_TOPIC) {
        processDiscoveryMessage(payloadStr);
        return;
    }
    
    // Buscar callback específico
    for (auto& pair : topicCallbacks) {
        if (topicStr.startsWith(pair.first) || pair.first.endsWith("#")) {
            // Verificar si coincide con wildcard
            if (pair.first.endsWith("#")) {
                String base = pair.first.substring(0, pair.first.length() - 1);
                if (topicStr.startsWith(base)) {
                    pair.second(topicStr, payloadStr);
                }
            } else if (pair.first == topicStr) {
                pair.second(topicStr, payloadStr);
            }
        }
    }
}

// === DISCOVERY ===

void MQTTManager::enableAutoDiscovery(bool enable) {
    autoDiscovery = enable;
    if (enable && mqttClient.connected()) {
        broadcastDiscovery();
    }
}

void MQTTManager::broadcastDiscovery() {
    StaticJsonDocument<512> doc;
    doc["nodeId"] = nodeId;
    doc["type"] = (int)nodeType;
    doc["ip"] = WiFi.localIP().toString();
    doc["mac"] = WiFi.macAddress();
    doc["version"] = FIRMWARE_VERSION;
    doc["capabilities"]["ota"] = true;
    doc["capabilities"]["telemetry"] = true;
    doc["capabilities"]["commands"] = true;
    
    publish(MQTT_DISCOVERY_TOPIC, doc, false);
    lastDiscoveryBroadcast = millis();
    
    SystemLogger.info("Discovery broadcast enviado", "MQTT");
}

void MQTTManager::processDiscoveryMessage(const String& payload) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        SystemLogger.error("Error al parsear discovery: " + String(error.c_str()), "MQTT");
        return;
    }
    
    String discoveredNodeId = doc["nodeId"].as<String>();
    
    // No procesar nuestro propio discovery
    if (discoveredNodeId == nodeId) {
        return;
    }
    
    // Crear/actualizar info del nodo
    NodeInfo node;
    node.nodeId = discoveredNodeId;
    node.type = (NodeType)doc["type"].as<int>();
    node.ip = doc["ip"].as<String>();
    node.mac = doc["mac"].as<String>();
    node.version = doc["version"].as<String>();
    node.lastSeen = millis();
    node.online = true;
    node.metadata = doc["capabilities"];
    
    // Guardar en mapa
    bool isNew = (discoveredNodes.find(discoveredNodeId) == discoveredNodes.end());
    discoveredNodes[discoveredNodeId] = node;
    
    if (isNew) {
        SystemLogger.info("Nuevo nodo descubierto: " + discoveredNodeId + " (" + node.ip + ")", "MQTT");
        
        // Notificar callbacks
        for (auto& callback : discoveryCallbacks) {
            callback(node);
        }
    } else {
        SystemLogger.debug("Nodo actualizado: " + discoveredNodeId, "MQTT");
    }
}

void MQTTManager::onNodeDiscovered(DiscoveryCallback callback) {
    discoveryCallbacks.push_back(callback);
}

std::vector<NodeInfo> MQTTManager::getDiscoveredNodes() {
    std::vector<NodeInfo> nodes;
    for (auto& pair : discoveredNodes) {
        nodes.push_back(pair.second);
    }
    return nodes;
}

NodeInfo MQTTManager::getNodeInfo(const String& nodeId) {
    if (discoveredNodes.find(nodeId) != discoveredNodes.end()) {
        return discoveredNodes[nodeId];
    }
    return NodeInfo();
}

// === COMANDOS ===

bool MQTTManager::broadcastCommand(const String& command, const JsonDocument& params) {
    return publishCommand("all", command, params);
}

bool MQTTManager::sendCommandToNode(const String& nodeId, const String& command, const JsonDocument& params) {
    return publishCommand(nodeId, command, params);
}

// === ZONAS ===

bool MQTTManager::subscribeToZone(const String& zoneId) {
    return subscribe(buildTopic("zone/" + zoneId + "/#"));
}

bool MQTTManager::publishToZone(const String& zoneId, const String& message) {
    return publish(buildTopic("zone/" + zoneId), message, false);
}

// === OTA ===

bool MQTTManager::publishOTANotification(const String& version, const String& url) {
    StaticJsonDocument<256> doc;
    doc["version"] = version;
    doc["url"] = url;
    doc["timestamp"] = millis();
    
    return publish(buildTopic("ota/available"), doc, true);
}

bool MQTTManager::requestOTAUpdate(const String& nodeId, const String& version) {
    StaticJsonDocument<256> params;
    params["version"] = version;
    
    return publishCommand(nodeId, "ota_update", params);
}

// === CALLBACKS ===

void MQTTManager::onConnect(ConnectionCallback callback) {
    connectionCallbacks.push_back(callback);
}

void MQTTManager::onDisconnect(ConnectionCallback callback) {
    connectionCallbacks.push_back(callback);
}

// === UTILIDADES ===

String MQTTManager::buildTopic(const String& subtopic) {
    return String(MQTT_BASE_TOPIC) + "/" + subtopic;
}

void MQTTManager::sendHeartbeat() {
    StaticJsonDocument<128> doc;
    doc["nodeId"] = nodeId;
    doc["timestamp"] = millis();
    doc["heap"] = ESP.getFreeHeap();
    
    publish(buildTopic("heartbeat/" + nodeId), doc, false);
}

// === LOOP ===

void MQTTManager::loop() {
    if (!mqttClient.connected()) {
        reconnect();
    } else {
        mqttClient.loop();
        
        // Procesar colas
        processOutgoingQueue();
        processIncomingQueue();
        
        // Heartbeat periódico
        if (millis() - lastHeartbeat > 30000) {  // Cada 30 segundos
            sendHeartbeat();
            lastHeartbeat = millis();
        }
        
        // Discovery periódico
        if (autoDiscovery && millis() - lastDiscoveryBroadcast > 300000) {  // Cada 5 minutos
            broadcastDiscovery();
        }
        
        // Limpiar nodos offline
        uint32_t now = millis();
        for (auto& pair : discoveredNodes) {
            if (now - pair.second.lastSeen > 600000) {  // 10 minutos sin actividad
                pair.second.online = false;
            }
        }
    }
}

void MQTTManager::processOutgoingQueue() {
    if (outgoingQueue.empty() || !mqttClient.connected()) {
        return;
    }
    
    // Enviar hasta 5 mensajes por loop
    int sent = 0;
    while (!outgoingQueue.empty() && sent < 5) {
        MQTTMessage msg = outgoingQueue.front();
        
        if (publish(msg.topic, msg.payload, msg.retained)) {
            outgoingQueue.erase(outgoingQueue.begin());
            sent++;
        } else {
            break;  // Si falla, intentar en el próximo loop
        }
    }
}

void MQTTManager::processIncomingQueue() {
    // Procesar hasta 10 mensajes por loop
    int processed = 0;
    while (!incomingQueue.empty() && processed < 10) {
        incomingQueue.erase(incomingQueue.begin());
        processed++;
    }
}

// === ESTADÍSTICAS ===

String MQTTManager::getStatistics() {
    StaticJsonDocument<512> doc;
    doc["connected"] = mqttClient.connected();
    doc["state"] = (int)state;
    doc["broker"] = brokerIP + ":" + String(brokerPort);
    doc["clientId"] = clientId;
    doc["nodeId"] = nodeId;
    doc["discoveredNodes"] = discoveredNodes.size();
    doc["subscribedTopics"] = subscribedTopics.size();
    doc["outgoingQueue"] = outgoingQueue.size();
    doc["incomingQueue"] = incomingQueue.size();
    
    String result;
    serializeJson(doc, result);
    return result;
}

void MQTTManager::printStatus() {
    SystemLogger.info("=== MQTT Status ===", "MQTT");
    SystemLogger.info("Connected: " + String(mqttClient.connected() ? "Yes" : "No"), "MQTT");
    SystemLogger.info("Broker: " + brokerIP + ":" + String(brokerPort), "MQTT");
    SystemLogger.info("Node ID: " + nodeId, "MQTT");
    SystemLogger.info("Discovered Nodes: " + String(discoveredNodes.size()), "MQTT");
    SystemLogger.info("Subscribed Topics: " + String(subscribedTopics.size()), "MQTT");
}