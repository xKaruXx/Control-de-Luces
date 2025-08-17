#include "AlertManager.h"
#include <ESP8266HTTPClient.h>

AlertManager Alerts;
NotificationManager Notifications;

// === ALERT MANAGER ===

AlertManager::AlertManager() {
    nextAlertId = 1;
    enabled = false;
    
    // Umbrales por defecto
    consumptionHighThreshold = 150.0;  // 150W
    consumptionLowThreshold = 10.0;    // 10W
    offlineTimeout = 300;              // 5 minutos
    zoneFailureThreshold = 3;          // 3 fallas en zona
}

bool AlertManager::begin() {
    SystemLogger.info("Iniciando AlertManager", "ALERT");
    
    // Habilitar sistema de alertas
    enable(true);
    
    // Agregar condiciones predefinidas
    addCondition("Alto Consumo", ALERT_CONSUMPTION_HIGH,
                [this](String id) { 
                    float consumption = Database.getConsumptionByLuminaria(id, 1);
                    return consumption > consumptionHighThreshold;
                });
    
    addCondition("Bajo Consumo", ALERT_CONSUMPTION_LOW,
                [this](String id) {
                    float consumption = Database.getConsumptionByLuminaria(id, 1);
                    return consumption < consumptionLowThreshold && consumption > 0;
                });
    
    SystemLogger.info("AlertManager iniciado con " + String(conditions.size()) + " condiciones", "ALERT");
    return true;
}

void AlertManager::enable(bool state) {
    enabled = state;
    SystemLogger.info("Sistema de alertas " + String(enabled ? "habilitado" : "deshabilitado"), "ALERT");
}

void AlertManager::registerCallback(AlertCallback callback) {
    callbacks.push_back(callback);
}

uint32_t AlertManager::createAlert(AlertType type, AlertSeverity severity,
                                  const String& source, const String& message,
                                  const String& details) {
    Alert alert;
    alert.id = nextAlertId++;
    alert.timestamp = millis() / 1000;
    alert.type = type;
    alert.severity = severity;
    alert.source = source;
    alert.message = message;
    alert.details = details;
    alert.acknowledged = false;
    alert.acknowledgedBy = "";
    alert.acknowledgedAt = 0;
    
    activeAlerts.push_back(alert);
    
    // Limitar tamaño
    if (activeAlerts.size() > MAX_ALERTS) {
        activeAlerts.erase(activeAlerts.begin());
    }
    
    // Registrar en base de datos
    String typeStr = "";
    switch (type) {
        case ALERT_FAILURE: typeStr = "FAILURE"; break;
        case ALERT_CONSUMPTION_HIGH: typeStr = "HIGH_CONSUMPTION"; break;
        case ALERT_CONSUMPTION_LOW: typeStr = "LOW_CONSUMPTION"; break;
        case ALERT_OFFLINE: typeStr = "OFFLINE"; break;
        case ALERT_ZONE_FAILURE: typeStr = "ZONE_FAILURE"; break;
        case ALERT_MAINTENANCE: typeStr = "MAINTENANCE"; break;
        case ALERT_SYSTEM: typeStr = "SYSTEM"; break;
        case ALERT_SECURITY: typeStr = "SECURITY"; break;
    }
    
    Database.logEvent(source, EVENT_FAILURE, "ALERTA: " + message, typeStr);
    
    // Notificar callbacks
    notifyCallbacks(alert);
    
    // Log según severidad
    switch (severity) {
        case SEVERITY_CRITICAL:
            SystemLogger.error("ALERTA CRÍTICA: " + message, "ALERT");
            break;
        case SEVERITY_ERROR:
            SystemLogger.error("ALERTA ERROR: " + message, "ALERT");
            break;
        case SEVERITY_WARNING:
            SystemLogger.warning("ALERTA: " + message, "ALERT");
            break;
        default:
            SystemLogger.info("ALERTA INFO: " + message, "ALERT");
    }
    
    // Enviar notificaciones para alertas críticas
    if (severity >= SEVERITY_ERROR) {
        Notifications.sendEmail("Alerta Sistema Luces", message);
        Notifications.sendWebhook(Notifications.formatAlertWebhook(alert));
    }
    
    return alert.id;
}

void AlertManager::notifyCallbacks(const Alert& alert) {
    for (auto& callback : callbacks) {
        callback(alert);
    }
}

void AlertManager::addCondition(const String& name, AlertType type,
                               std::function<bool(String)> checkFunction,
                               const String& targetId, float threshold) {
    AlertCondition condition;
    condition.name = name;
    condition.type = type;
    condition.checkFunction = checkFunction;
    condition.targetId = targetId;
    condition.threshold = threshold;
    condition.enabled = true;
    condition.lastTriggered = 0;
    
    conditions.push_back(condition);
    SystemLogger.debug("Condición de alerta agregada: " + name, "ALERT");
}

