#include "DatabaseManager.h"

DatabaseManager Database;

DatabaseManager::DatabaseManager() {
    nextEventId = 1;
    nextScheduleId = 1;
    nextZoneId = 1;
}

bool DatabaseManager::begin() {
    SystemLogger.info("Iniciando DatabaseManager", "DB");
    
    if (!ensureDatabase()) {
        SystemLogger.error("Error al crear estructura de base de datos", "DB");
        return false;
    }
    
    // Cargar datos existentes
    loadEventsFromFile();
    loadSchedulesFromFile();
    loadZonesFromFile();
    
    SystemLogger.info("Base de datos iniciada - Eventos: " + String(recentEvents.size()) + 
                     ", Schedules: " + String(schedules.size()) + 
                     ", Zonas: " + String(zones.size()), "DB");
    return true;
}

bool DatabaseManager::ensureDatabase() {
    if (!LittleFS.exists(DB_PATH)) {
        if (!LittleFS.mkdir(DB_PATH)) {
            return false;
        }
    }
    return true;
}

void DatabaseManager::rotateDatabase(const String& filename) {
    if (!LittleFS.exists(filename)) return;
    
    File f = LittleFS.open(filename, "r");
    if (f && f.size() > DB_ROTATION_SIZE) {
        f.close();
        String backup = filename + ".old";
        if (LittleFS.exists(backup)) {
            LittleFS.remove(backup);
        }
        LittleFS.rename(filename, backup);
        SystemLogger.info("Base de datos rotada: " + filename, "DB");
    } else if (f) {
        f.close();
    }
}

// === EVENTOS ===

uint32_t DatabaseManager::logEvent(const String& luminariaId, EventType type, 
                                   const String& description, const String& user) {
    Event event;
    event.id = nextEventId++;
    event.timestamp = millis() / 1000;
    event.luminariaId = luminariaId;
    event.type = type;
    event.description = description;
    event.user = user;
    event.value = 0;
    
    recentEvents.push_back(event);
    
    // Limitar tamaño del cache
    if (recentEvents.size() > MAX_RECORDS) {
        recentEvents.erase(recentEvents.begin());
    }
    
    // Guardar periódicamente
    if (recentEvents.size() % 10 == 0) {
        saveEventsToFile();
    }
    
    SystemLogger.debug("Evento registrado: " + description, "DB");
    return event.id;
}

std::vector<Event> DatabaseManager::getEvents(uint32_t limit, uint32_t offset) {
    std::vector<Event> result;
    
    if (offset >= recentEvents.size()) return result;
    
    uint32_t start = recentEvents.size() > offset ? recentEvents.size() - offset - 1 : 0;
    uint32_t count = 0;
    
    for (int i = start; i >= 0 && count < limit; i--) {
        result.push_back(recentEvents[i]);
        count++;
    }
    
    return result;
}

std::vector<Event> DatabaseManager::getEventsByLuminaria(const String& luminariaId, uint32_t limit) {
    std::vector<Event> result;
    
    for (int i = recentEvents.size() - 1; i >= 0 && result.size() < limit; i--) {
        if (recentEvents[i].luminariaId == luminariaId) {
            result.push_back(recentEvents[i]);
        }
    }
    
    return result;
}

String DatabaseManager::getEventsJson(uint32_t limit) {
    StaticJsonDocument<4096> doc;
    JsonArray arr = doc.to<JsonArray>();
    
    auto events = getEvents(limit);
    for (const auto& event : events) {
        JsonObject obj = arr.createNestedObject();
        obj["id"] = event.id;
        obj["timestamp"] = event.timestamp;
        obj["luminaria"] = event.luminariaId;
        obj["type"] = event.type;
        obj["description"] = event.description;
        obj["user"] = event.user;
    }
    
    String result;
    serializeJson(doc, result);
    return result;
}

bool DatabaseManager::saveEventsToFile() {
    rotateDatabase(DB_EVENTS_FILE);
    
    File file = LittleFS.open(DB_EVENTS_FILE, "w");
    if (!file) return false;
    
    StaticJsonDocument<8192> doc;
    JsonArray arr = doc.to<JsonArray>();
    
    // Guardar últimos 100 eventos
    uint32_t start = recentEvents.size() > 100 ? recentEvents.size() - 100 : 0;
    for (uint32_t i = start; i < recentEvents.size(); i++) {
        JsonObject obj = arr.createNestedObject();
        obj["id"] = recentEvents[i].id;
        obj["ts"] = recentEvents[i].timestamp;
        obj["lum"] = recentEvents[i].luminariaId;
        obj["type"] = recentEvents[i].type;
        obj["desc"] = recentEvents[i].description;
        obj["user"] = recentEvents[i].user;
    }
    
    serializeJson(doc, file);
    file.close();
    return true;
}

