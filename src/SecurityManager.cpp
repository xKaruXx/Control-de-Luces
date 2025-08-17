#include "SecurityManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <SHA256.h>

SecurityManager Security;
static SecurityManager* securityInstance = nullptr;

SecurityManager::SecurityManager() {
    securityInstance = this;
    watchdogEnabled = false;
    lastWatchdogFeed = 0;
    totalRequests = 0;
    blockedRequests = 0;
    validationErrors = 0;
    watchdogResets = 0;
}

bool SecurityManager::begin() {
    SystemLogger.info("Iniciando SecurityManager", "SECURITY");
    
    // Crear directorio de backups si no existe
    if (!LittleFS.exists("/backup")) {
        LittleFS.mkdir("/backup");
    }
    
    // Iniciar watchdog si está configurado
    if (WATCHDOG_TIMEOUT > 0) {
        enableWatchdog();
    }
    
    // Programar backup automático
    scheduleBackup(BACKUP_INTERVAL);
    
    SystemLogger.info("SecurityManager iniciado", "SECURITY");
    return true;
}

// === WATCHDOG ===

void SecurityManager::watchdogCallback() {
    if (securityInstance) {
        securityInstance->checkWatchdog();
    }
}

void SecurityManager::checkWatchdog() {
    uint32_t now = millis();
    
    if (watchdogEnabled && (now - lastWatchdogFeed > WATCHDOG_TIMEOUT)) {
        watchdogResets++;
        SystemLogger.error("Watchdog timeout! Sistema bloqueado por " + 
                          String(now - lastWatchdogFeed) + "ms", "SECURITY");
        
        // Intentar recuperación suave primero
        ESP.wdtFeed();
        yield();
        
        // Si sigue sin responder, reiniciar
        if (millis() - lastWatchdogFeed > WATCHDOG_TIMEOUT * 2) {
            SystemLogger.error("Reiniciando sistema por watchdog...", "SECURITY");
            ESP.restart();
        }
    }
}

void SecurityManager::enableWatchdog() {
    if (!watchdogEnabled) {
        watchdogEnabled = true;
        lastWatchdogFeed = millis();
        watchdogTicker.attach_ms(WATCHDOG_CHECK_INTERVAL, watchdogCallback);
        SystemLogger.info("Watchdog habilitado con timeout de " + 
                         String(WATCHDOG_TIMEOUT) + "ms", "SECURITY");
    }
}

void SecurityManager::disableWatchdog() {
    if (watchdogEnabled) {
        watchdogEnabled = false;
        watchdogTicker.detach();
        SystemLogger.info("Watchdog deshabilitado", "SECURITY");
    }
}

void SecurityManager::feedWatchdog() {
    lastWatchdogFeed = millis();
    ESP.wdtFeed();  // También alimentar el watchdog del hardware
}

// === RATE LIMITING ===

bool SecurityManager::checkRateLimit(const String& clientId) {
    totalRequests++;
    uint32_t now = millis();
    
    // Obtener o crear entrada para el cliente
    if (!rateLimitMap.count(clientId)) {
        RateLimitEntry entry;
        entry.count = 1;
        entry.windowStart = now;
        entry.lastRequest = now;
        entry.blocked = false;
        rateLimitMap[clientId] = entry;
        return true;
    }
    
    RateLimitEntry& entry = rateLimitMap[clientId];
    
    // Verificar si está bloqueado
    if (entry.blocked && (now - entry.windowStart < RATE_LIMIT_WINDOW * 2)) {
        blockedRequests++;
        return false;
    }
    
    // Reiniciar ventana si ha pasado el tiempo
    if (now - entry.windowStart >= RATE_LIMIT_WINDOW) {
        entry.count = 1;
        entry.windowStart = now;
        entry.blocked = false;
    } else {
        entry.count++;
        
        // Verificar límite
        if (entry.count > RATE_LIMIT_MAX_REQUESTS) {
            entry.blocked = true;
            blockedRequests++;
            SystemLogger.warning("Rate limit excedido para: " + clientId, "SECURITY");
            return false;
        }
    }
    
    entry.lastRequest = now;
    return true;
}

void SecurityManager::resetRateLimit(const String& clientId) {
    if (rateLimitMap.count(clientId)) {
        rateLimitMap.erase(clientId);
    }
}

bool SecurityManager::isClientBlocked(const String& clientId) {
    if (!rateLimitMap.count(clientId)) return false;
    return rateLimitMap[clientId].blocked;
}

uint32_t SecurityManager::getClientRequestCount(const String& clientId) {
    if (!rateLimitMap.count(clientId)) return 0;
    return rateLimitMap[clientId].count;
}

void SecurityManager::clearRateLimits() {
    rateLimitMap.clear();
    SystemLogger.info("Rate limits limpiados", "SECURITY");
}

// === VALIDACIÓN DE ENTRADAS ===

