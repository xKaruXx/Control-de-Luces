# Changelog - Sistema de Control de Alumbrado Público

Todos los cambios notables en este proyecto serán documentados en este archivo.

El formato está basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.0.0/),
y este proyecto adhiere a [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Por Hacer
- WebSockets para actualizaciones en tiempo real
- HTTPS con certificados SSL
- Integración con servicios externos (Weather API, Smart Grid)
- Machine Learning para predicción de fallas
- Progressive Web App (PWA)
- Análisis predictivo de mantenimiento

## [0.7.0] - 2025-01-17
### Agregado - Fase 5: Interfaz Moderna
- **SceneManager Completo**
  - Sistema de escenas y dimming
  - 5 presets predefinidos (eco, nocturno, emergencia, trabajo, festivo)
  - Control de intensidad 0-100%
  - Transiciones suaves con fade
  - Efectos visuales (onda, aleatorio, pulsar)
  
- **Dashboard Moderno**
  - dashboard.html con diseño glassmorphism
  - KPIs con animaciones en tiempo real
  - Gráficos de consumo con Chart.js
  - Mapa de calor de fallas con Leaflet
  - Estadísticas y métricas avanzadas
  
- **Control de Escenas**
  - escenas.html interfaz completa
  - Control por zonas visuales
  - Dimming individual y grupal
  - Efectos especiales
  - Timeline de programación
  
- **Sistema de Zonas Visuales**
  - ZoneVisualManager integrado
  - Agrupación lógica de luminarias
  - Control por zona en mapa
  - Colores y visualización
  
- **APIs de Control Avanzado**
  - /api/scenes - gestión de escenas
  - /api/dimming - control de intensidad
  - /api/zones/control - control por zonas
  - /api/effects - efectos visuales
  - /api/zones/visual - mapa de zonas

### Mejorado
- main.cpp actualizado con SceneManager
- Estructura Luminaria con campos de zona y dimming
- Integración completa MQTT-Escenas
- Callbacks para sincronización de estados

## [0.6.0] - 2025-01-17
### Agregado - Fase 4: IoT y Escalabilidad
- **MQTTManager Completo**
  - Cliente MQTT con PubSubClient
  - Auto-discovery de nodos
  - Topics organizados por función
  - Comunicación bidireccional
  - Heartbeat y telemetría automática
  
- **Firmware para Nodos Secundarios**
  - node_luminaria.cpp independiente
  - Configuración persistente en EEPROM
  - Modo automático con horarios
  - Control por zonas
  - Telemetría de consumo
  
- **Sistema OTA (Over-The-Air)**
  - OTAManager.h/cpp completo
  - Actualización HTTP y ArduinoOTA
  - Verificación de versiones
  - Seguridad con tokens
  - Progreso en tiempo real
  
- **Dashboard Multi-Nodo**
  - nodos.html con gestión completa
  - Mapa con ubicación de nodos
  - Control individual y grupal
  - Estadísticas en tiempo real
  - Interfaz de actualización OTA
  
- **Comunicación IoT**
  - Protocolo MQTT implementado
  - Discovery automático
  - Sincronización de estados
  - Comandos broadcast
  - Sistema de alertas distribuido

### Mejorado
- main.cpp actualizado con integración MQTT
- Callbacks para eventos de nodos
- APIs REST para gestión MQTT
- Sincronización automática de estados
- Soporte para múltiples nodos simultáneos

### Técnico
- 3000+ líneas de código IoT
- Arquitectura distribuida
- Protocolo MQTT estándar
- Sistema OTA robusto

## [0.5.0] - 2025-01-17
### Agregado - Fase 3: Características Avanzadas Completada
- **Base de Datos Local**
  - DatabaseManager.h/cpp con almacenamiento completo
  - Tablas para eventos, zonas, programaciones y consumo
  - 500+ líneas de código para gestión de datos
  - Exportación en CSV y JSON
  - Limpieza automática de datos antiguos
  
- **Programación Horaria Inteligente**
  - ScheduleManager.h/cpp con control automático
  - Soporte para días de la semana específicos
  - Cálculo de horarios solares
  - Programaciones predefinidas (nocturno, fin de semana)
  - TimeManager para gestión temporal
  
- **Sistema de Alertas Avanzado**
  - AlertManager.h/cpp con detección inteligente
  - 8 tipos de alertas diferentes
  - 4 niveles de severidad
  - Sistema de notificaciones (email, webhook)
  - Reconocimiento y descarte de alertas
  - Condiciones automáticas configurables
  
- **Monitoreo de Consumo Energético**
  - Registro detallado por luminaria
  - Cálculo de costos estimados
  - Emisiones de CO2
  - Gráficos de consumo en tiempo real
  - Estadísticas históricas
  
- **Gestión de Zonas**
  - Agrupación lógica de luminarias
  - Control por zona
  - Asignación dinámica
  - Estadísticas por zona
  
- **Página de Configuración Avanzada**
  - configuracion.html con interfaz completa
  - 5 pestañas: Programación, Zonas, Alertas, Consumo, Exportar
  - Modales para agregar/editar
  - Gráficos con Chart.js
  - Backup y restauración del sistema

### Mejorado
- main.cpp actualizado a v0.5.0 con integración completa
- 15+ nuevas APIs REST para todas las funcionalidades
- Loop principal con verificaciones periódicas
- Callbacks entre componentes
- Inicialización ordenada de todos los managers

### Técnico
- 2000+ líneas de código nuevas
- Arquitectura modular completa
- Sistema event-driven con callbacks
- Gestión eficiente de memoria
- Compatibilidad con ESP8266 limitado

## [0.4.0] - 2025-01-17
### Agregado - Fase 2: Seguridad y Estabilidad Completada
- **Sistema de Autenticación Completo**
  - AuthManager.h/cpp con gestión de usuarios y sesiones
  - 3 niveles de acceso: Admin, Operator, Viewer
  - Tokens SHA-256 seguros
  - Sesiones con timeout de 30 minutos
  - Bloqueo por intentos fallidos
  
- **Seguridad Avanzada**
  - SecurityManager.h/cpp con múltiples capas de protección
  - Watchdog Timer para prevenir bloqueos
  - Rate limiting (60 req/min por IP)
  - Validación robusta de entradas
  - Protección contra XSS y SQL injection
  - Sistema de backup/restore automático
  
- **Página de Login**
  - Interfaz profesional con gradientes
  - Manejo de sesiones en localStorage
  - Indicadores visuales de estado
  - Modal con usuarios demo
  
- **APIs Protegidas por Roles**
  - Middleware REQUIRE_AUTH para protección
  - 15 nuevas APIs de administración
  - Logs de auditoría para acciones críticas
  - Control granular por permisos

### Mejorado
- main.cpp actualizado con sistema de seguridad completo
- Todas las rutas ahora requieren autenticación
- Rate limiting en todas las APIs
- Validación de entradas en todos los endpoints
- Mejor manejo de errores y respuestas

### Seguridad
- Usuarios por defecto: admin/admin123, operator/oper123, viewer/view123
- Sesiones con tokens únicos y seguros
- Protección contra ataques comunes
- Backup automático cada hora
- Logs de eventos de seguridad

## [0.3.0] - 2025-01-17
### Agregado - Fase 1 Completada
- **Sistema de Logs Persistente**
  - Logger.h/cpp con buffer circular
  - Logs con niveles (ERROR, WARNING, INFO, DEBUG)
  - Persistencia en archivo con rotación automática
  - API para consulta y descarga de logs
  
- **Gestión Avanzada de Memoria**
  - MemoryManager.h/cpp para monitoreo continuo
  - Detección de pérdida de memoria
  - Auto-recuperación en situaciones críticas
  - Estadísticas detalladas de uso
  
- **WiFi Manager Robusto**
  - WifiManager.h/cpp con reconexión automática
  - Callbacks para eventos de conexión/desconexión
  - Modo AP de respaldo
  - Estadísticas de conexión
  
- **Página de Diagnóstico Web**
  - Dashboard completo en /diagnostico.html
  - Métricas en tiempo real
  - Gráficos de memoria
  - Visor de logs integrado
  - Acciones del sistema
  
- **APIs REST Expandidas**
  - /api/system/* - Información y control del sistema
  - /api/wifi/* - Estadísticas WiFi
  - /api/memory/* - Gestión de memoria
  - /api/logs/* - Sistema de logs

### Mejorado
- main.cpp completamente refactorizado con arquitectura modular
- IDs únicos para luminarias
- Mejor manejo de errores
- Callbacks para eventos del sistema
- Código más mantenible y escalable

### Técnico
- CircularBuffer implementado para eficiencia
- Separación de responsabilidades en módulos
- Mejor uso de memoria con validaciones
- Sistema de heartbeat mejorado

## [0.2.0] - 2025-01-17
### Agregado
- README.md con documentación completa del proyecto
- CHANGELOG.md para control de versiones
- ROADMAP.md con plan de desarrollo futuro
- Estructura de carpetas organizada (src/, data/, lib/, docs/)
- Archivo config.h para centralizar configuración
- platformio.ini para gestión de dependencias

### Mejorado
- Refactorización del código principal a src/main.cpp
- Mejor organización del proyecto
- Documentación técnica y de uso

### Seguridad
- Movidas credenciales WiFi a archivo de configuración separado

## [0.1.0] - 2024-12-15
### Inicial
- Servidor web asíncrono en ESP8266
- Interfaz web con mapa interactivo (Leaflet)
- Panel de administración con AdminLTE
- API REST básica para gestión de luminarias
- Sistema de actualización en tiempo real (polling cada 5 segundos)
- Tres archivos base: NodoReceptor.cpp, index.html, muestra_nodos.html

### Características
- Visualización de luminarias en mapa
- Estados: encendida, apagada, falla
- Actualización de estado via POST
- Coordenadas GPS de San Luis, Argentina

## Convenciones de Versionado

### Versión Mayor (X.0.0)
- Cambios incompatibles con versiones anteriores
- Rediseño completo de arquitectura
- Cambio de tecnologías base

### Versión Menor (0.X.0)
- Nueva funcionalidad compatible con versiones anteriores
- Mejoras significativas
- Nuevos endpoints API

### Versión Parche (0.0.X)
- Corrección de bugs
- Mejoras menores de rendimiento
- Actualizaciones de documentación

## Enlaces
- [Repositorio](https://github.com/tu-usuario/Control-de-Luces)
- [Reportar Bug](https://github.com/tu-usuario/Control-de-Luces/issues)
- [Solicitar Feature](https://github.com/tu-usuario/Control-de-Luces/issues)