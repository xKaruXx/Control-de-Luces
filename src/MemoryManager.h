#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "config.h"
#include "Logger.h"

class MemoryManager {
private:
    uint32_t lastHeapSize = 0;
    uint32_t minHeapSize = 0xFFFFFFFF;
    uint32_t maxHeapSize = 0;
    uint32_t fragmentationLevel = 0;
    unsigned long lastCheck = 0;
    const unsigned long CHECK_INTERVAL = 10000; // Chequear cada 10 segundos
    
    // Umbrales de memoria
    const uint32_t CRITICAL_HEAP = 5000;   // Crítico: menos de 5KB
    const uint32_t WARNING_HEAP = 10000;   // Advertencia: menos de 10KB
    const uint32_t SAFE_HEAP = 15000;      // Seguro: más de 15KB
    
public:
    MemoryManager();
    
    void begin();
    void check();
    bool isMemoryLow();
    bool isMemoryCritical();
    void performCleanup();
    void forceGarbageCollection();
    
    // Getters para estadísticas
    uint32_t getFreeHeap() { return ESP.getFreeHeap(); }
    uint32_t getMaxFreeBlockSize() { return ESP.getMaxFreeBlockSize(); }
    uint32_t getHeapFragmentation() { return fragmentationLevel; }
    uint32_t getMinHeap() { return minHeapSize; }
    uint32_t getMaxHeap() { return maxHeapSize; }
    
    String getMemoryStats();
    void logMemoryStatus();
    
    // Acciones de recuperación
    void emergencyCleanup();
    bool requestMemory(size_t size);
};

extern MemoryManager MemManager;

#endif // MEMORY_MANAGER_H