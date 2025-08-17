#include "ScheduleManager.h"

ScheduleManager Scheduler;
TimeManager Time;
static ScheduleManager* schedulerInstance = nullptr;

// === SCHEDULE MANAGER ===

ScheduleManager::ScheduleManager() {
    schedulerInstance = this;
    enabled = false;
    sunriseHour = 6;
    sunriseMinute = 30;
    sunsetHour = 18;
    sunsetMinute = 30;
    
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        lastExecutionTime[i] = 0;
    }
}

bool ScheduleManager::begin() {
    SystemLogger.info("Iniciando ScheduleManager", "SCHEDULE");
    
    // Calcular horarios solares para San Luis
    calculateSunTimes();
    
    // Habilitar scheduler
    enable(true);
    
    SystemLogger.info("Scheduler iniciado - Sunrise: " + getSunriseTime() + 
                     ", Sunset: " + getSunsetTime(), "SCHEDULE");
    return true;
}

void ScheduleManager::setCallback(ScheduleCallback callback) {
    actionCallback = callback;
}

void ScheduleManager::enable(bool state) {
    if (state && !enabled) {
        enabled = true;
        scheduleTicker.attach_ms(SCHEDULE_CHECK_INTERVAL, checkSchedulesCallback);
        SystemLogger.info("Scheduler habilitado", "SCHEDULE");
    } else if (!state && enabled) {
        enabled = false;
        scheduleTicker.detach();
        SystemLogger.info("Scheduler deshabilitado", "SCHEDULE");
    }
}

void ScheduleManager::checkSchedulesCallback() {
    if (schedulerInstance) {
        schedulerInstance->checkSchedules();
    }
}

void ScheduleManager::checkSchedules() {
    if (!enabled || !Time.isTimeValid()) return;
    
    uint8_t currentHour = Time.getCurrentHour();
    uint8_t currentMinute = Time.getCurrentMinute();
    uint8_t currentDay = Time.getCurrentDayOfWeek();
    
    auto schedules = Database.getActiveSchedules();
    
    for (const auto& schedule : schedules) {
        if (Database.shouldExecuteSchedule(schedule, currentHour, currentMinute, currentDay)) {
            // Verificar si ya se ejecutó recientemente
            uint32_t now = millis();
            if (now - lastExecutionTime[schedule.id % MAX_SCHEDULES] > 60000) {  // No repetir en 1 minuto
                executeSchedule(schedule);
                lastExecutionTime[schedule.id % MAX_SCHEDULES] = now;
            }
        }
    }
}

void ScheduleManager::executeSchedule(const Schedule& schedule) {
    SystemLogger.info("Ejecutando programación: " + schedule.name, "SCHEDULE");
    
    // Determinar acción basada en la hora
    uint8_t currentHour = Time.getCurrentHour();
    uint8_t currentMinute = Time.getCurrentMinute();
    uint16_t currentTime = currentHour * 60 + currentMinute;
    uint16_t onTime = schedule.hourOn * 60 + schedule.minuteOn;
    
    ScheduleAction action;
    if (abs((int)currentTime - (int)onTime) < 5) {  // Dentro de 5 minutos del tiempo de encendido
        action = ACTION_TURN_ON;
    } else {
        action = ACTION_TURN_OFF;
    }
    
    // Ejecutar callback si está definido
    if (actionCallback) {
        if (schedule.zones.length() > 0) {
            // Acción sobre zonas
            actionCallback(action == ACTION_TURN_ON ? ACTION_ZONE_ON : ACTION_ZONE_OFF, 
                         schedule.zones, 100);
        } else {
            // Acción global
            actionCallback(action, "all", 100);
        }
    }
    
    // Registrar evento
    Database.logEvent("SCHEDULE", EVENT_SCHEDULE, 
                     "Programación ejecutada: " + schedule.name + 
                     " - Acción: " + (action == ACTION_TURN_ON ? "Encender" : "Apagar"),
                     "SCHEDULER");
}

void ScheduleManager::calculateSunTimes() {
    // Cálculo simplificado para San Luis, Argentina
    // En una implementación real, usar librería de cálculo solar
    float latitude = DEFAULT_LAT;
    float longitude = DEFAULT_LNG;
    
    // Aproximación simple (ajustar según estación)
    uint8_t month = Time.getCurrentMonth();
    if (month >= 4 && month <= 9) {  // Otoño/Invierno
        sunriseHour = 7;
        sunriseMinute = 30;
        sunsetHour = 18;
        sunsetMinute = 0;
    } else {  // Primavera/Verano
        sunriseHour = 6;
        sunriseMinute = 0;
        sunsetHour = 20;
        sunsetMinute = 0;
    }
}

uint32_t ScheduleManager::createSchedule(const String& name, TriggerType trigger,
                                        ScheduleAction action, const String& target) {
    uint8_t hourOn = 18, minuteOn = 0;
    uint8_t hourOff = 6, minuteOff = 0;
    
    // Ajustar según trigger
    if (trigger == TRIGGER_SUNSET) {
        hourOn = sunsetHour;
        minuteOn = sunsetMinute + SUNSET_OFFSET;
    } else if (trigger == TRIGGER_SUNRISE) {
        hourOff = sunriseHour;
        minuteOff = sunriseMinute + SUNRISE_OFFSET;
    }
    
    // Crear en base de datos
    uint32_t id = Database.addSchedule(name, hourOn, minuteOn, hourOff, minuteOff, 0x7F);  // Todos los días
    
    SystemLogger.info("Programación creada: " + name + " (ID: " + String(id) + ")", "SCHEDULE");
    return id;
}

