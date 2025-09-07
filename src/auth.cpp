#include "auth.h"
#include "config.h"
#include <mbedtls/md5.h>
#include <mbedtls/sha256.h>

// External functions
extern uint16_t calculateChecksum(const void* data, size_t length);

// Session storage
static AuthSession sessions[MAX_CONCURRENT_SESSIONS];
static uint8_t session_count = 0;

// Account lockout tracking
static unsigned long failed_attempts_time[MAX_FAILED_ATTEMPTS];
static uint8_t failed_attempt_count = 0;
static bool account_locked = false;
static unsigned long lockout_start_time = 0;

// Security settings
static uint32_t session_timeout = DEFAULT_SESSION_TIMEOUT;
static uint8_t max_login_attempts = DEFAULT_MAX_LOGIN_ATTEMPTS;
static uint32_t account_lockout_duration = DEFAULT_LOCKOUT_DURATION;

bool initializeAuthSystem() {
    DEBUG_PRINTLN("Initializing authentication system...");
    
    // Clear session storage
    memset(sessions, 0, sizeof(sessions));
    session_count = 0;
    
    // Clear failed attempts
    memset(failed_attempts_time, 0, sizeof(failed_attempts_time));
    failed_attempt_count = 0;
    account_locked = false;
    
    DEBUG_PRINTLN("Authentication system initialized");
    return true;
}

AuthResult authenticateUser(const char* username, const char* password, IPAddress client_ip, const char* user_agent) {
    if (!username || !password) {
        return AUTH_INVALID_CREDENTIALS;
    }
    
    DEBUG_PRINTF("Authentication attempt for user: %s from IP: %s\n", username, client_ip.toString().c_str());
    
    // Check if account is locked
    if (isAccountLocked(username)) {
        DEBUG_PRINTLN("Authentication failed: Account is locked");
        recordFailedLogin(username, client_ip);
        return AUTH_ACCOUNT_LOCKED;
    }
    
    // Validate credentials against stored configuration
    if (strcmp(username, g_system_config.user.username) != 0) {
        DEBUG_PRINTLN("Authentication failed: Invalid username");
        recordFailedLogin(username, client_ip);
        return AUTH_INVALID_CREDENTIALS;
    }
    
    // Verify password hash
    String password_hash = hashPassword(password);
    if (password_hash != String(g_system_config.user.password_hash)) {
        DEBUG_PRINTLN("Authentication failed: Invalid password");
        recordFailedLogin(username, client_ip);
        return AUTH_INVALID_CREDENTIALS;
    }
    
    // Check password strength for new sessions
    if (!isPasswordSecure(password)) {
        DEBUG_PRINTLN("Authentication failed: Weak password");
        return AUTH_WEAK_PASSWORD;
    }
    
    // Authentication successful
    DEBUG_PRINTLN("Authentication successful");
    recordSuccessfulLogin(username, client_ip);
    resetFailedLoginAttempts(username);
    
    return AUTH_SUCCESS;
}

AuthResult changePassword(const char* username, const char* old_password, const char* new_password) {
    if (!username || !old_password || !new_password) {
        return AUTH_INVALID_CREDENTIALS;
    }
    
    DEBUG_PRINTF("Password change request for user: %s\n", username);
    
    // Verify current credentials
    if (authenticateUser(username, old_password, IPAddress(127, 0, 0, 1), "internal") != AUTH_SUCCESS) {
        return AUTH_INVALID_CREDENTIALS;
    }
    
    // Check new password strength
    if (!isPasswordSecure(new_password)) {
        DEBUG_PRINTLN("Password change failed: New password is too weak");
        return AUTH_WEAK_PASSWORD;
    }
    
    // Generate new password hash
    String new_hash = hashPassword(new_password);
    
    // Update configuration
    strncpy(g_system_config.user.password_hash, new_hash.c_str(), sizeof(g_system_config.user.password_hash) - 1);
    g_system_config.user.password_hash[sizeof(g_system_config.user.password_hash) - 1] = '\0';
    
    // Update checksum
    g_system_config.user.checksum = calculateChecksum(&g_system_config.user, sizeof(UserCredentials) - sizeof(uint16_t));
    
    // Destroy all existing sessions
    destroyAllSessions();
    
    DEBUG_PRINTLN("Password changed successfully");
    logSecurityEvent("PASSWORD_CHANGED", username, IPAddress(127, 0, 0, 1));
    
    return AUTH_SUCCESS;
}

