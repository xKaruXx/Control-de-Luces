#include "AuthManager.h"
#include <LittleFS.h>

AuthManager Auth;

AuthManager::AuthManager() {}

bool AuthManager::begin() {
    SystemLogger.info("Iniciando AuthManager", "AUTH");
    
    // Crear directorio de usuarios si no existe
    if (!LittleFS.exists("/users")) {
        LittleFS.mkdir("/users");
    }
    
    // Cargar usuarios desde archivo
    loadUsersFromFile();
    
    // Si no hay usuarios, crear admin por defecto
    if (users.empty()) {
        addUser("admin", "admin123", ROLE_ADMIN);
        addUser("operator", "oper123", ROLE_OPERATOR);
        addUser("viewer", "view123", ROLE_VIEWER);
        SystemLogger.warning("Usuarios por defecto creados. CAMBIAR CONTRASEÑAS!", "AUTH");
    }
    
    SystemLogger.info("AuthManager iniciado con " + String(users.size()) + " usuarios", "AUTH");
    return true;
}

String AuthManager::hashPassword(const String& password) {
    SHA256 sha;
    uint8_t hash[32];
    
    sha.reset();
    sha.update((const uint8_t*)password.c_str(), password.length());
    sha.finalize(hash, sizeof(hash));
    
    String hashStr = "";
    for (int i = 0; i < 32; i++) {
        if (hash[i] < 16) hashStr += "0";
        hashStr += String(hash[i], HEX);
    }
    
    return hashStr;
}

String AuthManager::generateToken() {
    String token = "";
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    
    for (int i = 0; i < TOKEN_LENGTH; i++) {
        token += chars[random(0, 62)];
    }
    
    return token + "_" + String(millis());
}

bool AuthManager::isSessionValid(const String& token) {
    if (!sessions.count(token)) return false;
    
    Session& session = sessions[token];
    uint32_t now = millis();
    
    // Verificar timeout
    if (now - session.lastActivity > SESSION_TIMEOUT) {
        session.valid = false;
        SystemLogger.info("Sesión expirada para: " + session.username, "AUTH");
        return false;
    }
    
    return session.valid;
}

void AuthManager::cleanExpiredSessions() {
    uint32_t now = millis();
    std::vector<String> toRemove;
    
    for (auto& pair : sessions) {
        if (!pair.second.valid || (now - pair.second.lastActivity > SESSION_TIMEOUT)) {
            toRemove.push_back(pair.first);
        }
    }
    
    for (const String& token : toRemove) {
        sessions.erase(token);
    }
    
    if (toRemove.size() > 0) {
        SystemLogger.debug("Limpiadas " + String(toRemove.size()) + " sesiones expiradas", "AUTH");
    }
}

void AuthManager::loadUsersFromFile() {
    if (!LittleFS.exists("/users/users.json")) {
        SystemLogger.info("No existe archivo de usuarios", "AUTH");
        return;
    }
    
    File file = LittleFS.open("/users/users.json", "r");
    if (!file) {
        SystemLogger.error("No se pudo abrir archivo de usuarios", "AUTH");
        return;
    }
    
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        SystemLogger.error("Error al parsear usuarios: " + String(error.c_str()), "AUTH");
        return;
    }
    
    users.clear();
    JsonArray usersArray = doc["users"];
    
    for (JsonObject userObj : usersArray) {
        User user;
        user.username = userObj["username"].as<String>();
        user.passwordHash = userObj["password"].as<String>();
        user.role = (UserRole)userObj["role"].as<int>();
        user.active = userObj["active"];
        user.lastLogin = 0;
        user.failedAttempts = 0;
        user.blockedUntil = 0;
        
        users[user.username] = user;
    }
    
    SystemLogger.info("Cargados " + String(users.size()) + " usuarios", "AUTH");
}

void AuthManager::saveUsersToFile() {
    StaticJsonDocument<2048> doc;
    JsonArray usersArray = doc.createNestedArray("users");
    
    for (const auto& pair : users) {
        const User& user = pair.second;
        JsonObject userObj = usersArray.createNestedObject();
        userObj["username"] = user.username;
        userObj["password"] = user.passwordHash;
        userObj["role"] = (int)user.role;
        userObj["active"] = user.active;
    }
    
    File file = LittleFS.open("/users/users.json", "w");
    if (!file) {
        SystemLogger.error("No se pudo guardar archivo de usuarios", "AUTH");
        return;
    }
    
    serializeJson(doc, file);
    file.close();
    
    SystemLogger.debug("Usuarios guardados en archivo", "AUTH");
}

