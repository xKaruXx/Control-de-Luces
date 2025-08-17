#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <CircularBuffer.h>
#include <ArduinoJson.h>
#include "config.h"

#define LOG_FILE "/logs/system.log"
#define LOG_BACKUP_FILE "/logs/system.old"
#define MAX_LOG_SIZE 10240  // 10KB máximo por archivo
#define MAX_LOG_ENTRIES 100 // Máximo de entradas en memoria

enum LogLevel {
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4
};

struct LogEntry {
    unsigned long timestamp;
    String level;
    String message;
    String module;
};

class Logger {
private:
    CircularBuffer<LogEntry, MAX_LOG_ENTRIES> buffer;
    File logFile;
    bool sdAvailable = false;
    unsigned long lastFlush = 0;
    const unsigned long FLUSH_INTERVAL = 30000; // Flush cada 30 segundos
    
    String formatLogEntry(const LogEntry& entry);
    void rotateLogFile();
    bool ensureLogDirectory();
    
public:
    Logger();
    ~Logger();
    
    bool begin();
    void log(LogLevel level, const String& message, const String& module = "SYSTEM");
    void error(const String& message, const String& module = "SYSTEM");
    void warning(const String& message, const String& module = "SYSTEM");
    void info(const String& message, const String& module = "SYSTEM");
    void debug(const String& message, const String& module = "SYSTEM");
    
    void flush();  // Guardar logs pendientes a archivo
    String getRecentLogs(int count = 50);  // Obtener logs recientes en JSON
    void clearLogs();  // Limpiar todos los logs
    size_t getLogFileSize();
    
    // Métodos para análisis
    int getErrorCount();
    int getWarningCount();
    String getLogStats();  // Estadísticas en JSON
};

// Instancia global del logger
extern Logger SystemLogger;

// Macros mejoradas para logging
#define SLOG_ERROR(msg) SystemLogger.error(msg, __func__)
#define SLOG_WARNING(msg) SystemLogger.warning(msg, __func__)
#define SLOG_INFO(msg) SystemLogger.info(msg, __func__)
#define SLOG_DEBUG(msg) SystemLogger.debug(msg, __func__)

#endif // LOGGER_H