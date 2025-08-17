#include "MemoryManager.h"

MemoryManager MemManager;

MemoryManager::MemoryManager() {}

void MemoryManager::begin() {
    lastHeapSize = ESP.getFreeHeap();
    minHeapSize = lastHeapSize;
    maxHeapSize = lastHeapSize;
    SystemLogger.info("MemoryManager iniciado. Heap libre: " + String(lastHeapSize) + " bytes", "MEMORY");
}

void MemoryManager::check() {
    if (millis() - lastCheck < CHECK_INTERVAL) return;
    lastCheck = millis();
    
    uint32_t currentHeap = ESP.getFreeHeap();
    uint32_t maxBlock = ESP.getMaxFreeBlockSize();
    
    // Actualizar estadísticas
    if (currentHeap < minHeapSize) minHeapSize = currentHeap;
    if (currentHeap > maxHeapSize) maxHeapSize = currentHeap;
    
    // Calcular fragmentación
    if (currentHeap > 0) {
        fragmentationLevel = 100 - (maxBlock * 100 / currentHeap);
    }
    
    // Detectar pérdida de memoria
    if (lastHeapSize - currentHeap > 1000) {
        SystemLogger.warning("Pérdida de memoria detectada: " + 
                           String(lastHeapSize - currentHeap) + " bytes", "MEMORY");
    }
    
    // Verificar niveles críticos
    if (currentHeap < CRITICAL_HEAP) {
        SystemLogger.error("Memoria CRÍTICA: " + String(currentHeap) + " bytes libres", "MEMORY");
        emergencyCleanup();
    } else if (currentHeap < WARNING_HEAP) {
        SystemLogger.warning("Memoria baja: " + String(currentHeap) + " bytes libres", "MEMORY");
        performCleanup();
    }
    
    // Alta fragmentación
    if (fragmentationLevel > 50) {
        SystemLogger.warning("Alta fragmentación de memoria: " + String(fragmentationLevel) + "%", "MEMORY");
    }
    
    lastHeapSize = currentHeap;
}

bool MemoryManager::isMemoryLow() {
    return ESP.getFreeHeap() < WARNING_HEAP;
}

bool MemoryManager::isMemoryCritical() {
    return ESP.getFreeHeap() < CRITICAL_HEAP;
}

void MemoryManager::performCleanup() {
    SystemLogger.info("Iniciando limpieza de memoria...", "MEMORY");
    
    uint32_t heapBefore = ESP.getFreeHeap();
    
    // Forzar recolección de basura
    ESP.wdtFeed();
    yield();
    
    // Flush de logs pendientes
    SystemLogger.flush();
    
    uint32_t heapAfter = ESP.getFreeHeap();
    
    if (heapAfter > heapBefore) {
        SystemLogger.info("Memoria recuperada: " + String(heapAfter - heapBefore) + " bytes", "MEMORY");
    }
}

void MemoryManager::forceGarbageCollection() {
    ESP.wdtFeed();
    delay(1);
    yield();
}

String MemoryManager::getMemoryStats() {
    StaticJsonDocument<512> doc;
    
    doc["free_heap"] = ESP.getFreeHeap();
    doc["max_free_block"] = ESP.getMaxFreeBlockSize();
    doc["heap_fragmentation"] = fragmentationLevel;
    doc["min_heap_seen"] = minHeapSize;
    doc["max_heap_seen"] = maxHeapSize;
    doc["sketch_size"] = ESP.getSketchSize();
    doc["free_sketch_space"] = ESP.getFreeSketchSpace();
    doc["chip_id"] = ESP.getChipId();
    doc["flash_chip_size"] = ESP.getFlashChipSize();
    doc["flash_chip_real_size"] = ESP.getFlashChipRealSize();
    doc["sdk_version"] = ESP.getSdkVersion();
    
    // Estados de memoria
    if (ESP.getFreeHeap() < CRITICAL_HEAP) {
        doc["status"] = "critical";
    } else if (ESP.getFreeHeap() < WARNING_HEAP) {
        doc["status"] = "warning";
    } else {
        doc["status"] = "ok";
    }
    
    String result;
    serializeJson(doc, result);
    return result;
}

void MemoryManager::logMemoryStatus() {
    String status = "Heap: " + String(ESP.getFreeHeap()) + 
                   " | Max Block: " + String(ESP.getMaxFreeBlockSize()) +
                   " | Frag: " + String(fragmentationLevel) + "%";
    SystemLogger.debug(status, "MEMORY");
}

void MemoryManager::emergencyCleanup() {
    SystemLogger.error("Ejecutando limpieza de emergencia!", "MEMORY");
    
    // Acciones drásticas para recuperar memoria
    WiFi.disconnect(false);  // Desconectar WiFi temporalmente
    delay(100);
    
    // Limpiar buffers
    SystemLogger.flush();
    SystemLogger.clearLogs();
    
    // Reconectar WiFi
    WiFi.reconnect();
    
    // Forzar recolección
    forceGarbageCollection();
    
    // Si aún es crítico, reiniciar
    if (ESP.getFreeHeap() < CRITICAL_HEAP / 2) {
        SystemLogger.error("Memoria insuficiente. Reiniciando sistema...", "MEMORY");
        delay(1000);
        ESP.restart();
    }
}

bool MemoryManager::requestMemory(size_t size) {
    if (ESP.getFreeHeap() < size + WARNING_HEAP) {
        performCleanup();
        
        if (ESP.getFreeHeap() < size + CRITICAL_HEAP) {
            SystemLogger.error("No hay suficiente memoria para asignar " + String(size) + " bytes", "MEMORY");
            return false;
        }
    }
    return true;
}