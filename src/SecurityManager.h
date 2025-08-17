#ifndef SECURITY_MANAGER_H
#define SECURITY_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include "config.h"
#include "Logger.h"
#include <map>

// Configuración de seguridad
#define RATE_LIMIT_WINDOW 60000     // Ventana de 1 minuto
#define RATE_LIMIT_MAX_REQUESTS 60  // Máximo 60 requests por minuto
#define WATCHDOG_TIMEOUT 8000        // 8 segundos
#define WATCHDOG_CHECK_INTERVAL 1000 // Verificar cada segundo
#define MAX_INPUT_LENGTH 1024        // Longitud máxima de entrada
#define BACKUP_INTERVAL 3600000      // Backup cada hora

// Validación de entradas
enum InputType {
    INPUT_TYPE_TEXT,
    INPUT_TYPE_NUMBER,
    INPUT_TYPE_EMAIL,
    INPUT_TYPE_IP,
    INPUT_TYPE_JSON,
    INPUT_TYPE_ALPHANUM,
    INPUT_TYPE_PATH
};

// Rate limiting
struct RateLimitEntry {
    uint32_t count;
    uint32_t windowStart;
    uint32_t lastRequest;
    bool blocked;
};

class SecurityManager {
private:
    Ticker watchdogTicker;
    Ticker backupTicker;
    uint32_t lastWatchdogFeed;
    bool watchdogEnabled;
    std::map<String, RateLimitEntry> rateLimitMap;
    
    // Estadísticas
    uint32_t totalRequests;
    uint32_t blockedRequests;
    uint32_t validationErrors;
    uint32_t watchdogResets;
    
    // Funciones internas
    static void watchdogCallback();
    static void backupCallback();
    void checkWatchdog();
    void performBackup();
    
public:
    SecurityManager();
    
    // Inicialización
    bool begin();
    
    // Watchdog
    void enableWatchdog();
    void disableWatchdog();
    void feedWatchdog();
    bool isWatchdogEnabled() { return watchdogEnabled; }
    uint32_t getLastFeedTime() { return lastWatchdogFeed; }
    
    // Rate Limiting
    bool checkRateLimit(const String& clientId);
    void resetRateLimit(const String& clientId);
    bool isClientBlocked(const String& clientId);
    uint32_t getClientRequestCount(const String& clientId);
    void clearRateLimits();
    
    // Validación de entradas
    bool validateInput(const String& input, InputType type);
    String sanitizeInput(const String& input);
    bool validateLength(const String& input, size_t maxLength);
    bool validateJSON(const String& json);
    bool validateIP(const String& ip);
    bool validateEmail(const String& email);
    bool validateAlphanumeric(const String& input);
    bool validateNumber(const String& input, int min = INT_MIN, int max = INT_MAX);
    bool validatePath(const String& path);
    
    // Protección XSS y SQL Injection
    String escapeHTML(const String& input);
    String escapeSQL(const String& input);
    bool containsMaliciousPattern(const String& input);
    
    // Backup y recuperación
    bool createBackup();
    bool restoreBackup();
    bool backupExists();
    String getLastBackupTime();
    void scheduleBackup(uint32_t interval);
    
    // Auditoría y logs
    void logSecurityEvent(const String& event, const String& details);
    void logFailedValidation(const String& input, InputType type);
    void logRateLimitViolation(const String& clientId);
    
    // Estadísticas
    String getSecurityStats();
    void resetStats();
    
    // Utilidades
    String generateSecureToken(size_t length = 32);
    String hashData(const String& data);
    bool verifyChecksum(const String& data, const String& checksum);
};

// Instancia global
extern SecurityManager Security;

// Macros de validación
#define VALIDATE_INPUT(input, type) \
    if (!Security.validateInput(input, type)) { \
        Security.logFailedValidation(input, type); \
        return false; \
    }

#define CHECK_RATE_LIMIT(clientId) \
    if (!Security.checkRateLimit(clientId)) { \
        Security.logRateLimitViolation(clientId); \
        request->send(429, "application/json", "{\"error\":\"Too many requests\"}"); \
        return; \
    }

#endif // SECURITY_MANAGER_H