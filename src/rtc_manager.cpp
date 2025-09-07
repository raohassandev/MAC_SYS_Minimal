#include "rtc_manager.h"
#include "config.h"

// Global RTC manager instance
RTCManager rtc_manager;

RTCManager::RTCManager() {
    rtc_available = false;
    ntp_synced = false;
    last_ntp_sync = 0;
    last_rtc_sync = 0;
    timezone_name = "PKT";
    timezone_offset = TIMEZONE_OFFSET;
}

bool RTCManager::begin() {
    DEBUG_PRINTLN("Initializing RTC Manager...");
    
    // Initialize RTC
    if (rtc.begin()) {
        rtc_available = true;
        DEBUG_PRINTLN("✅ DS1307 RTC found");
        
        // Check if RTC is running
        if (!rtc.isrunning()) {
            DEBUG_PRINTLN("⚠️ RTC is not running, starting it...");
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        
        // Display current RTC time
        DateTime now = rtc.now();
        DEBUG_PRINTF("📅 RTC Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                     now.year(), now.month(), now.day(),
                     now.hour(), now.minute(), now.second());
    } else {
        DEBUG_PRINTLN("❌ DS1307 RTC not found");
        rtc_available = false;
    }
    
    return rtc_available;
}

bool RTCManager::syncWithNTP() {
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("❌ WiFi not connected, cannot sync NTP");
        return false;
    }
    
    DEBUG_PRINTLN("🌐 Synchronizing with NTP servers...");
    
    // Configure NTP
    configTime(timezone_offset, DST_OFFSET, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
    
    // Wait for time sync (up to 10 seconds)
    int timeout = 100; // 10 seconds (100 * 100ms)
    while (timeout > 0) {
        time_t now = time(nullptr);
        if (now > 1000000000) { // Valid timestamp (after 2001)
            struct tm* timeinfo = localtime(&now);
            DEBUG_PRINTF("✅ NTP synchronized: %04d-%02d-%02d %02d:%02d:%02d %s\n",
                        timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                        timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, timezone_name);
            
            ntp_synced = true;
            last_ntp_sync = millis();
            
            // Sync RTC with NTP time if RTC is available
            if (rtc_available) {
                syncRTCWithNTP();
            }
            
            return true;
        }
        delay(100);
        timeout--;
    }
    
    DEBUG_PRINTLN("❌ NTP synchronization failed");
    return false;
}

bool RTCManager::syncRTCWithNTP() {
    if (!rtc_available || !ntp_synced) {
        return false;
    }
    
    time_t now = time(nullptr);
    if (now < 1000000000) { // Invalid timestamp
        return false;
    }
    
    struct tm* timeinfo = localtime(&now);
    DateTime ntp_time(timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                      timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    
    rtc.adjust(ntp_time);
    last_rtc_sync = millis();
    
    DEBUG_PRINTLN("✅ RTC synchronized with NTP time");
    return true;
}

void RTCManager::updateTime() {
    unsigned long now = millis();
    
    // Check if NTP sync is needed
    if (WiFi.status() == WL_CONNECTED && 
        (now - last_ntp_sync) > NTP_SYNC_INTERVAL) {
        syncWithNTP();
    }
    
    // Check if RTC sync is needed
    if (rtc_available && ntp_synced && 
        (now - last_rtc_sync) > RTC_SYNC_INTERVAL) {
        syncRTCWithNTP();
    }
}

DateTime RTCManager::getCurrentTime() {
    if (rtc_available) {
        return rtc.now();
    } else if (ntp_synced) {
        time_t now = time(nullptr);
        struct tm* timeinfo = localtime(&now);
        return DateTime(timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                       timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    } else {
        // Return compile time as fallback
        return DateTime(F(__DATE__), F(__TIME__));
    }
}

time_t RTCManager::getUnixTime() {
    if (ntp_synced) {
        return time(nullptr);
    } else if (rtc_available) {
        return rtc.now().unixtime();
    } else {
        return 0;
    }
}

String RTCManager::getFormattedTime(bool include_seconds) {
    DateTime now = getCurrentTime();
    String time_str = "";
    
    if (now.hour() < 10) time_str += "0";
    time_str += String(now.hour()) + ":";
    
    if (now.minute() < 10) time_str += "0";
    time_str += String(now.minute());
    
    if (include_seconds) {
        time_str += ":";
        if (now.second() < 10) time_str += "0";
        time_str += String(now.second());
    }
    
    return time_str;
}

String RTCManager::getFormattedDate() {
    DateTime now = getCurrentTime();
    String date_str = "";
    
    date_str += String(now.year()) + "-";
    
    if (now.month() < 10) date_str += "0";
    date_str += String(now.month()) + "-";
    
    if (now.day() < 10) date_str += "0";
    date_str += String(now.day());
    
    return date_str;
}

String RTCManager::getFormattedDateTime() {
    return getFormattedDate() + " " + getFormattedTime();
}

int RTCManager::getCurrentDayOfWeek() {
    DateTime now = getCurrentTime();
    return now.dayOfTheWeek(); // 0=Sunday, 1=Monday, etc.
}

int RTCManager::getCurrentHour() {
    DateTime now = getCurrentTime();
    return now.hour(); // 0-23
}

int RTCManager::getCurrentMinute() {
    DateTime now = getCurrentTime();
    return now.minute(); // 0-59
}

int RTCManager::getCurrentTimeMinutes() {
    DateTime now = getCurrentTime();
    return (now.hour() * 60) + now.minute(); // Minutes since midnight
}

bool RTCManager::isValidTime(const DateTime& dt) {
    return (dt.year() >= 2020 && dt.year() <= 2099 &&
            dt.month() >= 1 && dt.month() <= 12 &&
            dt.day() >= 1 && dt.day() <= 31 &&
            dt.hour() >= 0 && dt.hour() <= 23 &&
            dt.minute() >= 0 && dt.minute() <= 59 &&
            dt.second() >= 0 && dt.second() <= 59);
}

DateTime RTCManager::parseTimeString(const String& timeStr) {
    // Parse "HH:MM" format
    int colonPos = timeStr.indexOf(':');
    if (colonPos == -1 || timeStr.length() != 5) {
        return DateTime(); // Invalid format
    }
    
    int hour = timeStr.substring(0, colonPos).toInt();
    int minute = timeStr.substring(colonPos + 1).toInt();
    
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return DateTime(); // Invalid values
    }
    
    DateTime now = getCurrentTime();
    return DateTime(now.year(), now.month(), now.day(), hour, minute, 0);
}

String RTCManager::timeToString(const DateTime& dt) {
    String time_str = "";
    
    if (dt.hour() < 10) time_str += "0";
    time_str += String(dt.hour()) + ":";
    
    if (dt.minute() < 10) time_str += "0";
    time_str += String(dt.minute());
    
    return time_str;
}

int RTCManager::timeToMinutes(int hour, int minute) {
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return -1; // Invalid time
    }
    return (hour * 60) + minute;
}

void RTCManager::minutesToTime(int minutes, int& hour, int& minute) {
    if (minutes < 0 || minutes >= 1440) { // 1440 minutes in a day
        hour = 0;
        minute = 0;
        return;
    }
    
    hour = minutes / 60;
    minute = minutes % 60;
}

String RTCManager::getStatusString() {
    String status = "RTC Status:\n";
    
    if (rtc_available) {
        status += "✅ DS1307 RTC: Available\n";
        DateTime now = rtc.now();
        status += "📅 RTC Time: " + getFormattedDateTime() + "\n";
    } else {
        status += "❌ DS1307 RTC: Not Available\n";
    }
    
    if (ntp_synced) {
        status += "🌐 NTP Sync: Active\n";
        unsigned long minutes_ago = (millis() - last_ntp_sync) / 60000;
        status += "⏰ Last NTP Sync: " + String(minutes_ago) + " minutes ago\n";
    } else {
        status += "❌ NTP Sync: Not Active\n";
    }
    
    status += "🕒 Current Time: " + getFormattedDateTime() + "\n";
    status += "🌍 Timezone: " + String(timezone_name) + " (GMT+" + String(timezone_offset/3600) + ")\n";
    
    return status;
}

bool RTCManager::needsSync() {
    unsigned long now = millis();
    
    // Need NTP sync if never synced or interval exceeded
    if (!ntp_synced || (now - last_ntp_sync) > NTP_SYNC_INTERVAL) {
        return true;
    }
    
    // Need RTC sync if RTC available but not synced recently
    if (rtc_available && (now - last_rtc_sync) > RTC_SYNC_INTERVAL) {
        return true;
    }
    
    return false;
}