bool isAccountLocked(const char* username) {
    // Check if lockout period has expired
    if (account_locked && (millis() - lockout_start_time) > (account_lockout_duration * 1000)) {
        account_locked = false;
        failed_attempt_count = 0;
        DEBUG_PRINTLN("Account lockout expired");
    }
    
    return account_locked;
}

void unlockAccount(const char* username) {
    account_locked = false;
    failed_attempt_count = 0;
    memset(failed_attempts_time, 0, sizeof(failed_attempts_time));
    
    DEBUG_PRINTF("Account unlocked: %s\n", username);
    logSecurityEvent("ACCOUNT_UNLOCKED", username, IPAddress(127, 0, 0, 1));
}

void lockAccount(const char* username) {
    account_locked = true;
    lockout_start_time = millis();
    
    DEBUG_PRINTF("Account locked: %s\n", username);
    logSecurityEvent("ACCOUNT_LOCKED", username, IPAddress(127, 0, 0, 1));
}

String createSession(uint32_t user_id, IPAddress client_ip, const char* user_agent) {
    // Clean up expired sessions first
    cleanupExpiredSessions();
    
    // Check if we have space for new session
    if (session_count >= MAX_CONCURRENT_SESSIONS) {
        DEBUG_PRINTLN("Session creation failed: Maximum sessions reached");
        return "";
    }
    
    // Generate session ID
    String session_id = generateSessionId();
    
    // Find free session slot
    for (int i = 0; i < MAX_CONCURRENT_SESSIONS; i++) {
        if (!sessions[i].active) {
            // Initialize session
            strncpy(sessions[i].session_id, session_id.c_str(), sizeof(sessions[i].session_id) - 1);
            sessions[i].session_id[sizeof(sessions[i].session_id) - 1] = '\0';
            sessions[i].user_id = user_id;
            sessions[i].created_at = getCurrentTimestamp();
            sessions[i].last_activity = getCurrentTimestamp();
            sessions[i].expires_at = getCurrentTimestamp() + session_timeout;
            sessions[i].active = true;
            sessions[i].client_ip = client_ip;
            
            if (user_agent) {
                strncpy(sessions[i].user_agent, user_agent, sizeof(sessions[i].user_agent) - 1);
                sessions[i].user_agent[sizeof(sessions[i].user_agent) - 1] = '\0';
            }
            
            session_count++;
            
            DEBUG_PRINTF("Session created: %s for user %u from %s\n", 
                        session_id.c_str(), (unsigned int)user_id, client_ip.toString().c_str());
            
            return session_id;
        }
    }
    
    return "";
}

AuthResult validateSession(const char* session_id) {
    if (!session_id) {
        return AUTH_INVALID_SESSION;
    }
    
    AuthSession* session = getSession(session_id);
    if (!session) {
        return AUTH_INVALID_SESSION;
    }
    
    uint32_t current_time = getCurrentTimestamp();
    
    // Check if session has expired
    if (current_time > session->expires_at) {
        DEBUG_PRINTF("Session expired: %s\n", session_id);
        destroySession(session_id);
        return AUTH_SESSION_EXPIRED;
    }
    
    // Update last activity
    updateSessionActivity(session_id);
    
    return AUTH_SUCCESS;
}

bool isSessionValid(const char* session_id) {
    return validateSession(session_id) == AUTH_SUCCESS;
}

void destroySession(const char* session_id) {
    if (!session_id) return;
    
    for (int i = 0; i < MAX_CONCURRENT_SESSIONS; i++) {
        if (sessions[i].active && strcmp(sessions[i].session_id, session_id) == 0) {
            memset(&sessions[i], 0, sizeof(AuthSession));
            session_count--;
            
            DEBUG_PRINTF("Session destroyed: %s\n", session_id);
            break;
        }
    }
}