void ScheduleManager::createDefaultSchedules() {
    SystemLogger.info("Creando programaciones por defecto", "SCHEDULE");
    
    // Programación nocturna básica
    createNightSchedule();
    
    // Programación matutina
    createMorningSchedule();
    
    // Programación fin de semana
    createWeekendSchedule();
}

void ScheduleManager::createNightSchedule() {
    uint32_t id = Database.addSchedule(
        "Nocturno Automático",
        sunsetHour, sunsetMinute + 30,  // 30 min después del atardecer
        sunriseHour, sunriseMinute - 30,  // 30 min antes del amanecer
        0x7F  // Todos los días
    );
    
    SystemLogger.info("Programación nocturna creada (ID: " + String(id) + ")", "SCHEDULE");
}

void ScheduleManager::createMorningSchedule() {
    uint32_t id = Database.addSchedule(
        "Apagado Matutino",
        5, 0,   // Solo para apagar
        6, 30,  // Apagar a las 6:30
        0x3E  // Lun-Vie (0011 1110)
    );
    
    Database.enableSchedule(id, false);  // Deshabilitado por defecto
    SystemLogger.info("Programación matutina creada (ID: " + String(id) + ")", "SCHEDULE");
}

void ScheduleManager::createWeekendSchedule() {
    uint32_t id = Database.addSchedule(
        "Fin de Semana",
        17, 30,  // Encender más temprano
        23, 59,  // Apagar más tarde
        0x41  // Sáb-Dom (0100 0001)
    );
    
    SystemLogger.info("Programación fin de semana creada (ID: " + String(id) + ")", "SCHEDULE");
}

std::vector<Schedule> ScheduleManager::getTodaySchedules() {
    std::vector<Schedule> todaySchedules;
    uint8_t today = Time.getCurrentDayOfWeek();
    
    auto allSchedules = Database.getAllSchedules();
    for (const auto& schedule : allSchedules) {
        if (schedule.daysOfWeek & (1 << today)) {
            todaySchedules.push_back(schedule);
        }
    }
    
    return todaySchedules;
}

String ScheduleManager::getScheduleStats() {
    StaticJsonDocument<512> doc;
    
    auto schedules = Database.getAllSchedules();
    int activeCount = 0;
    for (const auto& s : schedules) {
        if (s.enabled) activeCount++;
    }
    
    doc["total"] = schedules.size();
    doc["active"] = activeCount;
    doc["sunrise"] = getSunriseTime();
    doc["sunset"] = getSunsetTime();
    doc["scheduler_enabled"] = enabled;
    doc["time_valid"] = Time.isTimeValid();
    
    String result;
    serializeJson(doc, result);
    return result;
}

// === TIME MANAGER ===

TimeManager::TimeManager() {
    lastNTPUpdate = 0;
    timeValid = false;
}

bool TimeManager::begin() {
    SystemLogger.info("Iniciando TimeManager", "TIME");
    
    // Establecer tiempo inicial (ejemplo)
    setTime(12, 0, 0, 17, 1, 2025);  // 12:00:00 17/01/2025
    timeValid = true;
    
    SystemLogger.info("Tiempo configurado: " + getDateTimeString(), "TIME");
    return true;
}

uint8_t TimeManager::getCurrentHour() {
    return hour();
}

uint8_t TimeManager::getCurrentMinute() {
    return minute();
}

uint8_t TimeManager::getCurrentSecond() {
    return second();
}

uint8_t TimeManager::getCurrentDay() {
    return day();
}

uint8_t TimeManager::getCurrentMonth() {
    return month();
}

uint16_t TimeManager::getCurrentYear() {
    return year();
}

uint8_t TimeManager::getCurrentDayOfWeek() {
    return weekday() - 1;  // 0=Domingo, 1=Lunes, etc.
}

String TimeManager::getTimeString() {
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", 
             getCurrentHour(), getCurrentMinute(), getCurrentSecond());
    return String(buffer);
}

String TimeManager::getDateString() {
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d",
             getCurrentDay(), getCurrentMonth(), getCurrentYear());
    return String(buffer);
}

String TimeManager::getDateTimeString() {
    return getDateString() + " " + getTimeString();
}

bool TimeManager::isDaytime() {
    uint8_t h = getCurrentHour();
    return (h >= 6 && h < 18);  // Simplificado
}

bool TimeManager::isNighttime() {
    return !isDaytime();
}

bool TimeManager::isWeekend() {
    uint8_t dow = getCurrentDayOfWeek();
    return (dow == 0 || dow == 6);  // Domingo o Sábado
}

bool TimeManager::isWeekday() {
    return !isWeekend();
}

bool TimeManager::isHoliday() {
    // Implementar feriados argentinos
    uint8_t d = getCurrentDay();
    uint8_t m = getCurrentMonth();
    
    // Algunos feriados fijos
    if ((d == 1 && m == 1) ||   // Año Nuevo
        (d == 25 && m == 5) ||  // 25 de Mayo
        (d == 9 && m == 7) ||   // Día de la Independencia
        (d == 25 && m == 12)) { // Navidad
        return true;
    }
    
    return false;
}