bool DatabaseManager::loadEventsFromFile() {
    if (!LittleFS.exists(DB_EVENTS_FILE)) return false;
    
    File file = LittleFS.open(DB_EVENTS_FILE, "r");
    if (!file) return false;
    
    StaticJsonDocument<8192> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) return false;
    
    recentEvents.clear();
    JsonArray arr = doc.as<JsonArray>();
    
    for (JsonObject obj : arr) {
        Event event;
        event.id = obj["id"];
        event.timestamp = obj["ts"];
        event.luminariaId = obj["lum"].as<String>();
        event.type = (EventType)obj["type"].as<int>();
        event.description = obj["desc"].as<String>();
        event.user = obj["user"].as<String>();
        recentEvents.push_back(event);
        
        if (event.id >= nextEventId) {
            nextEventId = event.id + 1;
        }
    }
    
    return true;
}

// === PROGRAMACIÓN HORARIA ===

uint32_t DatabaseManager::addSchedule(const String& name, uint8_t hourOn, uint8_t minuteOn,
                                      uint8_t hourOff, uint8_t minuteOff, uint8_t daysOfWeek) {
    Schedule schedule;
    schedule.id = nextScheduleId++;
    schedule.name = name;
    schedule.enabled = true;
    schedule.hourOn = hourOn;
    schedule.minuteOn = minuteOn;
    schedule.hourOff = hourOff;
    schedule.minuteOff = minuteOff;
    schedule.daysOfWeek = daysOfWeek;
    schedule.zones = "";
    
    schedules.push_back(schedule);
    saveSchedulesToFile();
    
    SystemLogger.info("Programación creada: " + name, "DB");
    return schedule.id;
}

bool DatabaseManager::updateSchedule(uint32_t id, const Schedule& schedule) {
    for (auto& s : schedules) {
        if (s.id == id) {
            s = schedule;
            saveSchedulesToFile();
            return true;
        }
    }
    return false;
}

bool DatabaseManager::deleteSchedule(uint32_t id) {
    for (auto it = schedules.begin(); it != schedules.end(); ++it) {
        if (it->id == id) {
            schedules.erase(it);
            saveSchedulesToFile();
            return true;
        }
    }
    return false;
}

bool DatabaseManager::enableSchedule(uint32_t id, bool enabled) {
    for (auto& s : schedules) {
        if (s.id == id) {
            s.enabled = enabled;
            saveSchedulesToFile();
            return true;
        }
    }
    return false;
}

std::vector<Schedule> DatabaseManager::getActiveSchedules() {
    std::vector<Schedule> active;
    for (const auto& s : schedules) {
        if (s.enabled) {
            active.push_back(s);
        }
    }
    return active;
}

bool DatabaseManager::shouldExecuteSchedule(const Schedule& schedule, uint8_t currentHour, 
                                           uint8_t currentMinute, uint8_t currentDay) {
    if (!schedule.enabled) return false;
    
    // Verificar día de la semana
    if (!(schedule.daysOfWeek & (1 << currentDay))) return false;
    
    // Verificar hora
    uint16_t currentTime = currentHour * 60 + currentMinute;
    uint16_t onTime = schedule.hourOn * 60 + schedule.minuteOn;
    uint16_t offTime = schedule.hourOff * 60 + schedule.minuteOff;
    
    if (onTime < offTime) {
        // Horario normal (ej: 6:00 - 18:00)
        return currentTime >= onTime && currentTime < offTime;
    } else {
        // Horario nocturno (ej: 18:00 - 6:00)
        return currentTime >= onTime || currentTime < offTime;
    }
}

String DatabaseManager::getSchedulesJson() {
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.to<JsonArray>();
    
    for (const auto& s : schedules) {
        JsonObject obj = arr.createNestedObject();
        obj["id"] = s.id;
        obj["name"] = s.name;
        obj["enabled"] = s.enabled;
        obj["hourOn"] = s.hourOn;
        obj["minuteOn"] = s.minuteOn;
        obj["hourOff"] = s.hourOff;
        obj["minuteOff"] = s.minuteOff;
        obj["daysOfWeek"] = s.daysOfWeek;
        obj["zones"] = s.zones;
    }
    
    String result;
    serializeJson(doc, result);
    return result;
}