void destroyAllSessions() {
    memset(sessions, 0, sizeof(sessions));
    session_count = 0;
    
    DEBUG_PRINTLN("All sessions destroyed");
    logSecurityEvent("ALL_SESSIONS_DESTROYED", "system", IPAddress(127, 0, 0, 1));
}

void cleanupExpiredSessions() {
    uint32_t current_time = getCurrentTimestamp();
    uint8_t cleaned = 0;
    
    for (int i = 0; i < MAX_CONCURRENT_SESSIONS; i++) {
        if (sessions[i].active && current_time > sessions[i].expires_at) {
            memset(&sessions[i], 0, sizeof(AuthSession));
            session_count--;
            cleaned++;
        }
    }
    
    if (cleaned > 0) {
        DEBUG_PRINTF("Cleaned up %d expired sessions\n", cleaned);
    }
}

AuthSession* getSession(const char* session_id) {
    if (!session_id) return nullptr;
    
    for (int i = 0; i < MAX_CONCURRENT_SESSIONS; i++) {
        if (sessions[i].active && strcmp(sessions[i].session_id, session_id) == 0) {
            return &sessions[i];
        }
    }
    
    return nullptr;
}

void updateSessionActivity(const char* session_id) {
    AuthSession* session = getSession(session_id);
    if (session) {
        uint32_t current_time = getCurrentTimestamp();
        session->last_activity = current_time;
        session->expires_at = current_time + session_timeout;
    }
}

// Password security functions
bool isPasswordSecure(const char* password) {
    if (!password) return false;
    
    size_t len = strlen(password);
    
    // Check minimum length
    if (len < MIN_PASSWORD_LENGTH) {
        return false;
    }
    
    // Check maximum length
    if (len > MAX_PASSWORD_LENGTH) {
        return false;
    }
    
    // Check for at least one digit and one letter
    bool has_digit = false;
    bool has_letter = false;
    
    for (size_t i = 0; i < len; i++) {
        if (isdigit(password[i])) has_digit = true;
        if (isalpha(password[i])) has_letter = true;
    }
    
    return has_digit && has_letter;
}

String hashPassword(const char* password, const char* salt) {
    if (!password) return "";
    
    String salt_str = salt ? String(salt) : "macsys_salt_2024";
    String to_hash = String(password) + salt_str;
    
    // Use SHA-256 for password hashing
    uint8_t hash[32];
    mbedtls_sha256_context sha256_ctx;
    
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);
    mbedtls_sha256_update(&sha256_ctx, (const unsigned char*)to_hash.c_str(), to_hash.length());
    mbedtls_sha256_finish(&sha256_ctx, hash);
    mbedtls_sha256_free(&sha256_ctx);
    
    // Convert to hex string
    String hash_str = "";
    for (int i = 0; i < 32; i++) {
        char hex[3];
        sprintf(hex, "%02x", hash[i]);
        hash_str += hex;
    }
    
    return hash_str;
}

String generateSalt() {
    // Generate random salt using ESP32 hardware RNG
    String salt = "";
    for (int i = 0; i < 16; i++) {
        char hex[3];
        sprintf(hex, "%02x", (unsigned int)(esp_random() & 0xFF));
        salt += hex;
    }
    return salt;
}

bool verifyPassword(const char* password, const char* hash) {
    if (!password || !hash) return false;
    
    String computed_hash = hashPassword(password);
    return computed_hash.equals(String(hash));
}

String generateSessionId() {
    // Generate cryptographically secure session ID
    String session_id = "";
    for (int i = 0; i < 16; i++) {
        char hex[3];
        sprintf(hex, "%02x", (unsigned int)(esp_random() & 0xFF));
        session_id += hex;
    }
    return session_id;
}

