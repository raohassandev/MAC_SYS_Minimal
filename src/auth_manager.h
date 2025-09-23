#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include "config.h"
#include <EEPROM.h>
#include <WebServer.h>

// Global session variable
extern SimpleUserSession g_current_session;

// Simple authentication functions (different from existing auth.h)
void initializeSimpleAuth();
bool authenticateSimpleUser(const char* username, const char* password, UserRole& role);
bool isAuthenticated();
bool isAdmin();
bool isUser();
void loginUser(const char* username, UserRole role);
void logoutUser();
void setupAuthAPI(WebServer* server);

// Default credentials - stored in existing UserCredentials structure
#define DEFAULT_ADMIN_USER "admin"
#define DEFAULT_ADMIN_PASS "admin123"
#define DEFAULT_USER_USER "user"
#define DEFAULT_USER_PASS "user123"

#endif // AUTH_MANAGER_H