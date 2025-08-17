# Roadmap - Sistema de Control de Alumbrado Público

Este documento describe las mejoras planificadas y la dirección futura del proyecto.

## Fase 1: Fundación (Q1 2025) ✅ COMPLETADA - v0.3.0 (17/01/2025)
### Objetivo: Establecer base sólida del proyecto

- [x] Documentación inicial (README, CHANGELOG, ROADMAP)
- [x] Estructura de carpetas organizada
- [x] Archivo de configuración centralizado (config.h)
- [x] Migración a PlatformIO
- [x] Sistema de logs con persistencia (Logger.h/cpp)
- [x] Manejo de errores mejorado
- [x] Reconexión WiFi automática (WifiManager.h/cpp)
- [x] Gestión de memoria avanzada (MemoryManager.h/cpp)
- [x] Dashboard de diagnóstico web
- [x] APIs REST expandidas
- [x] Buffer circular para eficiencia
- [x] Sistema de callbacks para eventos
- [ ] Tests unitarios básicos (postponed)

## Fase 2: Seguridad y Estabilidad (Q1-Q2 2025) ✅ COMPLETADA - v0.4.0 (17/01/2025)
### Objetivo: Sistema seguro y confiable

- [x] **Autenticación y Autorización**
  - [x] Sistema de login con usuarios y contraseñas
  - [x] Niveles de acceso (admin, operador, visualización)
  - [x] Sesiones con timeout (30 minutos)
  - [x] Logs de auditoría

- [x] **Protección y Validación**
  - [ ] HTTPS con certificados SSL (postponed - limitaciones ESP8266)
  - [x] Tokens seguros SHA-256
  - [x] Validación de entrada robusta
  - [x] Rate limiting (60 req/min)
  - [x] Protección XSS/SQL injection

- [x] **Estabilidad**
  - [x] Watchdog timer (8 segundos)
  - [x] Sistema de recuperación ante fallos
  - [x] Backup de configuración automático
  - [x] Restore de configuración

## Fase 3: Funcionalidades Avanzadas (Q2 2025) ✅ COMPLETADA - v0.5.0 (17/01/2025)
### Objetivo: Sistema inteligente y automatizado

- [x] **Base de Datos Local**
  - [x] Sistema de almacenamiento en memoria (DatabaseManager)
  - [x] Histórico de estados y eventos
  - [x] Registro de eventos y fallas
  - [x] Exportación de datos (CSV/JSON)

- [x] **Programación y Automatización**
  - [x] Programación horaria (encendido/apagado)
  - [x] Ajuste según calendario (días de la semana)
  - [x] Cálculo de horarios solares
  - [x] Programaciones predefinidas

- [x] **Sistema de Alertas**
  - [x] Detección automática de fallas
  - [x] Sistema de notificaciones preparado
  - [x] 8 tipos de alertas diferentes
  - [x] Dashboard de alertas en configuracion.html

- [x] **Adicionales Implementados**
  - [x] Gestión de zonas para agrupación de luminarias
  - [x] Monitoreo de consumo energético
  - [x] TimeManager para gestión temporal
  - [x] Sistema de callbacks entre componentes
  - [x] Página de configuración avanzada completa

## Fase 4: IoT y Escalabilidad (Q3 2025) ✅ COMPLETADA - v0.6.0 (17/01/2025)
### Objetivo: Sistema distribuido y escalable

- [x] **Protocolo MQTT**
  - [x] Cliente MQTT completo (MQTTManager)
  - [x] Comunicación con múltiples nodos
  - [x] Topics organizados por función
  - [x] QoS configurables

- [x] **Múltiples Nodos**
  - [x] Firmware para nodos secundarios
  - [x] Auto-descubrimiento de nodos
  - [x] Sincronización de estados
  - [ ] Mesh networking avanzado (postponed)

- [x] **OTA Updates**
  - [x] Actualización remota de firmware
  - [x] Versionado de firmware
  - [x] Sistema de seguridad con tokens
  - [x] Actualización individual y grupal

## Fase 5: Interfaz Moderna (Q3-Q4 2025) ✅ COMPLETADA - v0.7.0 (17/01/2025)
### Objetivo: UX/UI profesional y moderna

