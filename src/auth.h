#ifndef AUTH_H
#define AUTH_H

#include <Arduino.h>

// Authentication result codes
enum AuthResult {
    AUTH_SUCCESS = 0,
    AUTH_INVALID_CREDENTIALS = 1,
    AUTH_ACCOUNT_LOCKED = 2,
    AUTH_SESSION_EXPIRED = 3,
    AUTH_INVALID_SESSION = 4,
    AUTH_WEAK_PASSWORD = 5,
    AUTH_ERROR = 6
};

// Session structure
struct AuthSession {
    char session_id[33];      // 32-character hex string + null terminator
    uint32_t user_id;
    uint32_t created_at;
    uint32_t last_activity;
    uint32_t expires_at;
    bool active;
    IPAddress client_ip;
    char user_agent[64];
};

// Authentication system initialization
bool initializeAuthSystem();

// User authentication functions
AuthResult authenticateUser(const char* username, const char* password, IPAddress client_ip, const char* user_agent = "");
AuthResult changePassword(const char* username, const char* old_password, const char* new_password);
bool isAccountLocked(const char* username);
void unlockAccount(const char* username);
void lockAccount(const char* username);

// Session management functions
String createSession(uint32_t user_id, IPAddress client_ip, const char* user_agent = "");
AuthResult validateSession(const char* session_id);
bool isSessionValid(const char* session_id);
void destroySession(const char* session_id);
void destroyAllSessions();
void cleanupExpiredSessions();
AuthSession* getSession(const char* session_id);
void updateSessionActivity(const char* session_id);

// Password security functions
bool isPasswordSecure(const char* password);
String hashPassword(const char* password, const char* salt = "");
String generateSalt();
bool verifyPassword(const char* password, const char* hash);
String generateSessionId();

// Security monitoring
void recordFailedLogin(const char* username, IPAddress client_ip);
void recordSuccessfulLogin(const char* username, IPAddress client_ip);
int getFailedLoginAttempts(const char* username);
void resetFailedLoginAttempts(const char* username);

// System security functions
void enforceSecurityPolicies();
bool isClientBlocked(IPAddress client_ip);
void blockClient(IPAddress client_ip, uint32_t duration_seconds = 3600);
void unblockClient(IPAddress client_ip);

// Configuration functions
void setSessionTimeout(uint32_t timeout_seconds);
uint32_t getSessionTimeout();
void setMaxLoginAttempts(uint8_t max_attempts);
uint8_t getMaxLoginAttempts();
void setAccountLockoutDuration(uint32_t duration_seconds);
uint32_t getAccountLockoutDuration();

// Utility functions
uint32_t getCurrentTimestamp();
String formatTimestamp(uint32_t timestamp);
void logSecurityEvent(const char* event, const char* details, IPAddress client_ip);

#endif // AUTH_H