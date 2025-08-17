#include "SceneManager.h"

SceneManager Scenes;
DimmingController Dimming;
ZoneVisualManager ZoneVisual;

// === SCENE MANAGER ===

SceneManager::SceneManager() {
    activeScene = nullptr;
    transitioning = false;
    transitionStartTime = 0;
}

bool SceneManager::begin() {
    SystemLogger.info("Iniciando SceneManager", "SCENE");
    
    // Cargar escenas guardadas
    loadPresetsFromFile();
    
    // Crear presets por defecto
    createDefaultPresets();
    
    SystemLogger.info("SceneManager iniciado con " + String(scenes.size()) + " escenas", "SCENE");
    return true;
}

uint32_t SceneManager::createScene(const String& name, SceneType type) {
    Scene newScene;
    newScene.id = scenes.size() + 1;
    newScene.name = name;
    newScene.type = type;
    newScene.enabled = true;
    newScene.lastActivated = 0;
    newScene.activationCount = 0;
    
    scenes.push_back(newScene);
    
    SystemLogger.info("Escena creada: " + name + " (ID: " + String(newScene.id) + ")", "SCENE");
    return newScene.id;
}

bool SceneManager::addActionToScene(uint32_t sceneId, const SceneAction& action) {
    for (auto& scene : scenes) {
        if (scene.id == sceneId) {
            scene.actions.push_back(action);
            SystemLogger.debug("Acción agregada a escena " + String(sceneId), "SCENE");
            return true;
        }
    }
    return false;
}

bool SceneManager::activateScene(uint32_t sceneId) {
    Scene* scene = getScene(sceneId);
    if (!scene || !scene->enabled) {
        SystemLogger.error("Escena no encontrada o deshabilitada: " + String(sceneId), "SCENE");
        return false;
    }
    
    SystemLogger.info("Activando escena: " + scene->name, "SCENE");
    
    activeScene = scene;
    transitioning = true;
    transitionStartTime = millis();
    scene->lastActivated = millis();
    scene->activationCount++;
    
    // Ejecutar todas las acciones
    for (const auto& action : scene->actions) {
        if (action.delay > 0) {
            // Programar ejecución con delay
            // En producción usar un sistema de timers
            delay(action.delay);
        }
        executeAction(action);
    }
    
    // Notificar callbacks
    for (const auto& callback : activationCallbacks) {
        callback(*scene);
    }
    
    transitioning = false;
    return true;
}

void SceneManager::executeAction(const SceneAction& action) {
    SystemLogger.debug("Ejecutando acción: " + action.targetId + " -> " + String(action.brightness) + "%", "SCENE");
    
    if (action.isZone) {
        setZoneBrightness(action.targetId, action.brightness, action.transitionTime);
    } else {
        setLightBrightness(action.targetId, action.brightness, action.transitionTime);
    }
}

bool SceneManager::setLightBrightness(const String& lightId, uint8_t brightness, uint32_t transitionTime) {
    uint8_t currentBright = currentBrightness[lightId];
    targetBrightness[lightId] = brightness;
    
    if (transitionTime > 0) {
        applyTransition(lightId, currentBright, brightness, TRANSITION_FADE, transitionTime);
    } else {
        currentBrightness[lightId] = brightness;
        if (dimmingCallback) {
            dimmingCallback(lightId, brightness);
        }
    }
    
    return true;
}

bool SceneManager::setZoneBrightness(const String& zoneId, uint8_t brightness, uint32_t transitionTime) {
    auto lights = ZoneVisual.getZoneLights(zoneId);
    for (const auto& lightId : lights) {
        setLightBrightness(lightId, brightness, transitionTime);
    }
    return true;
}

void SceneManager::applyTransition(const String& targetId, uint8_t fromBright, uint8_t toBright, 
                                  TransitionType type, uint32_t duration) {
    // Implementación simplificada de transición
    // En producción usar interpolación más sofisticada
    
    uint32_t steps = duration / 50;  // Actualizar cada 50ms
    float stepSize = (float)(toBright - fromBright) / steps;
    
    for (uint32_t i = 0; i <= steps; i++) {
        uint8_t newBright = fromBright + (stepSize * i);
        currentBrightness[targetId] = newBright;
        
        if (dimmingCallback) {
            dimmingCallback(targetId, newBright);
        }
        
        delay(50);  // En producción usar timers no bloqueantes
    }
}

