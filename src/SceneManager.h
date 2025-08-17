#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>
#include "config.h"
#include "Logger.h"
#include "DatabaseManager.h"

// Configuración de escenas
#define MAX_SCENES 20
#define MAX_SCENE_ACTIONS 50
#define SCENE_TRANSITION_TIME 2000  // Tiempo de transición en ms

// Tipos de escena
enum SceneType {
    SCENE_MANUAL,        // Escena manual definida por usuario
    SCENE_AUTOMATIC,     // Escena automática basada en condiciones
    SCENE_EMERGENCY,     // Escena de emergencia
    SCENE_ENERGY_SAVE,   // Escena de ahorro energético
    SCENE_FESTIVE,       // Escena festiva/eventos
    SCENE_MAINTENANCE    // Escena de mantenimiento
};

// Tipos de transición
enum TransitionType {
    TRANSITION_INSTANT,   // Cambio instantáneo
    TRANSITION_FADE,      // Desvanecimiento gradual
    TRANSITION_WAVE,      // Efecto onda
    TRANSITION_RANDOM,    // Aleatorio por luminaria
    TRANSITION_SEQUENCE   // Secuencial por orden
};

// Acción de escena
struct SceneAction {
    String targetId;        // ID de luminaria o zona
    bool isZone;           // true si es zona, false si es luminaria
    uint8_t brightness;    // 0-100%
    uint32_t delay;        // Delay antes de ejecutar (ms)
    String color;          // Color RGB (futuro)
    TransitionType transition;
    uint32_t transitionTime;
};

// Estructura de escena
struct Scene {
    uint32_t id;
    String name;
    String description;
    SceneType type;
    bool enabled;
    std::vector<SceneAction> actions;
    String triggerCondition;  // Condición para activación automática
    uint32_t lastActivated;
    uint32_t activationCount;
    JsonDocument metadata;
};

// Preset de escena
struct ScenePreset {
    String name;
    String description;
    SceneType type;
    std::function<void()> setupFunction;
};

// Callbacks
typedef std::function<void(const Scene& scene)> SceneActivatedCallback;
typedef std::function<void(const String& targetId, uint8_t brightness)> DimmingCallback;

class SceneManager {
private:
    std::vector<Scene> scenes;
    std::map<String, ScenePreset> presets;
    Scene* activeScene;
    bool transitioning;
    uint32_t transitionStartTime;
    
    // Callbacks
    std::vector<SceneActivatedCallback> activationCallbacks;
    DimmingCallback dimmingCallback;
    
    // Control de dimming
    std::map<String, uint8_t> currentBrightness;
    std::map<String, uint8_t> targetBrightness;
    
    // Métodos privados
    void executeAction(const SceneAction& action);
    void applyTransition(const String& targetId, uint8_t fromBright, uint8_t toBright, TransitionType type, uint32_t duration);
    bool evaluateTriggerCondition(const String& condition);
    void loadPresetsFromFile();
    void saveScenesToFile();
    
public:
    SceneManager();
    
    // Inicialización
    bool begin();
    void reset();
    
    // Gestión de escenas
    uint32_t createScene(const String& name, SceneType type = SCENE_MANUAL);
    bool addActionToScene(uint32_t sceneId, const SceneAction& action);
    bool updateScene(uint32_t sceneId, const Scene& scene);
    bool deleteScene(uint32_t sceneId);
    Scene* getScene(uint32_t sceneId);
    Scene* getSceneByName(const String& name);
    std::vector<Scene> getAllScenes();
    std::vector<Scene> getScenesByType(SceneType type);
    
    // Activación de escenas
    bool activateScene(uint32_t sceneId);
    bool activateScene(const String& sceneName);
    bool deactivateCurrentScene();
    bool isSceneActive(uint32_t sceneId);
    Scene* getActiveScene() { return activeScene; }
    
    // Control de dimming
    void setDimmingCallback(DimmingCallback callback);
    bool setLightBrightness(const String& lightId, uint8_t brightness, uint32_t transitionTime = 0);
    bool setZoneBrightness(const String& zoneId, uint8_t brightness, uint32_t transitionTime = 0);
    uint8_t getLightBrightness(const String& lightId);
    void fadeIn(const String& targetId, uint32_t duration = 2000);
    void fadeOut(const String& targetId, uint32_t duration = 2000);
    void pulsate(const String& targetId, uint8_t minBright = 20, uint8_t maxBright = 100, uint32_t period = 3000);
    