bool DatabaseManager::saveSchedulesToFile() {
    File file = LittleFS.open(DB_SCHEDULE_FILE, "w");
    if (!file) return false;
    
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.to<JsonArray>();
    
    for (const auto& s : schedules) {
        JsonObject obj = arr.createNestedObject();
        obj["id"] = s.id;
        obj["name"] = s.name;
        obj["enabled"] = s.enabled;
        obj["on_h"] = s.hourOn;
        obj["on_m"] = s.minuteOn;
        obj["off_h"] = s.hourOff;
        obj["off_m"] = s.minuteOff;
        obj["days"] = s.daysOfWeek;
        obj["zones"] = s.zones;
    }
    
    serializeJson(doc, file);
    file.close();
    return true;
}

bool DatabaseManager::loadSchedulesFromFile() {
    if (!LittleFS.exists(DB_SCHEDULE_FILE)) return false;
    
    File file = LittleFS.open(DB_SCHEDULE_FILE, "r");
    if (!file) return false;
    
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) return false;
    
    schedules.clear();
    JsonArray arr = doc.as<JsonArray>();
    
    for (JsonObject obj : arr) {
        Schedule s;
        s.id = obj["id"];
        s.name = obj["name"].as<String>();
        s.enabled = obj["enabled"];
        s.hourOn = obj["on_h"];
        s.minuteOn = obj["on_m"];
        s.hourOff = obj["off_h"];
        s.minuteOff = obj["off_m"];
        s.daysOfWeek = obj["days"];
        s.zones = obj["zones"].as<String>();
        schedules.push_back(s);
        
        if (s.id >= nextScheduleId) {
            nextScheduleId = s.id + 1;
        }
    }
    
    return true;
}

// === ZONAS ===

uint32_t DatabaseManager::createZone(const String& name, const String& description) {
    Zone zone;
    zone.id = nextZoneId++;
    zone.name = name;
    zone.description = description;
    zone.avgConsumption = 0;
    zone.active = true;
    
    zones[zone.id] = zone;
    saveZonesToFile();
    
    SystemLogger.info("Zona creada: " + name, "DB");
    return zone.id;
}

bool DatabaseManager::addLuminariaToZone(uint32_t zoneId, const String& luminariaId) {
    if (zones.find(zoneId) == zones.end()) return false;
    
    zones[zoneId].luminarias.push_back(luminariaId);
    saveZonesToFile();
    return true;
}

bool DatabaseManager::removeLuminariaFromZone(uint32_t zoneId, const String& luminariaId) {
    if (zones.find(zoneId) == zones.end()) return false;
    
    auto& lums = zones[zoneId].luminarias;
    lums.erase(std::remove(lums.begin(), lums.end(), luminariaId), lums.end());
    saveZonesToFile();
    return true;
}

std::vector<String> DatabaseManager::getLuminariasInZone(uint32_t zoneId) {
    if (zones.find(zoneId) == zones.end()) return {};
    return zones[zoneId].luminarias;
}

String DatabaseManager::getZonesJson() {
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.to<JsonArray>();
    
    for (const auto& pair : zones) {
        const Zone& z = pair.second;
        JsonObject obj = arr.createNestedObject();
        obj["id"] = z.id;
        obj["name"] = z.name;
        obj["description"] = z.description;
        obj["luminarias"] = z.luminarias.size();
        obj["consumption"] = z.avgConsumption;
        obj["active"] = z.active;
    }
    
    String result;
    serializeJson(doc, result);
    return result;
}

bool DatabaseManager::saveZonesToFile() {
    File file = LittleFS.open(DB_ZONES_FILE, "w");
    if (!file) return false;
    
    StaticJsonDocument<4096> doc;
    JsonArray arr = doc.to<JsonArray>();
    
    for (const auto& pair : zones) {
        const Zone& z = pair.second;
        JsonObject obj = arr.createNestedObject();
        obj["id"] = z.id;
        obj["name"] = z.name;
        obj["desc"] = z.description;
        obj["active"] = z.active;
        
        JsonArray lumsArr = obj.createNestedArray("lums");
        for (const auto& lum : z.luminarias) {
            lumsArr.add(lum);
        }
    }
    
    serializeJson(doc, file);
    file.close();
    return true;
}

bool DatabaseManager::loadZonesFromFile() {
    if (!LittleFS.exists(DB_ZONES_FILE)) return false;
    
    File file = LittleFS.open(DB_ZONES_FILE, "r");
    if (!file) return false;
    
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) return false;
    
    zones.clear();
    JsonArray arr = doc.as<JsonArray>();
    
    for (JsonObject obj : arr) {
        Zone z;
        z.id = obj["id"];
        z.name = obj["name"].as<String>();
        z.description = obj["desc"].as<String>();
        z.active = obj["active"];
        z.avgConsumption = 0;
        
        JsonArray lumsArr = obj["lums"];
        for (JsonVariant v : lumsArr) {
            z.luminarias.push_back(v.as<String>());
        }
        
        zones[z.id] = z;
        
        if (z.id >= nextZoneId) {
            nextZoneId = z.id + 1;
        }
    }
    
    return true;
}

