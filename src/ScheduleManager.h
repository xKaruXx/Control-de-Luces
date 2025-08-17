#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include <Arduino.h>
#include <TimeLib.h>
#include <Ticker.h>
#include "config.h"
#include "Logger.h"
#include "DatabaseManager.h"
#include <functional>

// Configuración del scheduler
#define SCHEDULE_CHECK_INTERVAL 30000  // Verificar cada 30 segundos
#define MAX_SCHEDULES 20
#define SUNRISE_OFFSET -30  // Minutos antes del amanecer
#define SUNSET_OFFSET 30    // Minutos después del atardecer

// Tipos de trigger
enum TriggerType {
    TRIGGER_TIME,      // Hora específica
    TRIGGER_SUNRISE,   // Amanecer
    TRIGGER_SUNSET,    // Atardecer
    TRIGGER_SENSOR,    // Sensor de luz
    TRIGGER_MANUAL     // Manual
};

// Acciones programadas
enum ScheduleAction {
    ACTION_TURN_ON,
    ACTION_TURN_OFF,
    ACTION_DIM,
    ACTION_ZONE_ON,
    ACTION_ZONE_OFF,
    ACTION_SCENE
};

// Callback para ejecutar acciones
typedef std::function<void(ScheduleAction action, String target, int value)> ScheduleCallback;

class ScheduleManager {
private:
    Ticker scheduleTicker;
    ScheduleCallback actionCallback;
    bool enabled;
    
    // Horarios calculados
    uint8_t sunriseHour;
    uint8_t sunriseMinute;
    uint8_t sunsetHour;
    uint8_t sunsetMinute;
    
    // Última ejecución
    uint32_t lastExecutionTime[MAX_SCHEDULES];
    
    // Funciones internas
    static void checkSchedulesCallback();
    void checkSchedules();
    void executeSchedule(const Schedule& schedule);
    void calculateSunTimes();
    bool isTimeToExecute(const Schedule& schedule);
    uint8_t getCurrentDayOfWeek();
    
public:
    ScheduleManager();
    
    // Inicialización
    bool begin();
    void setCallback(ScheduleCallback callback);
    
    // Control
    void enable(bool state = true);
    bool isEnabled() { return enabled; }
    void forceCheck();
    
    // Gestión de programaciones
    uint32_t createSchedule(const String& name, TriggerType trigger, 
                           ScheduleAction action, const String& target);
    bool updateScheduleTime(uint32_t id, uint8_t hourOn, uint8_t minuteOn,
                           uint8_t hourOff, uint8_t minuteOff);
    bool setScheduleDays(uint32_t id, uint8_t daysOfWeek);
    bool setScheduleZones(uint32_t id, const String& zones);
    bool enableSchedule(uint32_t id, bool enabled);
    bool deleteSchedule(uint32_t id);
    
    // Consultas
    std::vector<Schedule> getTodaySchedules();
    std::vector<Schedule> getUpcomingSchedules(uint8_t hours = 24);
    String getNextScheduleTime(uint32_t id);
    bool isScheduleActive(uint32_t id);
    
    // Horarios solares
    void setSunriseSunset(uint8_t riseH, uint8_t riseM, uint8_t setH, uint8_t setM);
    void updateSunTimes(float latitude, float longitude);
    String getSunriseTime() { return String(sunriseHour) + ":" + String(sunriseMinute); }
    String getSunsetTime() { return String(sunsetHour) + ":" + String(sunsetMinute); }
    
    // Escenas predefinidas
    void createDefaultSchedules();
    void createNightSchedule();    // Encender al atardecer
    void createMorningSchedule();  // Apagar al amanecer
    void createWeekendSchedule();  // Horario especial fin de semana
    void createHolidaySchedule();  // Horario días festivos
    
    // Estadísticas
    String getScheduleStats();
    uint32_t getExecutionCount(uint32_t scheduleId);
    String getLastExecutionTime(uint32_t scheduleId);
    
    // Mantenimiento
    void cleanupOldSchedules();
    void optimizeSchedules();
};

// Instancia global
extern ScheduleManager Scheduler;

// === FUNCIONES AUXILIARES DE TIEMPO ===

class TimeManager {
private:
    uint32_t lastNTPUpdate;
    bool timeValid;
    
public:
    TimeManager();
    
    bool begin();
    void updateTime();
    bool isTimeValid() { return timeValid; }
    
    // Obtener tiempo actual
    uint8_t getCurrentHour();
    uint8_t getCurrentMinute();
    uint8_t getCurrentSecond();
    uint8_t getCurrentDay();
    uint8_t getCurrentMonth();
    uint16_t getCurrentYear();
    uint8_t getCurrentDayOfWeek();
    
    // Formato de tiempo
    String getTimeString();
    String getDateString();
    String getDateTimeString();
    
    // Cálculos solares
    float calculateSunrise(float latitude, float longitude, int timezone = -3);
    float calculateSunset(float latitude, float longitude, int timezone = -3);
    bool isDaytime();
    bool isNighttime();
    
    // Días especiales
    bool isWeekend();
    bool isWeekday();
    bool isHoliday();  // Feriados argentinos
    void addHoliday(uint8_t day, uint8_t month);
};

extern TimeManager Time;

#endif // SCHEDULE_MANAGER_H