bool SecurityManager::validateInput(const String& input, InputType type) {
    // Verificar longitud máxima
    if (input.length() > MAX_INPUT_LENGTH) {
        validationErrors++;
        return false;
    }
    
    // Verificar patrones maliciosos
    if (containsMaliciousPattern(input)) {
        validationErrors++;
        logSecurityEvent("MALICIOUS_PATTERN", "Input: " + input.substring(0, 50));
        return false;
    }
    
    // Validar según tipo
    switch (type) {
        case INPUT_TYPE_TEXT:
            return true;  // Ya validado arriba
            
        case INPUT_TYPE_NUMBER:
            return validateNumber(input);
            
        case INPUT_TYPE_EMAIL:
            return validateEmail(input);
            
        case INPUT_TYPE_IP:
            return validateIP(input);
            
        case INPUT_TYPE_JSON:
            return validateJSON(input);
            
        case INPUT_TYPE_ALPHANUM:
            return validateAlphanumeric(input);
            
        case INPUT_TYPE_PATH:
            return validatePath(input);
            
        default:
            return false;
    }
}

String SecurityManager::sanitizeInput(const String& input) {
    String sanitized = input;
    
    // Remover caracteres de control
    for (int i = sanitized.length() - 1; i >= 0; i--) {
        char c = sanitized[i];
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
            sanitized.remove(i, 1);
        }
    }
    
    // Escapar HTML
    sanitized = escapeHTML(sanitized);
    
    return sanitized;
}

bool SecurityManager::validateLength(const String& input, size_t maxLength) {
    return input.length() <= maxLength;
}

bool SecurityManager::validateJSON(const String& json) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json);
    return !error;
}

bool SecurityManager::validateIP(const String& ip) {
    IPAddress addr;
    return addr.fromString(ip);
}

bool SecurityManager::validateEmail(const String& email) {
    // Validación básica de email
    int atPos = email.indexOf('@');
    int dotPos = email.lastIndexOf('.');
    
    return (atPos > 0 && dotPos > atPos + 1 && dotPos < email.length() - 1);
}

bool SecurityManager::validateAlphanumeric(const String& input) {
    for (size_t i = 0; i < input.length(); i++) {
        char c = input[i];
        if (!isalnum(c) && c != '_' && c != '-') {
            return false;
        }
    }
    return true;
}

bool SecurityManager::validateNumber(const String& input, int min, int max) {
    char* endptr;
    long value = strtol(input.c_str(), &endptr, 10);
    
    if (*endptr != '\0') return false;  // No es un número válido
    if (value < min || value > max) return false;  // Fuera de rango
    
    return true;
}

bool SecurityManager::validatePath(const String& path) {
    // No permitir path traversal
    if (path.indexOf("..") >= 0) return false;
    if (path.indexOf("//") >= 0) return false;
    
    // Solo permitir caracteres seguros en paths
    for (size_t i = 0; i < path.length(); i++) {
        char c = path[i];
        if (!isalnum(c) && c != '/' && c != '.' && c != '_' && c != '-') {
            return false;
        }
    }
    
    return true;
}

// === PROTECCIÓN XSS/SQL ===

String SecurityManager::escapeHTML(const String& input) {
    String escaped = input;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&#x27;");
    escaped.replace("/", "&#x2F;");
    return escaped;
}

String SecurityManager::escapeSQL(const String& input) {
    String escaped = input;
    escaped.replace("'", "''");
    escaped.replace("\\", "\\\\");
    escaped.replace("\n", "\\n");
    escaped.replace("\r", "\\r");
    escaped.replace("\x00", "");
    escaped.replace("\x1a", "");
    return escaped;
}

bool SecurityManager::containsMaliciousPattern(const String& input) {
    // Patrones comunes de ataques
    const char* patterns[] = {
        "<script", "</script>", "javascript:",
        "onclick=", "onerror=", "onload=",
        "'; DROP TABLE", "1=1", "OR 1=1",
        "../", "..\\", "%2e%2e",
        "\x00", "%00",
        nullptr
    };
    
    String lowerInput = input;
    lowerInput.toLowerCase();
    
    for (int i = 0; patterns[i] != nullptr; i++) {
        if (lowerInput.indexOf(patterns[i]) >= 0) {
            return true;
        }
    }
    
    return false;
}

// === BACKUP ===

void SecurityManager::backupCallback() {
    if (securityInstance) {
        securityInstance->performBackup();
    }
}

void SecurityManager::performBackup() {
    if (createBackup()) {
        SystemLogger.info("Backup automático completado", "SECURITY");
    } else {
        SystemLogger.error("Fallo en backup automático", "SECURITY");
    }
}

bool SecurityManager::createBackup() {
    StaticJsonDocument<4096> doc;
    
    // Incluir configuración importante
    doc["version"] = FIRMWARE_VERSION;
    doc["timestamp"] = millis();
    doc["wifi_ssid"] = WiFi.SSID();
    doc["hostname"] = WiFi.hostname();
    
    // Guardar estadísticas
    JsonObject stats = doc.createNestedObject("stats");
    stats["total_requests"] = totalRequests;
    stats["blocked_requests"] = blockedRequests;
    stats["validation_errors"] = validationErrors;
    stats["watchdog_resets"] = watchdogResets;
    
    // Guardar en archivo
    File file = LittleFS.open("/backup/config.bak", "w");
    if (!file) {
        SystemLogger.error("No se pudo crear archivo de backup", "SECURITY");
        return false;
    }
    
    serializeJson(doc, file);
    file.close();
    
    SystemLogger.info("Backup creado exitosamente", "SECURITY");
    return true;
}