// === CONSUMO ===

void DatabaseManager::logConsumption(const String& luminariaId, float power, float voltage, float current) {
    ConsumptionRecord record;
    record.timestamp = millis() / 1000;
    record.luminariaId = luminariaId;
    record.power = power;
    record.voltage = voltage;
    record.current = current;
    record.energy = (power * 1.0) / 1000.0;  // kWh por hora
    
    consumptionCache.push_back(record);
    
    // Limitar cache
    if (consumptionCache.size() > 1000) {
        consumptionCache.erase(consumptionCache.begin());
    }
}

float DatabaseManager::getTotalConsumption() {
    float total = 0;
    for (const auto& record : consumptionCache) {
        total += record.power;
    }
    return total;
}

float DatabaseManager::getConsumptionByLuminaria(const String& luminariaId, uint32_t hours) {
    float total = 0;
    uint32_t cutoff = (millis() / 1000) - (hours * 3600);
    
    for (const auto& record : consumptionCache) {
        if (record.luminariaId == luminariaId && record.timestamp > cutoff) {
            total += record.energy;
        }
    }
    return total;
}

String DatabaseManager::getConsumptionStats() {
    StaticJsonDocument<512> doc;
    
    float totalPower = getTotalConsumption();
    doc["total_power"] = totalPower;
    doc["total_energy_24h"] = totalPower * 24 / 1000;  // kWh
    doc["records"] = consumptionCache.size();
    doc["avg_voltage"] = 220;  // Valor ejemplo
    
    String result;
    serializeJson(doc, result);
    return result;
}

// === EXPORTACIÓN ===

String DatabaseManager::exportToCSV(const String& dataType) {
    String csv = "";
    
    if (dataType == "events") {
        csv = "ID,Timestamp,Luminaria,Type,Description,User\n";
        for (const auto& e : recentEvents) {
            csv += String(e.id) + "," + String(e.timestamp) + "," + e.luminariaId + "," +
                   String(e.type) + "," + e.description + "," + e.user + "\n";
        }
    } else if (dataType == "schedules") {
        csv = "ID,Name,Enabled,OnTime,OffTime,Days\n";
        for (const auto& s : schedules) {
            csv += String(s.id) + "," + s.name + "," + String(s.enabled) + "," +
                   String(s.hourOn) + ":" + String(s.minuteOn) + "," +
                   String(s.hourOff) + ":" + String(s.minuteOff) + "," +
                   String(s.daysOfWeek) + "\n";
        }
    } else if (dataType == "zones") {
        csv = "ID,Name,Description,Luminarias,Active\n";
        for (const auto& pair : zones) {
            const Zone& z = pair.second;
            csv += String(z.id) + "," + z.name + "," + z.description + "," +
                   String(z.luminarias.size()) + "," + String(z.active) + "\n";
        }
    }
    
    return csv;
}

// === ESTADÍSTICAS ===

String DatabaseManager::getDatabaseStats() {
    StaticJsonDocument<512> doc;
    
    doc["events"] = recentEvents.size();
    doc["schedules"] = schedules.size();
    doc["zones"] = zones.size();
    doc["consumption_records"] = consumptionCache.size();
    doc["db_size"] = getDatabaseSize();
    doc["next_event_id"] = nextEventId;
    doc["next_schedule_id"] = nextScheduleId;
    doc["next_zone_id"] = nextZoneId;
    
    String result;
    serializeJson(doc, result);
    return result;
}

size_t DatabaseManager::getDatabaseSize() {
    size_t total = 0;
    
    if (LittleFS.exists(DB_EVENTS_FILE)) {
        File f = LittleFS.open(DB_EVENTS_FILE, "r");
        if (f) {
            total += f.size();
            f.close();
        }
    }
    
    if (LittleFS.exists(DB_SCHEDULE_FILE)) {
        File f = LittleFS.open(DB_SCHEDULE_FILE, "r");
        if (f) {
            total += f.size();
            f.close();
        }
    }
    
    if (LittleFS.exists(DB_ZONES_FILE)) {
        File f = LittleFS.open(DB_ZONES_FILE, "r");
        if (f) {
            total += f.size();
            f.close();
        }
    }
    
    return total;
}

void DatabaseManager::performMaintenance() {
    // Guardar todos los cambios pendientes
    saveEventsToFile();
    saveSchedulesToFile();
    saveZonesToFile();
    
    // Limpiar eventos antiguos
    clearOldEvents(30);
    
    SystemLogger.info("Mantenimiento de base de datos completado", "DB");
}