void AlertManager::checkConditions() {
    if (!enabled) return;
    
    uint32_t now = millis();
    
    for (auto& condition : conditions) {
        if (!condition.enabled) continue;
        
        // Verificar cooldown
        if (now - condition.lastTriggered < ALERT_COOLDOWN) continue;
        
        // Verificar condición
        if (condition.checkFunction && condition.checkFunction(condition.targetId)) {
            // Crear alerta
            createAlert(condition.type, SEVERITY_WARNING,
                       condition.targetId, condition.name,
                       "Umbral: " + String(condition.threshold));
            
            condition.lastTriggered = now;
        }
    }
}

bool AlertManager::acknowledgeAlert(uint32_t alertId, const String& user) {
    for (auto& alert : activeAlerts) {
        if (alert.id == alertId) {
            alert.acknowledged = true;
            alert.acknowledgedBy = user;
            alert.acknowledgedAt = millis() / 1000;
            
            SystemLogger.info("Alerta #" + String(alertId) + " reconocida por " + user, "ALERT");
            return true;
        }
    }
    return false;
}

bool AlertManager::dismissAlert(uint32_t alertId) {
    for (auto it = activeAlerts.begin(); it != activeAlerts.end(); ++it) {
        if (it->id == alertId) {
            // Mover a historial
            alertHistory.push_back(*it);
            
            // Limitar historial
            if (alertHistory.size() > 100) {
                alertHistory.erase(alertHistory.begin());
            }
            
            // Eliminar de activas
            activeAlerts.erase(it);
            
            SystemLogger.info("Alerta #" + String(alertId) + " descartada", "ALERT");
            return true;
        }
    }
    return false;
}

std::vector<Alert> AlertManager::getActiveAlerts() {
    return activeAlerts;
}

std::vector<Alert> AlertManager::getAlertsByType(AlertType type) {
    std::vector<Alert> filtered;
    for (const auto& alert : activeAlerts) {
        if (alert.type == type) {
            filtered.push_back(alert);
        }
    }
    return filtered;
}

std::vector<Alert> AlertManager::getAlertsBySeverity(AlertSeverity severity) {
    std::vector<Alert> filtered;
    for (const auto& alert : activeAlerts) {
        if (alert.severity == severity) {
            filtered.push_back(alert);
        }
    }
    return filtered;
}

uint32_t AlertManager::getUnacknowledgedCount() {
    uint32_t count = 0;
    for (const auto& alert : activeAlerts) {
        if (!alert.acknowledged) count++;
    }
    return count;
}

void AlertManager::checkLuminariaFailure(const String& luminariaId, const String& estado) {
    if (estado == "falla") {
        createAlert(ALERT_FAILURE, SEVERITY_ERROR,
                   luminariaId, "Falla detectada en luminaria " + luminariaId);
    }
}

void AlertManager::checkConsumption(const String& luminariaId, float consumption) {
    if (consumption > consumptionHighThreshold) {
        createAlert(ALERT_CONSUMPTION_HIGH, SEVERITY_WARNING,
                   luminariaId, "Consumo alto: " + String(consumption) + "W",
                   "Umbral: " + String(consumptionHighThreshold) + "W");
    } else if (consumption < consumptionLowThreshold && consumption > 0) {
        createAlert(ALERT_CONSUMPTION_LOW, SEVERITY_WARNING,
                   luminariaId, "Consumo bajo: " + String(consumption) + "W",
                   "Posible falla. Umbral: " + String(consumptionLowThreshold) + "W");
    }
}

void AlertManager::checkZoneHealth(uint32_t zoneId) {
    auto luminarias = Database.getLuminariasInZone(zoneId);
    uint8_t failureCount = 0;
    
    for (const auto& lumId : luminarias) {
        auto events = Database.getEventsByLuminaria(lumId, 10);
        for (const auto& event : events) {
            if (event.type == EVENT_FAILURE) {
                failureCount++;
                break;
            }
        }
    }
    
    if (failureCount >= zoneFailureThreshold) {
        Zone zone = Database.getZone(zoneId);
        createAlert(ALERT_ZONE_FAILURE, SEVERITY_ERROR,
                   "ZONE_" + String(zoneId),
                   "Múltiples fallas en zona: " + zone.name,
                   String(failureCount) + " luminarias con fallas");
    }
}

void AlertManager::checkSystemHealth() {
    // Verificar memoria
    if (ESP.getFreeHeap() < 5000) {
        createAlert(ALERT_SYSTEM, SEVERITY_CRITICAL,
                   "SYSTEM", "Memoria crítica: " + String(ESP.getFreeHeap()) + " bytes");
    }
    
    // Verificar WiFi
    if (WiFi.status() != WL_CONNECTED) {
        createAlert(ALERT_SYSTEM, SEVERITY_ERROR,
                   "WIFI", "Conexión WiFi perdida");
    }
}