bool AuthManager::addUser(const String& username, const String& password, UserRole role) {
    if (userExists(username)) {
        SystemLogger.warning("Usuario ya existe: " + username, "AUTH");
        return false;
    }
    
    User newUser;
    newUser.username = username;
    newUser.passwordHash = hashPassword(password);
    newUser.role = role;
    newUser.active = true;
    newUser.lastLogin = 0;
    newUser.failedAttempts = 0;
    newUser.blockedUntil = 0;
    
    users[username] = newUser;
    saveUsersToFile();
    
    SystemLogger.info("Usuario creado: " + username + " con rol " + String(role), "AUTH");
    return true;
}

bool AuthManager::removeUser(const String& username) {
    if (!userExists(username)) return false;
    
    // No permitir eliminar el último admin
    if (users[username].role == ROLE_ADMIN) {
        int adminCount = 0;
        for (const auto& pair : users) {
            if (pair.second.role == ROLE_ADMIN) adminCount++;
        }
        if (adminCount <= 1) {
            SystemLogger.error("No se puede eliminar el último administrador", "AUTH");
            return false;
        }
    }
    
    users.erase(username);
    invalidateUserSessions(username);
    saveUsersToFile();
    
    SystemLogger.info("Usuario eliminado: " + username, "AUTH");
    return true;
}

bool AuthManager::updateUserPassword(const String& username, const String& newPassword) {
    if (!userExists(username)) return false;
    
    users[username].passwordHash = hashPassword(newPassword);
    invalidateUserSessions(username);
    saveUsersToFile();
    
    SystemLogger.info("Contraseña actualizada para: " + username, "AUTH");
    return true;
}

bool AuthManager::updateUserRole(const String& username, UserRole newRole) {
    if (!userExists(username)) return false;
    
    users[username].role = newRole;
    invalidateUserSessions(username);
    saveUsersToFile();
    
    SystemLogger.info("Rol actualizado para " + username + ": " + String(newRole), "AUTH");
    return true;
}

bool AuthManager::userExists(const String& username) {
    return users.count(username) > 0;
}

String AuthManager::login(const String& username, const String& password, const String& ip) {
    // Limpiar sesiones expiradas
    cleanExpiredSessions();
    
    // Verificar si el usuario existe
    if (!userExists(username)) {
        SystemLogger.warning("Intento de login con usuario inexistente: " + username, "AUTH");
        return "";
    }
    
    User& user = users[username];
    
    // Verificar si está bloqueado
    if (isUserBlocked(username)) {
        SystemLogger.warning("Intento de login con usuario bloqueado: " + username, "AUTH");
        return "";
    }
    
    // Verificar contraseña
    String passwordHash = hashPassword(password);
    if (user.passwordHash != passwordHash) {
        recordFailedAttempt(username);
        SystemLogger.warning("Contraseña incorrecta para: " + username, "AUTH");
        return "";
    }
    
    // Verificar si el usuario está activo
    if (!user.active) {
        SystemLogger.warning("Intento de login con usuario inactivo: " + username, "AUTH");
        return "";
    }
    
    // Verificar límite de sesiones
    if (sessions.size() >= MAX_SESSIONS) {
        cleanExpiredSessions();
        if (sessions.size() >= MAX_SESSIONS) {
            SystemLogger.error("Límite de sesiones alcanzado", "AUTH");
            return "";
        }
    }
    
    // Crear nueva sesión
    String token = generateToken();
    Session newSession;
    newSession.token = token;
    newSession.username = username;
    newSession.ip = ip;
    newSession.role = user.role;
    newSession.createdAt = millis();
    newSession.lastActivity = millis();
    newSession.valid = true;
    
    sessions[token] = newSession;
    
    // Actualizar usuario
    user.lastLogin = millis();
    user.failedAttempts = 0;
    
    SystemLogger.info("Login exitoso: " + username + " desde " + ip, "AUTH");
    return token;
}

bool AuthManager::logout(const String& token) {
    if (!sessions.count(token)) return false;
    
    String username = sessions[token].username;
    sessions.erase(token);
    
    SystemLogger.info("Logout: " + username, "AUTH");
    return true;
}

bool AuthManager::validateToken(const String& token) {
    return isSessionValid(token);
}

UserRole AuthManager::getUserRole(const String& token) {
    if (!isSessionValid(token)) return ROLE_NONE;
    return sessions[token].role;
}

bool AuthManager::hasPermission(const String& token, UserRole requiredRole) {
    UserRole userRole = getUserRole(token);
    return userRole >= requiredRole;
}

String AuthManager::getCurrentUser(const String& token) {
    if (!isSessionValid(token)) return "";
    return sessions[token].username;
}