// Security monitoring functions
void recordFailedLogin(const char* username, IPAddress client_ip) {
    unsigned long current_time = millis();
    
    // Shift failed attempts array
    for (int i = MAX_FAILED_ATTEMPTS - 1; i > 0; i--) {
        failed_attempts_time[i] = failed_attempts_time[i-1];
    }
    failed_attempts_time[0] = current_time;
    
    failed_attempt_count++;
    
    DEBUG_PRINTF("Failed login recorded for %s from %s (count: %d)\n", 
                 username, client_ip.toString().c_str(), failed_attempt_count);
    
    // Check if account should be locked
    if (failed_attempt_count >= max_login_attempts) {
        lockAccount(username);
    }
    
    logSecurityEvent("FAILED_LOGIN", username, client_ip);
}

void recordSuccessfulLogin(const char* username, IPAddress client_ip) {
    g_system_config.user.last_login = getCurrentTimestamp();
    
    DEBUG_PRINTF("Successful login recorded for %s from %s\n", 
                 username, client_ip.toString().c_str());
    
    logSecurityEvent("SUCCESSFUL_LOGIN", username, client_ip);
}

int getFailedLoginAttempts(const char* username) {
    return failed_attempt_count;
}

void resetFailedLoginAttempts(const char* username) {
    failed_attempt_count = 0;
    memset(failed_attempts_time, 0, sizeof(failed_attempts_time));
    
    DEBUG_PRINTF("Failed login attempts reset for %s\n", username);
}

// System security functions
void enforceSecurityPolicies() {
    // Clean up expired sessions
    cleanupExpiredSessions();
    
    // Check for suspicious activity patterns
    // This is a placeholder for more advanced security monitoring
}

bool isClientBlocked(IPAddress client_ip) {
    // Placeholder for IP-based blocking
    // In a full implementation, this would check against a blocked IP list
    return false;
}

void blockClient(IPAddress client_ip, uint32_t duration_seconds) {
    // Placeholder for IP blocking
    DEBUG_PRINTF("Client blocked: %s for %d seconds\n", 
                 client_ip.toString().c_str(), duration_seconds);
    
    logSecurityEvent("CLIENT_BLOCKED", "system", client_ip);
}

void unblockClient(IPAddress client_ip) {
    // Placeholder for IP unblocking
    DEBUG_PRINTF("Client unblocked: %s\n", client_ip.toString().c_str());
    
    logSecurityEvent("CLIENT_UNBLOCKED", "system", client_ip);
}

// Configuration functions
void setSessionTimeout(uint32_t timeout_seconds) {
    session_timeout = timeout_seconds;
    DEBUG_PRINTF("Session timeout set to %d seconds\n", timeout_seconds);
}

uint32_t getSessionTimeout() {
    return session_timeout;
}

void setMaxLoginAttempts(uint8_t max_attempts) {
    max_login_attempts = max_attempts;
    DEBUG_PRINTF("Max login attempts set to %d\n", max_attempts);
}

uint8_t getMaxLoginAttempts() {
    return max_login_attempts;
}

void setAccountLockoutDuration(uint32_t duration_seconds) {
    account_lockout_duration = duration_seconds;
    DEBUG_PRINTF("Account lockout duration set to %d seconds\n", duration_seconds);
}

uint32_t getAccountLockoutDuration() {
    return account_lockout_duration;
}

// Utility functions
uint32_t getCurrentTimestamp() {
    return millis() / 1000; // Convert to seconds
}

String formatTimestamp(uint32_t timestamp) {
    // Simple timestamp formatting
    uint32_t hours = (timestamp / 3600) % 24;
    uint32_t minutes = (timestamp / 60) % 60;
    uint32_t seconds = timestamp % 60;
    
    char time_str[16];
    sprintf(time_str, "%02d:%02d:%02d", (int)hours, (int)minutes, (int)seconds);
    
    return String(time_str);
}

void logSecurityEvent(const char* event, const char* details, IPAddress client_ip) {
    DEBUG_PRINTF("[SECURITY] %s: %s from %s at %s\n", 
                 event, details, client_ip.toString().c_str(), 
                 formatTimestamp(getCurrentTimestamp()).c_str());
    
    // In a full implementation, this would also log to persistent storage
}