- [x] **Dashboard Avanzado**
  - [x] Estadísticas en tiempo real
  - [x] Gráficos de consumo energético
  - [x] Mapa de calor de fallas
  - [x] KPIs del sistema

- [x] **Control Granular**
  - [x] Control individual de luminarias
  - [x] Agrupación por zonas
  - [x] Dimming (control de intensidad)
  - [x] Escenas predefinidas

- [ ] **Progressive Web App**
  - [ ] Diseño responsive mejorado (parcial)
  - [ ] Modo offline
  - [ ] Instalable en móviles
  - [ ] Push notifications

## Fase 6: Inteligencia y Análisis (Q4 2025)
### Objetivo: Sistema predictivo e inteligente

- [ ] **Análisis de Datos**
  - [ ] Reportes automáticos mensuales
  - [ ] Análisis de patrones de fallas
  - [ ] Optimización de consumo
  - [ ] Predicción de mantenimiento

- [ ] **Machine Learning** (Opcional)
  - [ ] Predicción de fallas
  - [ ] Optimización automática de horarios
  - [ ] Detección de anomalías
  - [ ] Sugerencias de mejora

- [ ] **Integración Ciudad Inteligente**
  - [ ] API RESTful completa
  - [ ] Integración con otros sistemas municipales
  - [ ] Estándares de Smart Cities
  - [ ] Open Data

## Fase 7: Producción (2026)
### Objetivo: Sistema listo para deployment masivo

- [ ] **Hardware Profesional**
  - [ ] PCB personalizado
  - [ ] Carcasa IP65/IP67
  - [ ] Certificaciones eléctricas
  - [ ] Manual de instalación

- [ ] **Documentación Completa**
  - [ ] Manual de usuario
  - [ ] Manual técnico
  - [ ] API documentation
  - [ ] Videos tutoriales

- [ ] **Soporte y Mantenimiento**
  - [ ] Sistema de tickets
  - [ ] Actualizaciones regulares
  - [ ] SLA definidos
  - [ ] Capacitación a operadores

## Progreso Actual

### ✅ Completado
**Fase 1 - v0.3.0 (Fundación)**
- Sistema de logs persistente con rotación
- Gestión de memoria con auto-recovery
- WiFi Manager con reconexión automática
- Dashboard de diagnóstico completo
- 11 APIs REST
- Arquitectura modular (6 módulos)

**Fase 2 - v0.4.0 (Seguridad)**
- AuthManager con usuarios y sesiones
- SecurityManager con validación y rate limiting
- Watchdog Timer implementado
- Sistema de backup/restore
- 15 nuevas APIs protegidas
- Login page profesional
- 3 niveles de acceso (Admin/Operator/Viewer)

### 🔄 En Progreso (Fase 3)
- [ ] Base de datos local
- [ ] Programación horaria
- [ ] Sistema de alertas

### 📅 Próximas Prioridades (Enero-Febrero 2025)
1. ⚡ Implementar login básico con usuarios
2. ⚡ Agregar tokens de sesión
3. ⚡ Configurar HTTPS
4. ⚡ Crear sistema de respaldo de configuración
5. ⚡ Implementar watchdog timer

## Métricas de Éxito

### Logradas en v0.3.0:
- ✅ **Modularidad**: 6 módulos independientes
- ✅ **APIs**: 11 endpoints REST funcionales
- ✅ **Memoria**: Sistema de recuperación automática
- ✅ **Logs**: Sistema completo con 4 niveles
- ✅ **Diagnóstico**: Dashboard en tiempo real

### Objetivos Pendientes:
- **Confiabilidad**: 99.9% uptime (en evaluación)
- **Respuesta**: < 100ms para operaciones críticas
- **Escalabilidad**: Soportar 1000+ nodos
- **Seguridad**: 0 vulnerabilidades críticas
- **Usabilidad**: < 5 min curva de aprendizaje
- **Mantenibilidad**: < 1h para agregar features simples

## Contribución

Si deseas contribuir con alguna de estas características:
1. Revisa los issues abiertos
2. Comenta tu interés en trabajar en alguna feature
3. Sigue las guías de contribución en README.md
4. Abre un PR cuando esté listo

## Notas

- Las fechas son estimativas y pueden ajustarse
- Las prioridades pueden cambiar según necesidades
- Features marcadas como (Opcional) son nice-to-have
- Versión 1.0.0 se lanzará al completar Fase 3