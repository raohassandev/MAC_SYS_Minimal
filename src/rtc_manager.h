#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>
#include <WiFi.h>
#include <time.h>

// Time synchronization settings
#define NTP_SERVER1 "pool.ntp.org"
#define NTP_SERVER2 "time.nist.gov" 
#define NTP_SERVER3 "time.google.com"
#define TIMEZONE_OFFSET 5 * 3600  // GMT+5 (Pakistan Standard Time)
#define DST_OFFSET 0              // No daylight saving

// Time sync intervals
#define NTP_SYNC_INTERVAL 3600000  // Sync every hour (ms)
#define RTC_SYNC_INTERVAL 86400000 // Daily RTC sync (ms)
// Upper bound for one NTP attempt to complete (non-blocking window)
#define NTP_ATTEMPT_TIMEOUT 15000   // 15 seconds

class RTCManager {
private:
    RTC_DS1307 rtc;
    bool rtc_available;
    bool ntp_synced;
    bool ntp_in_progress;          // true while waiting for SNTP to update time
    unsigned long last_ntp_sync;
    unsigned long last_rtc_sync;
    unsigned long ntp_request_time; // when current NTP attempt started
    
    // Time zone and formatting
    const char* timezone_name;
    int timezone_offset;
    
public:
    RTCManager();
    
    // Initialization
    bool begin();
    bool isRTCAvailable() { return rtc_available; }
    bool isNTPSynced() { return ntp_synced; }
    
    // Time synchronization
    bool syncWithNTP();             // kick off non-blocking NTP sync
    bool syncRTCWithNTP();
    void updateTime(); // Call regularly to maintain sync
    
    // Time retrieval
    DateTime getCurrentTime();
    time_t getUnixTime();
    String getFormattedTime(bool include_seconds = true);
    String getFormattedDate();
    String getFormattedDateTime();
    
    // Schedule helpers
    int getCurrentDayOfWeek();    // 0=Sunday, 1=Monday, etc.
    int getCurrentHour();         // 0-23
    int getCurrentMinute();       // 0-59
    int getCurrentTimeMinutes();  // Minutes since midnight (0-1439)
    
    // Time utilities
    bool isValidTime(const DateTime& dt);
    DateTime parseTimeString(const String& timeStr); // "HH:MM" format
    String timeToString(const DateTime& dt);
    int timeToMinutes(int hour, int minute); // Convert to minutes since midnight
    void minutesToTime(int minutes, int& hour, int& minute);
    
    // Status and diagnostics
    String getStatusString();
    bool needsSync();
    unsigned long getLastSyncTime() { return last_ntp_sync; }
};

// Global RTC manager instance
extern RTCManager rtc_manager;

#endif // RTC_MANAGER_H