void AuthManager::updateSessionActivity(const String& token) {
    if (sessions.count(token)) {
        sessions[token].lastActivity = millis();
    }
}

uint32_t AuthManager::getActiveSessionCount() {
    cleanExpiredSessions();
    return sessions.size();
}

String AuthManager::getSessionInfo(const String& token) {
    if (!sessions.count(token)) return "{}";
    
    Session& session = sessions[token];
    StaticJsonDocument<256> doc;
    doc["username"] = session.username;
    doc["role"] = session.role;
    doc["ip"] = session.ip;
    doc["created"] = session.createdAt;
    doc["lastActivity"] = session.lastActivity;
    doc["valid"] = session.valid;
    
    String result;
    serializeJson(doc, result);
    return result;
}

String AuthManager::getAllSessions() {
    cleanExpiredSessions();
    
    StaticJsonDocument<1024> doc;
    JsonArray sessionsArray = doc.createNestedArray("sessions");
    
    for (const auto& pair : sessions) {
        const Session& session = pair.second;
        JsonObject sessionObj = sessionsArray.createNestedObject();
        sessionObj["token"] = pair.first.substring(0, 8) + "...";  // Mostrar solo parte del token
        sessionObj["username"] = session.username;
        sessionObj["role"] = session.role;
        sessionObj["ip"] = session.ip;
        sessionObj["created"] = session.createdAt;
        sessionObj["lastActivity"] = session.lastActivity;
    }
    
    doc["count"] = sessions.size();
    doc["max"] = MAX_SESSIONS;
    
    String result;
    serializeJson(doc, result);
    return result;
}

void AuthManager::invalidateAllSessions() {
    sessions.clear();
    SystemLogger.warning("Todas las sesiones invalidadas", "AUTH");
}

void AuthManager::invalidateUserSessions(const String& username) {
    std::vector<String> toRemove;
    
    for (const auto& pair : sessions) {
        if (pair.second.username == username) {
            toRemove.push_back(pair.first);
        }
    }
    
    for (const String& token : toRemove) {
        sessions.erase(token);
    }
    
    if (toRemove.size() > 0) {
        SystemLogger.info("Invalidadas " + String(toRemove.size()) + " sesiones de " + username, "AUTH");
    }
}

bool AuthManager::isUserBlocked(const String& username) {
    if (!userExists(username)) return false;
    
    User& user = users[username];
    if (user.blockedUntil > millis()) {
        return true;
    }
    
    return false;
}

void AuthManager::resetFailedAttempts(const String& username) {
    if (!userExists(username)) return;
    
    users[username].failedAttempts = 0;
    users[username].blockedUntil = 0;
}

void AuthManager::recordFailedAttempt(const String& username) {
    if (!userExists(username)) return;
    
    User& user = users[username];
    user.failedAttempts++;
    
    if (user.failedAttempts >= MAX_LOGIN_ATTEMPTS) {
        user.blockedUntil = millis() + LOGIN_BLOCK_TIME;
        SystemLogger.warning("Usuario bloqueado por múltiples intentos fallidos: " + username, "AUTH");
    }
}

String AuthManager::getUserStats() {
    StaticJsonDocument<512> doc;
    
    int adminCount = 0, operatorCount = 0, viewerCount = 0, activeCount = 0;
    
    for (const auto& pair : users) {
        const User& user = pair.second;
        if (user.active) activeCount++;
        
        switch (user.role) {
            case ROLE_ADMIN: adminCount++; break;
            case ROLE_OPERATOR: operatorCount++; break;
            case ROLE_VIEWER: viewerCount++; break;
            default: break;
        }
    }
    
    doc["total"] = users.size();
    doc["active"] = activeCount;
    doc["admins"] = adminCount;
    doc["operators"] = operatorCount;
    doc["viewers"] = viewerCount;
    
    String result;
    serializeJson(doc, result);
    return result;
}

String AuthManager::getAuthStats() {
    StaticJsonDocument<512> doc;
    
    doc["users"] = users.size();
    doc["sessions"] = sessions.size();
    doc["max_sessions"] = MAX_SESSIONS;
    doc["session_timeout"] = SESSION_TIMEOUT;
    
    JsonArray usersArray = doc.createNestedArray("user_list");
    for (const auto& pair : users) {
        JsonObject userObj = usersArray.createNestedObject();
        userObj["username"] = pair.first;
        userObj["role"] = pair.second.role;
        userObj["active"] = pair.second.active;
        userObj["blocked"] = isUserBlocked(pair.first);
    }
    
    String result;
    serializeJson(doc, result);
    return result;
}

void AuthManager::checkSessions() {
    cleanExpiredSessions();
}