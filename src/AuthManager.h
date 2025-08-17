#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <Crypto.h>
#include <SHA256.h>
#include "config.h"
#include "Logger.h"
#include <map>

// Configuración de autenticación
#define MAX_SESSIONS 10
#define SESSION_TIMEOUT 1800000  // 30 minutos
#define MAX_LOGIN_ATTEMPTS 5
#define LOGIN_BLOCK_TIME 300000  // 5 minutos de bloqueo
#define TOKEN_LENGTH 32

// Niveles de acceso
enum UserRole {
    ROLE_NONE = 0,
    ROLE_VIEWER = 1,     // Solo lectura
    ROLE_OPERATOR = 2,   // Puede controlar luces
    ROLE_ADMIN = 3       // Acceso total
};

// Estructura de usuario
struct User {
    String username;
    String passwordHash;
    UserRole role;
    bool active;
    uint32_t lastLogin;
    uint8_t failedAttempts;
    uint32_t blockedUntil;
};

// Estructura de sesión
struct Session {
    String token;
    String username;
    String ip;
    UserRole role;
    uint32_t createdAt;
    uint32_t lastActivity;
    bool valid;
};

class AuthManager {
private:
    std::map<String, User> users;
    std::map<String, Session> sessions;
    uint32_t sessionCounter = 0;
    
    // Funciones internas
    String hashPassword(const String& password);
    String generateToken();
    bool isSessionValid(const String& token);
    void cleanExpiredSessions();
    void loadUsersFromFile();
    void saveUsersToFile();
    
public:
    AuthManager();
    
    // Inicialización
    bool begin();
    
    // Gestión de usuarios
    bool addUser(const String& username, const String& password, UserRole role);
    bool removeUser(const String& username);
    bool updateUserPassword(const String& username, const String& newPassword);
    bool updateUserRole(const String& username, UserRole newRole);
    bool userExists(const String& username);
    
    // Autenticación
    String login(const String& username, const String& password, const String& ip);
    bool logout(const String& token);
    bool validateToken(const String& token);
    
    // Autorización
    UserRole getUserRole(const String& token);
    bool hasPermission(const String& token, UserRole requiredRole);
    String getCurrentUser(const String& token);
    
    // Gestión de sesiones
    void updateSessionActivity(const String& token);
    uint32_t getActiveSessionCount();
    String getSessionInfo(const String& token);
    String getAllSessions();
    void invalidateAllSessions();
    void invalidateUserSessions(const String& username);
    
    // Seguridad
    bool isUserBlocked(const String& username);
    void resetFailedAttempts(const String& username);
    void recordFailedAttempt(const String& username);
    
    // Estadísticas
    String getUserStats();
    String getAuthStats();
    
    // Utilidades
    void checkSessions();  // Llamar periódicamente para limpiar sesiones expiradas
};

// Instancia global
extern AuthManager Auth;

// Middleware para proteger rutas
#define REQUIRE_AUTH(request, role) \
    { \
        if (!request->hasHeader("Authorization")) { \
            request->send(401, "application/json", "{\"error\":\"No autorizado\"}"); \
            return; \
        } \
        String token = request->header("Authorization"); \
        token.replace("Bearer ", ""); \
        if (!Auth.hasPermission(token, role)) { \
            request->send(403, "application/json", "{\"error\":\"Permisos insuficientes\"}"); \
            return; \
        } \
        Auth.updateSessionActivity(token); \
    }

#endif // AUTH_MANAGER_H