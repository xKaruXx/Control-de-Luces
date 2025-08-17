#include "Logger.h"

Logger SystemLogger;

Logger::Logger() {}

Logger::~Logger() {
    flush();
    if (logFile) {
        logFile.close();
    }
}

bool Logger::begin() {
    if (!LittleFS.exists("/logs")) {
        LittleFS.mkdir("/logs");
    }
    
    // Verificar tamaño del archivo actual
    if (LittleFS.exists(LOG_FILE)) {
        File f = LittleFS.open(LOG_FILE, "r");
        if (f && f.size() > MAX_LOG_SIZE) {
            f.close();
            rotateLogFile();
        } else if (f) {
            f.close();
        }
    }
    
    info("Sistema de logs iniciado", "LOGGER");
    return true;
}

String Logger::formatLogEntry(const LogEntry& entry) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "[%lu] [%s] [%s] %s\n",
             entry.timestamp,
             entry.level.c_str(),
             entry.module.c_str(),
             entry.message.c_str());
    return String(buffer);
}

void Logger::rotateLogFile() {
    // Eliminar backup anterior si existe
    if (LittleFS.exists(LOG_BACKUP_FILE)) {
        LittleFS.remove(LOG_BACKUP_FILE);
    }
    
    // Renombrar archivo actual como backup
    if (LittleFS.exists(LOG_FILE)) {
        LittleFS.rename(LOG_FILE, LOG_BACKUP_FILE);
    }
    
    info("Archivo de log rotado", "LOGGER");
}

void Logger::log(LogLevel level, const String& message, const String& module) {
    if (level > CURRENT_LOG_LEVEL) return;
    
    LogEntry entry;
    entry.timestamp = millis() / 1000;
    entry.message = message;
    entry.module = module;
    
    switch(level) {
        case LOG_LEVEL_ERROR:
            entry.level = "ERROR";
            Serial.print("[ERROR] ");
            break;
        case LOG_LEVEL_WARNING:
            entry.level = "WARN";
            Serial.print("[WARN] ");
            break;
        case LOG_LEVEL_INFO:
            entry.level = "INFO";
            Serial.print("[INFO] ");
            break;
        case LOG_LEVEL_DEBUG:
            entry.level = "DEBUG";
            Serial.print("[DEBUG] ");
            break;
    }
    
    Serial.print("[" + module + "] ");
    Serial.println(message);
    
    buffer.push(entry);
    
    // Auto-flush si ha pasado suficiente tiempo
    if (millis() - lastFlush > FLUSH_INTERVAL) {
        flush();
    }
}

void Logger::error(const String& message, const String& module) {
    log(LOG_LEVEL_ERROR, message, module);
}

void Logger::warning(const String& message, const String& module) {
    log(LOG_LEVEL_WARNING, message, module);
}

void Logger::info(const String& message, const String& module) {
    log(LOG_LEVEL_INFO, message, module);
}

void Logger::debug(const String& message, const String& module) {
    log(LOG_LEVEL_DEBUG, message, module);
}

void Logger::flush() {
    if (buffer.isEmpty()) return;
    
    // Verificar tamaño antes de escribir
    if (LittleFS.exists(LOG_FILE)) {
        File f = LittleFS.open(LOG_FILE, "r");
        if (f && f.size() > MAX_LOG_SIZE) {
            f.close();
            rotateLogFile();
        } else if (f) {
            f.close();
        }
    }
    
    logFile = LittleFS.open(LOG_FILE, "a");
    if (!logFile) {
        Serial.println("[ERROR] No se pudo abrir archivo de log");
        return;
    }
    
    while (!buffer.isEmpty()) {
        LogEntry entry = buffer.shift();
        logFile.print(formatLogEntry(entry));
    }
    
    logFile.close();
    lastFlush = millis();
}

String Logger::getRecentLogs(int count) {
    StaticJsonDocument<4096> doc;
    JsonArray logs = doc.to<JsonArray>();
    
    int startIdx = buffer.size() > count ? buffer.size() - count : 0;
    
    for (int i = startIdx; i < buffer.size(); i++) {
        JsonObject logObj = logs.createNestedObject();
        logObj["timestamp"] = buffer[i].timestamp;
        logObj["level"] = buffer[i].level;
        logObj["module"] = buffer[i].module;
        logObj["message"] = buffer[i].message;
    }
    
    String result;
    serializeJson(doc, result);
    return result;
}

void Logger::clearLogs() {
    buffer.clear();
    if (LittleFS.exists(LOG_FILE)) {
        LittleFS.remove(LOG_FILE);
    }
    if (LittleFS.exists(LOG_BACKUP_FILE)) {
        LittleFS.remove(LOG_BACKUP_FILE);
    }
    info("Logs limpiados", "LOGGER");
}

size_t Logger::getLogFileSize() {
    if (!LittleFS.exists(LOG_FILE)) return 0;
    
    File f = LittleFS.open(LOG_FILE, "r");
    if (!f) return 0;
    
    size_t size = f.size();
    f.close();
    return size;
}

int Logger::getErrorCount() {
    int count = 0;
    for (int i = 0; i < buffer.size(); i++) {
        if (buffer[i].level == "ERROR") count++;
    }
    return count;
}

int Logger::getWarningCount() {
    int count = 0;
    for (int i = 0; i < buffer.size(); i++) {
        if (buffer[i].level == "WARN") count++;
    }
    return count;
}

String Logger::getLogStats() {
    StaticJsonDocument<512> doc;
    doc["total_entries"] = buffer.size();
    doc["errors"] = getErrorCount();
    doc["warnings"] = getWarningCount();
    doc["file_size"] = getLogFileSize();
    doc["max_file_size"] = MAX_LOG_SIZE;
    doc["buffer_usage"] = (buffer.size() * 100) / MAX_LOG_ENTRIES;
    
    String result;
    serializeJson(doc, result);
    return result;
}