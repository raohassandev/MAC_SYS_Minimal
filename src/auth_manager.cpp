#include "auth_manager.h"
#include "utils.h"
#include <ArduinoJson.h>

// Global session
SimpleUserSession g_current_session = {false, ROLE_USER, "", 0};

void initializeSimpleAuth() {
    DEBUG_PRINTLN("Initializing simple authentication system...");
    
    // Reset session on startup
    g_current_session.is_authenticated = false;
    g_current_session.role = ROLE_USER;
    strcpy(g_current_session.username, "");
    g_current_session.login_time = 0;
    
    DEBUG_PRINTLN("Simple authentication system initialized");
}

bool authenticateSimpleUser(const char* username, const char* password, UserRole& role) {
    // Simple hardcoded authentication for now
    // Check admin credentials
    if (strcmp(username, DEFAULT_ADMIN_USER) == 0 && 
        strcmp(password, DEFAULT_ADMIN_PASS) == 0) {
        role = ROLE_ADMIN;
        return true;
    }
    
    // Check user credentials
    if (strcmp(username, DEFAULT_USER_USER) == 0 && 
        strcmp(password, DEFAULT_USER_PASS) == 0) {
        role = ROLE_USER;
        return true;
    }
    
    return false;
}

bool isAuthenticated() {
    return g_current_session.is_authenticated;
}

bool isAdmin() {
    return g_current_session.is_authenticated && g_current_session.role == ROLE_ADMIN;
}

bool isUser() {
    return g_current_session.is_authenticated && g_current_session.role == ROLE_USER;
}

void loginUser(const char* username, UserRole role) {
    g_current_session.is_authenticated = true;
    g_current_session.role = role;
    strcpy(g_current_session.username, username);
    g_current_session.login_time = millis();
    
    DEBUG_PRINTF("User logged in: %s (role: %s)\n", username, 
                 role == ROLE_ADMIN ? "admin" : "user");
}

void logoutUser() {
    DEBUG_PRINTF("User logged out: %s\n", g_current_session.username);
    
    g_current_session.is_authenticated = false;
    g_current_session.role = ROLE_USER;
    strcpy(g_current_session.username, "");
    g_current_session.login_time = 0;
}

void setupAuthAPI(WebServer* server) {
    if (!server) return;
    
    // Login endpoint
    server->on("/api/auth/login", HTTP_POST, [server]() {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, server->arg("plain"));
        
        if (error) {
            server->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
            return;
        }
        
        const char* username = doc["username"];
        const char* password = doc["password"];
        
        if (!username || !password) {
            server->send(400, "application/json", "{\"success\":false,\"message\":\"Username and password required\"}");
            return;
        }
        
        UserRole role;
        if (authenticateSimpleUser(username, password, role)) {
            loginUser(username, role);
            
            StaticJsonDocument<256> response;
            response["success"] = true;
            response["role"] = (role == ROLE_ADMIN) ? "admin" : "user";
            response["username"] = username;
            response["message"] = "Login successful";
            
            String output;
            serializeJson(response, output);
            server->send(200, "application/json", output);
        } else {
            server->send(401, "application/json", "{\"success\":false,\"message\":\"Invalid credentials\"}");
        }
    });
    
    // Logout endpoint
    server->on("/api/auth/logout", HTTP_POST, [server]() {
        logoutUser();
        server->send(200, "application/json", "{\"success\":true,\"message\":\"Logged out successfully\"}");
    });
    
    // Check session endpoint
    server->on("/api/auth/session", HTTP_GET, [server]() {
        StaticJsonDocument<256> response;
        response["authenticated"] = g_current_session.is_authenticated;
        response["role"] = (g_current_session.role == ROLE_ADMIN) ? "admin" : "user";
        response["username"] = g_current_session.username;
        
        String output;
        serializeJson(response, output);
        server->send(200, "application/json", output);
    });
}