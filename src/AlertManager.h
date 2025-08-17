#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "config.h"
#include "Logger.h"
#include "DatabaseManager.h"

// Configuración de alertas
#define MAX_ALERTS 50
#define ALERT_CHECK_INTERVAL 10000  // Verificar cada 10 segundos
#define ALERT_COOLDOWN 300000      // 5 minutos entre alertas similares

// Tipos de alerta
enum AlertType {
    ALERT_FAILURE,           // Falla de luminaria
    ALERT_CONSUMPTION_HIGH,  // Consumo alto
    ALERT_CONSUMPTION_LOW,   // Consumo bajo (posible falla)
    ALERT_OFFLINE,          // Luminaria sin respuesta
    ALERT_ZONE_FAILURE,     // Múltiples fallas en zona
    ALERT_MAINTENANCE,      // Mantenimiento requerido
    ALERT_SYSTEM,          // Alerta del sistema
    ALERT_SECURITY         // Alerta de seguridad
};

// Severidad
enum AlertSeverity {
    SEVERITY_INFO,
    SEVERITY_WARNING,
    SEVERITY_ERROR,
    SEVERITY_CRITICAL
};

// Estructura de alerta
struct Alert {
    uint32_t id;
    uint32_t timestamp;
    AlertType type;
    AlertSeverity severity;
    String source;      // ID de luminaria o zona
    String message;
    String details;
    bool acknowledged;
    String acknowledgedBy;
    uint32_t acknowledgedAt;
};

// Condición de alerta
struct AlertCondition {
    String name;
    AlertType type;
    std::function<bool(String)> checkFunction;
    String targetId;
    float threshold;
    bool enabled;
    uint32_t lastTriggered;
};

// Callback para notificaciones
typedef std::function<void(const Alert&)> AlertCallback;

class AlertManager {
private:
    std::vector<Alert> activeAlerts;
    std::vector<Alert> alertHistory;
    std::vector<AlertCondition> conditions;
    std::vector<AlertCallback> callbacks;
    
    uint32_t nextAlertId;
    bool enabled;
    
    // Umbrales
    float consumptionHighThreshold;
    float consumptionLowThreshold;
    uint32_t offlineTimeout;
    uint8_t zoneFailureThreshold;
    
    // Funciones internas
    bool shouldTriggerAlert(const AlertCondition& condition);
    void checkConditions();
    void notifyCallbacks(const Alert& alert);
    
public:
    AlertManager();
    
    // Inicialización
    bool begin();
    void enable(bool state = true);
    bool isEnabled() { return enabled; }
    
    // Callbacks
    void registerCallback(AlertCallback callback);
    void clearCallbacks();
    
    // Crear alertas
    uint32_t createAlert(AlertType type, AlertSeverity severity, 
                        const String& source, const String& message,
                        const String& details = "");
    
    // Condiciones automáticas
    void addCondition(const String& name, AlertType type,
                     std::function<bool(String)> checkFunction,
                     const String& targetId = "", float threshold = 0);
    void removeCondition(const String& name);
    void enableCondition(const String& name, bool enabled);
    
    // Gestión de alertas
    bool acknowledgeAlert(uint32_t alertId, const String& user);
    bool dismissAlert(uint32_t alertId);
    void clearAlert(uint32_t alertId);
    void clearAllAlerts();
    
    // Consultas
    std::vector<Alert> getActiveAlerts();
    std::vector<Alert> getAlertsByType(AlertType type);
    std::vector<Alert> getAlertsBySeverity(AlertSeverity severity);
    std::vector<Alert> getAlertHistory(uint32_t limit = 50);
    Alert getAlert(uint32_t alertId);
    uint32_t getActiveAlertCount();
    uint32_t getUnacknowledgedCount();
    
    // Configuración de umbrales
    void setConsumptionThresholds(float high, float low);
    void setOfflineTimeout(uint32_t seconds);
    void setZoneFailureThreshold(uint8_t count);
    
    // Verificaciones predefinidas
    void checkLuminariaFailure(const String& luminariaId, const String& estado);
    void checkConsumption(const String& luminariaId, float consumption);
    void checkZoneHealth(uint32_t zoneId);
    void checkSystemHealth();
    void checkSecurityEvents();
    
    // Notificaciones
    void sendEmailAlert(const Alert& alert);
    void sendSMSAlert(const Alert& alert);
    void sendWebhookAlert(const Alert& alert);
    
    // Estadísticas
    String getAlertStats();
    String getAlertsJson();
    
    // Mantenimiento
    void cleanOldAlerts(uint32_t daysToKeep = 7);
    void performMaintenance();
};

// Instancia global
extern AlertManager Alerts;

// === NOTIFICACIONES ===

class NotificationManager {
private:
    bool emailEnabled;
    bool smsEnabled;
    bool webhookEnabled;
    
    String emailServer;
    uint16_t emailPort;
    String emailUser;
    String emailPassword;
    String emailRecipients;
    
    String webhookUrl;
    String webhookToken;
    
public:
    NotificationManager();
    
    bool begin();
    
    // Configuración Email
    void configureEmail(const String& server, uint16_t port,
                       const String& user, const String& password);
    void setEmailRecipients(const String& recipients);
    void enableEmail(bool enabled) { emailEnabled = enabled; }
    
    // Configuración Webhook
    void configureWebhook(const String& url, const String& token);
    void enableWebhook(bool enabled) { webhookEnabled = enabled; }
    
    // Enviar notificaciones
    bool sendEmail(const String& subject, const String& body);
    bool sendWebhook(const String& payload);
    
    // Templates
    String formatAlertEmail(const Alert& alert);
    String formatAlertWebhook(const Alert& alert);
};

extern NotificationManager Notifications;

#endif // ALERT_MANAGER_H