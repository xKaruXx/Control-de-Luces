#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "Logger.h"
#include <vector>
#include <map>

// Configuración de base de datos
#define DB_PATH "/db/"
#define DB_EVENTS_FILE "/db/events.db"
#define DB_SCHEDULE_FILE "/db/schedule.db"
#define DB_ZONES_FILE "/db/zones.db"
#define DB_CONSUMPTION_FILE "/db/consumption.db"
#define MAX_RECORDS 1000
#define DB_ROTATION_SIZE 50000  // 50KB máximo por archivo

// Tipos de eventos
enum EventType {
    EVENT_POWER_ON,
    EVENT_POWER_OFF,
    EVENT_FAILURE,
    EVENT_REPAIR,
    EVENT_SCHEDULE,
    EVENT_MANUAL,
    EVENT_SENSOR
};

// Estructura de evento
struct Event {
    uint32_t id;
    uint32_t timestamp;
    String luminariaId;
    EventType type;
    String description;
    String user;
    float value;  // Para consumo o sensor
};

// Estructura de programación horaria
struct Schedule {
    uint32_t id;
    String name;
    bool enabled;
    uint8_t hourOn;
    uint8_t minuteOn;
    uint8_t hourOff;
    uint8_t minuteOff;
    uint8_t daysOfWeek;  // Bitmask: 0=Dom, 1=Lun, etc
    String zones;  // IDs de zonas separadas por coma
};

// Estructura de zona
struct Zone {
    uint32_t id;
    String name;
    String description;
    std::vector<String> luminarias;  // IDs de luminarias
    float avgConsumption;
    bool active;
};

// Estructura de consumo
struct ConsumptionRecord {
    uint32_t timestamp;
    String luminariaId;
    float power;  // Watts
    float voltage;  // Voltios
    float current;  // Amperios
    float energy;  // kWh acumulado
};

class DatabaseManager {
private:
    uint32_t nextEventId;
    uint32_t nextScheduleId;
    uint32_t nextZoneId;
    
    // Cache en memoria
    std::vector<Event> recentEvents;
    std::vector<Schedule> schedules;
    std::map<uint32_t, Zone> zones;
    std::vector<ConsumptionRecord> consumptionCache;
    
    // Funciones internas
    bool ensureDatabase();
    void rotateDatabase(const String& filename);
    bool saveEventsToFile();
    bool loadEventsFromFile();
    bool saveSchedulesToFile();
    bool loadSchedulesFromFile();
    bool saveZonesToFile();
    bool loadZonesFromFile();
    
public:
    DatabaseManager();
    
    // Inicialización
    bool begin();
    void reset();
    
    // === EVENTOS ===
    uint32_t logEvent(const String& luminariaId, EventType type, const String& description, const String& user = "SYSTEM");
    std::vector<Event> getEvents(uint32_t limit = 50, uint32_t offset = 0);
    std::vector<Event> getEventsByLuminaria(const String& luminariaId, uint32_t limit = 20);
    std::vector<Event> getEventsByType(EventType type, uint32_t limit = 20);
    String getEventsJson(uint32_t limit = 50);
    bool clearOldEvents(uint32_t daysToKeep = 30);
    
    // === PROGRAMACIÓN HORARIA ===
    uint32_t addSchedule(const String& name, uint8_t hourOn, uint8_t minuteOn, 
                         uint8_t hourOff, uint8_t minuteOff, uint8_t daysOfWeek);
    bool updateSchedule(uint32_t id, const Schedule& schedule);
    bool deleteSchedule(uint32_t id);
    bool enableSchedule(uint32_t id, bool enabled);
    Schedule getSchedule(uint32_t id);
    std::vector<Schedule> getAllSchedules();
    std::vector<Schedule> getActiveSchedules();
    bool shouldExecuteSchedule(const Schedule& schedule, uint8_t currentHour, uint8_t currentMinute, uint8_t currentDay);
    String getSchedulesJson();
    
    // === ZONAS ===
    uint32_t createZone(const String& name, const String& description);
    bool updateZone(uint32_t id, const Zone& zone);
    bool deleteZone(uint32_t id);
    bool addLuminariaToZone(uint32_t zoneId, const String& luminariaId);
    bool removeLuminariaFromZone(uint32_t zoneId, const String& luminariaId);
    Zone getZone(uint32_t id);
    std::vector<Zone> getAllZones();
    std::vector<String> getLuminariasInZone(uint32_t zoneId);
    uint32_t getZoneByLuminaria(const String& luminariaId);
    String getZonesJson();
    
    // === CONSUMO ENERGÉTICO ===
    void logConsumption(const String& luminariaId, float power, float voltage, float current);
    float getTotalConsumption();
    float getConsumptionByLuminaria(const String& luminariaId, uint32_t hours = 24);
    float getConsumptionByZone(uint32_t zoneId, uint32_t hours = 24);
    std::vector<ConsumptionRecord> getConsumptionHistory(const String& luminariaId, uint32_t limit = 100);
    String getConsumptionStats();
    String getConsumptionJson(uint32_t hours = 24);
    
    // === EXPORTACIÓN ===
    String exportToCSV(const String& dataType);  // events, schedules, zones, consumption
    String exportToJSON(const String& dataType);
    bool importFromJSON(const String& dataType, const String& jsonData);
    
    // === ESTADÍSTICAS ===
    String getDatabaseStats();
    uint32_t getEventCount();
    uint32_t getScheduleCount();
    uint32_t getZoneCount();
    size_t getDatabaseSize();
    
    // === MANTENIMIENTO ===
    bool compactDatabase();
    bool backupDatabase();
    bool restoreDatabase();
    void performMaintenance();
};

// Instancia global
extern DatabaseManager Database;

#endif // DATABASE_MANAGER_H