String AlertManager::getAlertStats() {
    StaticJsonDocument<512> doc;
    
    uint32_t criticalCount = 0, errorCount = 0, warningCount = 0, infoCount = 0;
    
    for (const auto& alert : activeAlerts) {
        switch (alert.severity) {
            case SEVERITY_CRITICAL: criticalCount++; break;
            case SEVERITY_ERROR: errorCount++; break;
            case SEVERITY_WARNING: warningCount++; break;
            case SEVERITY_INFO: infoCount++; break;
        }
    }
    
    doc["total_active"] = activeAlerts.size();
    doc["unacknowledged"] = getUnacknowledgedCount();
    doc["critical"] = criticalCount;
    doc["error"] = errorCount;
    doc["warning"] = warningCount;
    doc["info"] = infoCount;
    doc["history_size"] = alertHistory.size();
    doc["conditions"] = conditions.size();
    
    String result;
    serializeJson(doc, result);
    return result;
}

String AlertManager::getAlertsJson() {
    StaticJsonDocument<4096> doc;
    JsonArray arr = doc.to<JsonArray>();
    
    for (const auto& alert : activeAlerts) {
        JsonObject obj = arr.createNestedObject();
        obj["id"] = alert.id;
        obj["timestamp"] = alert.timestamp;
        obj["type"] = alert.type;
        obj["severity"] = alert.severity;
        obj["source"] = alert.source;
        obj["message"] = alert.message;
        obj["details"] = alert.details;
        obj["acknowledged"] = alert.acknowledged;
        if (alert.acknowledged) {
            obj["acknowledged_by"] = alert.acknowledgedBy;
            obj["acknowledged_at"] = alert.acknowledgedAt;
        }
    }
    
    String result;
    serializeJson(doc, result);
    return result;
}

// === NOTIFICATION MANAGER ===

NotificationManager::NotificationManager() {
    emailEnabled = false;
    smsEnabled = false;
    webhookEnabled = false;
}

bool NotificationManager::begin() {
    SystemLogger.info("Iniciando NotificationManager", "NOTIFY");
    
    // Cargar configuración desde archivo si existe
    // Por ahora usar valores por defecto
    
    return true;
}

void NotificationManager::configureEmail(const String& server, uint16_t port,
                                        const String& user, const String& password) {
    emailServer = server;
    emailPort = port;
    emailUser = user;
    emailPassword = password;
    SystemLogger.info("Email configurado: " + server + ":" + String(port), "NOTIFY");
}

void NotificationManager::configureWebhook(const String& url, const String& token) {
    webhookUrl = url;
    webhookToken = token;
    SystemLogger.info("Webhook configurado: " + url, "NOTIFY");
}

bool NotificationManager::sendEmail(const String& subject, const String& body) {
    if (!emailEnabled || emailServer.length() == 0) return false;
    
    // En un ESP8266 real, usar librería de email
    SystemLogger.info("Email simulado: " + subject, "NOTIFY");
    return true;
}

bool NotificationManager::sendWebhook(const String& payload) {
    if (!webhookEnabled || webhookUrl.length() == 0) return false;
    
    HTTPClient http;
    WiFiClient client;
    
    http.begin(client, webhookUrl);
    http.addHeader("Content-Type", "application/json");
    if (webhookToken.length() > 0) {
        http.addHeader("Authorization", "Bearer " + webhookToken);
    }
    
    int httpCode = http.POST(payload);
    
    if (httpCode > 0) {
        SystemLogger.info("Webhook enviado. Código: " + String(httpCode), "NOTIFY");
        http.end();
        return httpCode >= 200 && httpCode < 300;
    } else {
        SystemLogger.error("Error enviando webhook: " + http.errorToString(httpCode), "NOTIFY");
        http.end();
        return false;
    }
}

String NotificationManager::formatAlertEmail(const Alert& alert) {
    String body = "Sistema de Control de Alumbrado Público\n";
    body += "=====================================\n\n";
    body += "ALERTA #" + String(alert.id) + "\n";
    body += "Hora: " + String(alert.timestamp) + "\n";
    body += "Severidad: ";
    
    switch (alert.severity) {
        case SEVERITY_CRITICAL: body += "CRÍTICA"; break;
        case SEVERITY_ERROR: body += "ERROR"; break;
        case SEVERITY_WARNING: body += "ADVERTENCIA"; break;
        case SEVERITY_INFO: body += "INFORMACIÓN"; break;
    }
    
    body += "\n\nMensaje: " + alert.message;
    body += "\nFuente: " + alert.source;
    if (alert.details.length() > 0) {
        body += "\nDetalles: " + alert.details;
    }
    
    return body;
}

String NotificationManager::formatAlertWebhook(const Alert& alert) {
    StaticJsonDocument<512> doc;
    
    doc["alert_id"] = alert.id;
    doc["timestamp"] = alert.timestamp;
    doc["type"] = alert.type;
    doc["severity"] = alert.severity;
    doc["source"] = alert.source;
    doc["message"] = alert.message;
    doc["details"] = alert.details;
    
    String payload;
    serializeJson(doc, payload);
    return payload;
}