void SceneManager::createDefaultPresets() {
    // Escena: Modo Trabajo
    registerPreset("work_mode", SCENE_MANUAL, [this]() {
        uint32_t sceneId = createScene("Modo Trabajo", SCENE_MANUAL);
        SceneAction action;
        action.targetId = "all";
        action.isZone = true;
        action.brightness = 100;
        action.transition = TRANSITION_FADE;
        action.transitionTime = 1000;
        addActionToScene(sceneId, action);
    });
    
    // Escena: Modo Ahorro
    registerPreset("eco_mode", SCENE_ENERGY_SAVE, [this]() {
        uint32_t sceneId = createScene("Modo Eco", SCENE_ENERGY_SAVE);
        SceneAction action;
        action.targetId = "all";
        action.isZone = true;
        action.brightness = 60;
        action.transition = TRANSITION_FADE;
        action.transitionTime = 3000;
        addActionToScene(sceneId, action);
    });
    
    // Escena: Modo Nocturno
    registerPreset("night_mode", SCENE_AUTOMATIC, [this]() {
        uint32_t sceneId = createScene("Modo Nocturno", SCENE_AUTOMATIC);
        SceneAction action;
        action.targetId = "all";
        action.isZone = true;
        action.brightness = 30;
        action.transition = TRANSITION_FADE;
        action.transitionTime = 5000;
        addActionToScene(sceneId, action);
    });
    
    // Escena: Emergencia
    registerPreset("emergency", SCENE_EMERGENCY, [this]() {
        uint32_t sceneId = createScene("Emergencia", SCENE_EMERGENCY);
        SceneAction action;
        action.targetId = "all";
        action.isZone = true;
        action.brightness = 100;
        action.transition = TRANSITION_INSTANT;
        action.transitionTime = 0;
        addActionToScene(sceneId, action);
    });
    
    // Escena: Festiva
    registerPreset("festive", SCENE_FESTIVE, [this]() {
        uint32_t sceneId = createScene("Modo Festivo", SCENE_FESTIVE);
        // Crear efecto de onda
        for (int i = 0; i < 10; i++) {
            SceneAction action;
            action.targetId = "zone_" + String(i);
            action.isZone = true;
            action.brightness = (i % 2 == 0) ? 100 : 50;
            action.delay = i * 200;
            action.transition = TRANSITION_WAVE;
            action.transitionTime = 1000;
            addActionToScene(sceneId, action);
        }
    });
}

void SceneManager::registerPreset(const String& name, SceneType type, std::function<void()> setup) {
    ScenePreset preset;
    preset.name = name;
    preset.type = type;
    preset.setupFunction = setup;
    presets[name] = preset;
    
    SystemLogger.debug("Preset registrado: " + name, "SCENE");
}

bool SceneManager::activatePreset(const String& presetName) {
    if (presets.find(presetName) != presets.end()) {
        presets[presetName].setupFunction();
        Scene* scene = getSceneByName(presets[presetName].name);
        if (scene) {
            return activateScene(scene->id);
        }
    }
    return false;
}

// Efectos especiales
void SceneManager::waveEffect(const String& zoneId, uint32_t duration) {
    SystemLogger.info("Aplicando efecto onda en zona: " + zoneId, "SCENE");
    
    auto lights = ZoneVisual.getZoneLights(zoneId);
    uint32_t delayPerLight = duration / lights.size();
    
    for (size_t i = 0; i < lights.size(); i++) {
        setLightBrightness(lights[i], 100, 500);
        delay(delayPerLight);
        setLightBrightness(lights[i], 30, 500);
    }
}