    // Escenas predefinidas
    void registerPreset(const String& name, SceneType type, std::function<void()> setup);
    void createDefaultPresets();
    bool activatePreset(const String& presetName);
    
    // Escenas automáticas
    void enableAutomaticScenes(bool enable);
    void checkAutomaticTriggers();
    bool setSceneTrigger(uint32_t sceneId, const String& condition);
    
    // Efectos especiales
    void waveEffect(const String& zoneId, uint32_t duration = 5000);
    void randomEffect(const String& zoneId, uint32_t duration = 10000);
    void sequenceEffect(const std::vector<String>& lightIds, uint32_t interval = 500);
    void strobeEffect(const String& targetId, uint32_t duration = 5000, uint32_t frequency = 100);
    void rainbowEffect(const String& zoneId, uint32_t duration = 10000);
    
    // Escenas de emergencia
    void activateEmergencyLighting();
    void activateEvacuationRoute(const std::vector<String>& routeLights);
    void flashAlert(const String& zoneId, uint32_t duration = 10000);
    
    // Escenas de ahorro
    void activateEcoMode();
    void activateNightMode();
    void adaptiveLighting(float ambientLight);
    
    // Callbacks
    void onSceneActivated(SceneActivatedCallback callback);
    
    // Estadísticas
    uint32_t getSceneActivationCount(uint32_t sceneId);
    String getMostUsedScene();
    float getAverageBrightness();
    String getSceneStatistics();
    
    // Loop de actualización
    void loop();
    bool isTransitioning() { return transitioning; }
    float getTransitionProgress();
};

// Instancia global
extern SceneManager Scenes;

// === CONTROL DE DIMMING ===

class DimmingController {
private:
    uint8_t minBrightness;
    uint8_t maxBrightness;
    bool smoothDimming;
    uint32_t dimmingSpeed;
    
    std::map<String, uint8_t> brightnessLevels;
    std::map<String, uint32_t> lastUpdateTime;
    
public:
    DimmingController();
    
    // Configuración
    void setLimits(uint8_t min, uint8_t max);
    void enableSmoothDimming(bool enable);
    void setDimmingSpeed(uint32_t speedMs);
    
    // Control directo
    void setBrightness(const String& id, uint8_t level);
    uint8_t getBrightness(const String& id);
    void adjustBrightness(const String& id, int8_t delta);
    
    // Efectos de dimming
    void fadeTo(const String& id, uint8_t target, uint32_t duration);
    void fadeToggle(const String& id);
    void breathe(const String& id, uint32_t period);
    
    // Control por grupo
    void setGroupBrightness(const std::vector<String>& ids, uint8_t level);
    void syncBrightness(const std::vector<String>& ids);
    
    // Presets de brillo
    void applyPreset(const String& preset);
    void saveCurrentAsPreset(const String& name);
    
    // Loop de actualización
    void update();
};

extern DimmingController Dimming;

// === GESTOR DE ZONAS VISUALES ===

class ZoneVisualManager {
private:
    struct VisualZone {
        String id;
        String name;
        std::vector<String> lightIds;
        String color;           // Color de la zona
        uint8_t defaultBrightness;
        bool active;
        JsonDocument properties;
    };
    
    std::vector<VisualZone> zones;
    std::map<String, String> lightToZoneMap;
    
public:
    ZoneVisualManager();
    
    // Gestión de zonas
    bool createZone(const String& id, const String& name);
    bool addLightToZone(const String& zoneId, const String& lightId);
    bool removeLightFromZone(const String& zoneId, const String& lightId);
    bool deleteZone(const String& zoneId);
    
    // Control de zonas
    void activateZone(const String& zoneId);
    void deactivateZone(const String& zoneId);
    void setZoneBrightness(const String& zoneId, uint8_t brightness);
    void setZoneColor(const String& zoneId, const String& color);
    
    // Consultas
    VisualZone* getZone(const String& zoneId);
    std::vector<VisualZone> getAllZones();
    String getLightZone(const String& lightId);
    std::vector<String> getZoneLights(const String& zoneId);
    
    // Visualización
    String getZoneMapJSON();
    String getZoneStatistics();
    
    // Efectos por zona
    void applyZoneEffect(const String& zoneId, const String& effect);
    void rotateZones(uint32_t interval);
    void cascadeEffect(uint32_t delay);
};

extern ZoneVisualManager ZoneVisual;

#endif // SCENE_MANAGER_H