bool SecurityManager::restoreBackup() {
    if (!backupExists()) {
        SystemLogger.error("No existe archivo de backup", "SECURITY");
        return false;
    }
    
    File file = LittleFS.open("/backup/config.bak", "r");
    if (!file) {
        SystemLogger.error("No se pudo abrir archivo de backup", "SECURITY");
        return false;
    }
    
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        SystemLogger.error("Error al parsear backup: " + String(error.c_str()), "SECURITY");
        return false;
    }
    
    // Restaurar estadísticas
    totalRequests = doc["stats"]["total_requests"] | 0;
    blockedRequests = doc["stats"]["blocked_requests"] | 0;
    validationErrors = doc["stats"]["validation_errors"] | 0;
    watchdogResets = doc["stats"]["watchdog_resets"] | 0;
    
    SystemLogger.info("Backup restaurado exitosamente", "SECURITY");
    return true;
}

bool SecurityManager::backupExists() {
    return LittleFS.exists("/backup/config.bak");
}

String SecurityManager::getLastBackupTime() {
    if (!backupExists()) return "Nunca";
    
    File file = LittleFS.open("/backup/config.bak", "r");
    if (!file) return "Error";
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) return "Error";
    
    uint32_t timestamp = doc["timestamp"] | 0;
    uint32_t elapsed = (millis() - timestamp) / 1000;
    
    if (elapsed < 60) return String(elapsed) + " segundos";
    if (elapsed < 3600) return String(elapsed / 60) + " minutos";
    return String(elapsed / 3600) + " horas";
}

void SecurityManager::scheduleBackup(uint32_t interval) {
    backupTicker.attach_ms(interval, backupCallback);
    SystemLogger.info("Backup programado cada " + String(interval / 60000) + " minutos", "SECURITY");
}

// === AUDITORÍA ===

void SecurityManager::logSecurityEvent(const String& event, const String& details) {
    SystemLogger.warning("[SECURITY EVENT] " + event + ": " + details, "SECURITY");
}

void SecurityManager::logFailedValidation(const String& input, InputType type) {
    String typeStr = "";
    switch (type) {
        case INPUT_TYPE_TEXT: typeStr = "TEXT"; break;
        case INPUT_TYPE_NUMBER: typeStr = "NUMBER"; break;
        case INPUT_TYPE_EMAIL: typeStr = "EMAIL"; break;
        case INPUT_TYPE_IP: typeStr = "IP"; break;
        case INPUT_TYPE_JSON: typeStr = "JSON"; break;
        case INPUT_TYPE_ALPHANUM: typeStr = "ALPHANUM"; break;
        case INPUT_TYPE_PATH: typeStr = "PATH"; break;
    }
    
    logSecurityEvent("VALIDATION_FAILED", "Type: " + typeStr + ", Input: " + input.substring(0, 50));
}

void SecurityManager::logRateLimitViolation(const String& clientId) {
    logSecurityEvent("RATE_LIMIT_EXCEEDED", "Client: " + clientId);
}

// === ESTADÍSTICAS ===

String SecurityManager::getSecurityStats() {
    StaticJsonDocument<512> doc;
    
    doc["total_requests"] = totalRequests;
    doc["blocked_requests"] = blockedRequests;
    doc["block_rate"] = totalRequests > 0 ? (blockedRequests * 100.0 / totalRequests) : 0;
    doc["validation_errors"] = validationErrors;
    doc["watchdog_resets"] = watchdogResets;
    doc["watchdog_enabled"] = watchdogEnabled;
    doc["rate_limit_clients"] = rateLimitMap.size();
    doc["backup_exists"] = backupExists();
    doc["last_backup"] = getLastBackupTime();
    
    String result;
    serializeJson(doc, result);
    return result;
}

void SecurityManager::resetStats() {
    totalRequests = 0;
    blockedRequests = 0;
    validationErrors = 0;
    watchdogResets = 0;
    SystemLogger.info("Estadísticas de seguridad reiniciadas", "SECURITY");
}

// === UTILIDADES ===

String SecurityManager::generateSecureToken(size_t length) {
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*";
    String token = "";
    
    for (size_t i = 0; i < length; i++) {
        token += chars[random(0, strlen(chars))];
    }
    
    return token;
}

String SecurityManager::hashData(const String& data) {
    SHA256 sha;
    uint8_t hash[32];
    
    sha.reset();
    sha.update((const uint8_t*)data.c_str(), data.length());
    sha.finalize(hash, sizeof(hash));
    
    String hashStr = "";
    for (int i = 0; i < 32; i++) {
        if (hash[i] < 16) hashStr += "0";
        hashStr += String(hash[i], HEX);
    }
    
    return hashStr;
}

bool SecurityManager::verifyChecksum(const String& data, const String& checksum) {
    return hashData(data) == checksum;
}