void SceneManager::randomEffect(const String& zoneId, uint32_t duration) {
    SystemLogger.info("Aplicando efecto aleatorio en zona: " + zoneId, "SCENE");
    
    auto lights = ZoneVisual.getZoneLights(zoneId);
    uint32_t endTime = millis() + duration;
    
    while (millis() < endTime) {
        int randomLight = random(0, lights.size());
        uint8_t randomBrightness = random(20, 100);
        setLightBrightness(lights[randomLight], randomBrightness, 200);
        delay(100);
    }
}

void SceneManager::activateEmergencyLighting() {
    SystemLogger.warning("¡ILUMINACIÓN DE EMERGENCIA ACTIVADA!", "SCENE");
    
    // Encender todas las luces al máximo instantáneamente
    for (auto& pair : currentBrightness) {
        currentBrightness[pair.first] = 100;
        if (dimmingCallback) {
            dimmingCallback(pair.first, 100);
        }
    }
}

void SceneManager::activateEcoMode() {
    SystemLogger.info("Modo Eco activado", "SCENE");
    
    // Reducir brillo general al 60%
    for (auto& pair : currentBrightness) {
        setLightBrightness(pair.first, 60, 3000);
    }
}

void SceneManager::activateNightMode() {
    SystemLogger.info("Modo Nocturno activado", "SCENE");
    
    // Reducir brillo general al 30%
    for (auto& pair : currentBrightness) {
        setLightBrightness(pair.first, 30, 5000);
    }
}

void SceneManager::fadeIn(const String& targetId, uint32_t duration) {
    setLightBrightness(targetId, 100, duration);
}

void SceneManager::fadeOut(const String& targetId, uint32_t duration) {
    setLightBrightness(targetId, 0, duration);
}

void SceneManager::pulsate(const String& targetId, uint8_t minBright, uint8_t maxBright, uint32_t period) {
    // Efecto de pulsación
    uint32_t halfPeriod = period / 2;
    setLightBrightness(targetId, maxBright, halfPeriod);
    delay(halfPeriod);
    setLightBrightness(targetId, minBright, halfPeriod);
}

String SceneManager::getSceneStatistics() {
    StaticJsonDocument<1024> doc;
    doc["total_scenes"] = scenes.size();
    doc["active_scene"] = activeScene ? activeScene->name : "none";
    doc["transitioning"] = transitioning;
    
    JsonArray sceneList = doc.createNestedArray("scenes");
    for (const auto& scene : scenes) {
        JsonObject sceneObj = sceneList.createNestedObject();
        sceneObj["id"] = scene.id;
        sceneObj["name"] = scene.name;
        sceneObj["type"] = scene.type;
        sceneObj["enabled"] = scene.enabled;
        sceneObj["activations"] = scene.activationCount;
    }
    
    String result;
    serializeJson(doc, result);
    return result;
}

void SceneManager::loop() {
    // Verificar triggers automáticos
    static uint32_t lastCheck = 0;
    if (millis() - lastCheck > 10000) {  // Cada 10 segundos
        lastCheck = millis();
        checkAutomaticTriggers();
    }
    
    // Actualizar transiciones en progreso
    if (transitioning) {
        float progress = getTransitionProgress();
        if (progress >= 1.0) {
            transitioning = false;
        }
    }
}

float SceneManager::getTransitionProgress() {
    if (!transitioning) return 1.0;
    
    uint32_t elapsed = millis() - transitionStartTime;
    return min(1.0f, (float)elapsed / SCENE_TRANSITION_TIME);
}

void SceneManager::checkAutomaticTriggers() {
    for (auto& scene : scenes) {
        if (scene.type == SCENE_AUTOMATIC && scene.enabled) {
            if (evaluateTriggerCondition(scene.triggerCondition)) {
                activateScene(scene.id);
            }
        }
    }
}

bool SceneManager::evaluateTriggerCondition(const String& condition) {
    // Implementar evaluación de condiciones
    // Por ejemplo: "time=18:00", "ambient_light<50", etc.
    return false;
}

void SceneManager::loadPresetsFromFile() {
    // Cargar escenas desde archivo
    // Implementación pendiente
}

void SceneManager::saveScenesToFile() {
    // Guardar escenas en archivo
    // Implementación pendiente
}

Scene* SceneManager::getScene(uint32_t sceneId) {
    for (auto& scene : scenes) {
        if (scene.id == sceneId) {
            return &scene;
        }
    }
    return nullptr;
}

Scene* SceneManager::getSceneByName(const String& name) {
    for (auto& scene : scenes) {
        if (scene.name == name) {
            return &scene;
        }
    }
    return nullptr;
}

std::vector<Scene> SceneManager::getAllScenes() {
    return scenes;
}

void SceneManager::onSceneActivated(SceneActivatedCallback callback) {
    activationCallbacks.push_back(callback);
}

void SceneManager::setDimmingCallback(DimmingCallback callback) {
    dimmingCallback = callback;
}

uint8_t SceneManager::getLightBrightness(const String& lightId) {
    if (currentBrightness.find(lightId) != currentBrightness.end()) {
        return currentBrightness[lightId];
    }
    return 0;
}

// === DIMMING CONTROLLER ===

DimmingController::DimmingController() {
    minBrightness = 0;
    maxBrightness = 100;
    smoothDimming = true;
    dimmingSpeed = 50;  // ms entre pasos
}

void DimmingController::setLimits(uint8_t min, uint8_t max) {
    minBrightness = min;
    maxBrightness = max;
}

void DimmingController::setBrightness(const String& id, uint8_t level) {
    level = constrain(level, minBrightness, maxBrightness);
    brightnessLevels[id] = level;
    lastUpdateTime[id] = millis();
}

uint8_t DimmingController::getBrightness(const String& id) {
    if (brightnessLevels.find(id) != brightnessLevels.end()) {
        return brightnessLevels[id];
    }
    return 0;
}

void DimmingController::adjustBrightness(const String& id, int8_t delta) {
    uint8_t current = getBrightness(id);
    setBrightness(id, current + delta);
}

void DimmingController::fadeTo(const String& id, uint8_t target, uint32_t duration) {
    // Implementar fade con timers no bloqueantes
    uint8_t current = getBrightness(id);
    uint32_t steps = duration / dimmingSpeed;
    float stepSize = (float)(target - current) / steps;
    
    // En producción usar Ticker o similar
    for (uint32_t i = 0; i <= steps; i++) {
        setBrightness(id, current + (stepSize * i));
        delay(dimmingSpeed);
    }
}

// === ZONE VISUAL MANAGER ===

ZoneVisualManager::ZoneVisualManager() {
    // Constructor
}

bool ZoneVisualManager::createZone(const String& id, const String& name) {
    VisualZone newZone;
    newZone.id = id;
    newZone.name = name;
    newZone.defaultBrightness = 100;
    newZone.active = false;
    newZone.color = "#FFFFFF";
    
    zones.push_back(newZone);
    SystemLogger.info("Zona visual creada: " + name, "ZONE");
    return true;
}

bool ZoneVisualManager::addLightToZone(const String& zoneId, const String& lightId) {
    for (auto& zone : zones) {
        if (zone.id == zoneId) {
            zone.lightIds.push_back(lightId);
            lightToZoneMap[lightId] = zoneId;
            return true;
        }
    }
    return false;
}

std::vector<String> ZoneVisualManager::getZoneLights(const String& zoneId) {
    for (const auto& zone : zones) {
        if (zone.id == zoneId) {
            return zone.lightIds;
        }
    }
    return std::vector<String>();
}

String ZoneVisualManager::getZoneMapJSON() {
    StaticJsonDocument<2048> doc;
    JsonArray zonesArray = doc.createNestedArray("zones");
    
    for (const auto& zone : zones) {
        JsonObject zoneObj = zonesArray.createNestedObject();
        zoneObj["id"] = zone.id;
        zoneObj["name"] = zone.name;
        zoneObj["color"] = zone.color;
        zoneObj["active"] = zone.active;
        zoneObj["brightness"] = zone.defaultBrightness;
        
        JsonArray lights = zoneObj.createNestedArray("lights");
        for (const auto& lightId : zone.lightIds) {
            lights.add(lightId);
        }
    }
    
    String result;
    serializeJson(doc, result);
    return result;
}