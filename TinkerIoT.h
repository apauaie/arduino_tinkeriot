#ifndef TINKERIOT_H
#define TINKERIOT_H

// Board detection and compatibility layer
#if defined(ESP32)
  #include <WiFi.h>
  #define TINKERIOT_BOARD "ESP32"
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #define TINKERIOT_BOARD "ESP8266"
#elif defined(ARDUINO_SAMD_NANO_33_IOT)
  #include <WiFiNINA.h>
  #define TINKERIOT_BOARD "Nano33IoT"

#elif defined(ARDUINO_SAMD_MKRWIFI1010)
  #include <WiFiNINA.h>
  #define TINKERIOT_BOARD "MKRWiFi1010"
#else
  #error "Unsupported board! This library supports ESP32, ESP8266, Arduino Nano 33 IoT, and MKR WiFi 1010"
#endif


#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// ===== CROSS-PLATFORM MUTEX/CRITICAL SECTION =====
// Protects cloudPins[] array from race conditions during concurrent access

#if defined(ESP32)
    // ESP32: Use FreeRTOS portMUX_TYPE for thread-safe operations
    #define TINKERIOT_MUTEX_TYPE portMUX_TYPE
    #define TINKERIOT_MUTEX_INIT portMUX_INITIALIZER_UNLOCKED
    #define TINKERIOT_LOCK(mutex) portENTER_CRITICAL(&mutex)
    #define TINKERIOT_UNLOCK(mutex) portEXIT_CRITICAL(&mutex)
    #define TINKERIOT_HAS_FREERTOS 1

#elif defined(ESP8266)
    // ESP8266: Use interrupt disable/enable for critical sections
    #define TINKERIOT_MUTEX_TYPE uint8_t
    #define TINKERIOT_MUTEX_INIT 0
    #define TINKERIOT_LOCK(mutex) noInterrupts()
    #define TINKERIOT_UNLOCK(mutex) interrupts()
    #define TINKERIOT_HAS_FREERTOS 0

#elif defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_SAMD_MKRWIFI1010)
    // Arduino SAMD (Nano IoT 33, MKR WiFi 1010): Use interrupt disable/enable
    #define TINKERIOT_MUTEX_TYPE uint8_t
    #define TINKERIOT_MUTEX_INIT 0
    #define TINKERIOT_LOCK(mutex) noInterrupts()
    #define TINKERIOT_UNLOCK(mutex) interrupts()
    #define TINKERIOT_HAS_FREERTOS 0

#else
    // Generic Arduino: Use interrupt disable/enable as fallback
    #define TINKERIOT_MUTEX_TYPE uint8_t
    #define TINKERIOT_MUTEX_INIT 0
    #define TINKERIOT_LOCK(mutex) noInterrupts()
    #define TINKERIOT_UNLOCK(mutex) interrupts()
    #define TINKERIOT_HAS_FREERTOS 0
#endif

// Debug output control - enable/disable separately for connection vs data
#define TINKERIOT_PRINT Serial          // Connection debug (WiFi, WebSocket, Login)
#define TINKERIOT_DATA_DEBUG Serial     // Data transmission debug (cloudWrite, messages, protocol)

// Usage examples:
// Production: Comment out TINKERIOT_DATA_DEBUG, keep TINKERIOT_PRINT
// Development: Enable both for full debugging
// Silent: Comment out both for no debug output

// ===== CLOUD PIN CONSTANTS =====
#define C0  0   
#define C1  1  
#define C2  2   
#define C3  3   
#define C4  4
#define C5  5   
#define C6  6   
#define C7  7   
#define C8  8   
#define C9  9
#define C10 10  
#define C11 11  
#define C12 12  
#define C13 13  
#define C14 14
#define C15 15  
#define C16 16  
#define C17 17  
#define C18 18  
#define C19 19
#define C20 20  
#define C21 21  
#define C22 22  
#define C23 23  
#define C24 24
#define C25 25  
#define C26 26 
#define C27 27  
#define C28 28  
#define C29 29
#define C30 30  
#define C31 31

// TinkerIoT Protocol Constants
enum TinkerIoTCmd {
    RESPONSE = 0,
    REGISTER = 1,
    LOGIN = 2,
    SAVE_PROF = 3,
    LOAD_PROF = 4,
    GET_TOKEN = 5,
    PING = 6,
    ACTIVATE = 7,
    DEACTIVATE = 8,
    REFRESH = 9,
    GET_GRAPH_DATA = 10,
    EXPORT_GRAPH_DATA = 11,
    SET_WIDGET_PROPERTY = 12,
    BRIDGE = 15,
    HARDWARE = 20,
    CREATE_DASH = 21,
    UPDATE_DASH = 22,
    DELETE_DASH = 23,
    LOAD_PROF_GZ = 24,
    SYNC = 25,
    SHARING = 26,
    ADD_PUSH_TOKEN = 27,
    EMAIL = 28,
    PUSH_NOTIFICATION = 29,
    BRIDGE_STATE = 30,
    SMS = 31
};

enum TinkerIoTStatus {
    SUCCESS = 200,
    QUOTA_LIMIT = 1,
    ILLEGAL_COMMAND = 2,
    NOT_REGISTERED = 3,
    ALREADY_REGISTERED = 4,
    NOT_AUTHENTICATED = 5,
    NOT_ALLOWED = 6,
    DEVICE_NOT_IN_NETWORK = 7,
    NO_ACTIVE_DASHBOARD = 8,
    INVALID_TOKEN = 9,
    ILLEGAL_COMMAND_BODY = 11,
    GET_GRAPH_DATA_EXCEPTION = 12,
    NO_DATA_EXCEPTION = 17,
    DEVICE_WENT_OFFLINE = 18,
    SERVER_EXCEPTION = 19
};

// Forward declarations
class TinkerIoTClass;

// Function pointer type for TinkerIoT write handlers
typedef void (*TinkerIoTWriteHandler)(String value);

// Function pointer type for timer callbacks
typedef void (*TinkerIoTTimerCallback)();

// ===== AUTO-REGISTRATION SYSTEM =====

class TinkerIoTAutoRegister {
public:
    struct HandlerNode {
        int pin;
        TinkerIoTWriteHandler handler;
        HandlerNode* next;
    };
    
private:
    static HandlerNode* handlerList;
    
public:
    // Constructor automatically registers the handler
    TinkerIoTAutoRegister(int pin, TinkerIoTWriteHandler handler) {
        HandlerNode* newNode = new HandlerNode;
        newNode->pin = pin;
        newNode->handler = handler;
        newNode->next = handlerList;
        handlerList = newNode;
        
 
    }
    
    // Static method to register all queued handlers
    static void registerAll();
    
    // Static method to get handler count
    static int getHandlerCount();
};

// Helper class for parameter access 
class TinkerIoTParam {
private:
    String _value;
public:
    TinkerIoTParam(String value = "") : _value(value) {}
    
    int asInt() { return _value.toInt(); }
    float asFloat() { return _value.toFloat(); }
    String asString() { return _value; }
    bool asBool() { 
        return (_value == "1" || _value.equalsIgnoreCase("true") || _value.equalsIgnoreCase("on")); 
    }
    
    void setValue(String value) { _value = value; }
};

// Timer class for TinkerIoT
class TinkerIoTTimer {
private:
    struct TimerItem {
        unsigned long interval;
        unsigned long lastRun;
        TinkerIoTTimerCallback callback;
        bool enabled;
    };
    
    #if defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_SAMD_MKRWIFI1010)
        static const int MAX_TIMERS = 8;  // Reduce for SAMD boards
    #else
        static const int MAX_TIMERS = 16;
    #endif
    TimerItem timers[MAX_TIMERS];
    int timerCount;
    
public:
    TinkerIoTTimer() : timerCount(0) {
        for (int i = 0; i < MAX_TIMERS; i++) {
            timers[i].enabled = false;
        }
    }
    
    // Set interval timer  - AUTO-ENABLED BY DEFAULT
    int setInterval(unsigned long interval, TinkerIoTTimerCallback callback) {
        if (timerCount >= MAX_TIMERS) return -1;
        
        timers[timerCount].interval = interval;
        timers[timerCount].lastRun = millis();
        timers[timerCount].callback = callback;
        timers[timerCount].enabled = true;  // Auto-enabled by default
        
        return timerCount++;
    }
    
    // Set timeout timer (one-shot)
    int setTimeout(unsigned long timeout, TinkerIoTTimerCallback callback) {
        if (timerCount >= MAX_TIMERS) return -1;
        
        timers[timerCount].interval = timeout;
        timers[timerCount].lastRun = millis();
        timers[timerCount].callback = callback;
        timers[timerCount].enabled = true;
        
        // Mark as timeout (negative interval after first run)
        int timerId = timerCount++;
        return timerId;
    }
    
    // Run all timers (call this in loop)
    void run() {
        unsigned long currentTime = millis();
        
        for (int i = 0; i < timerCount; i++) {
            if (!timers[i].enabled) continue;
            
            if (currentTime - timers[i].lastRun >= timers[i].interval) {
                timers[i].lastRun = currentTime;
                
                // Call the callback function
                if (timers[i].callback) {
                    timers[i].callback();
                }
                
                // For setTimeout, disable after first run
                if (timers[i].interval < 0) {
                    timers[i].enabled = false;
                }
            }
        }
    }
    
    // Enable/disable timer
    void enable(int timerId) {
        if (timerId >= 0 && timerId < timerCount) {
            timers[timerId].enabled = true;
            timers[timerId].lastRun = millis(); // Reset timing
        }
    }
    
    void disable(int timerId) {
        if (timerId >= 0 && timerId < timerCount) {
            timers[timerId].enabled = false;
        }
    }
    
    // Change interval
    void changeInterval(int timerId, unsigned long newInterval) {
        if (timerId >= 0 && timerId < timerCount) {
            timers[timerId].interval = newInterval;
            timers[timerId].lastRun = millis(); // Reset timing
        }
    }
    
    // Delete timer
    void deleteTimer(int timerId) {
        if (timerId >= 0 && timerId < timerCount) {
            timers[timerId].enabled = false;
        }
    }
};

// Enhanced TinkerIoT class with auto-registration support
class TinkerIoTClass {
private:
    // WebSocket client
    WebSocketsClient webSocket;
    
    // Connection state
    bool isConnected = false;
    bool loginSent = false;
    bool loginFailed = false;           // Track login failure
    bool tokenErrorReported = false;    // Track if token error was already reported
    
    // Server configuration - OPTIMIZED for dynamic assignment
    const char* server_host = "10.161.42.200";  // Change this to the server ip
    int server_port = 8008;                     // Removed const to allow modification
    bool use_ssl = true;                        // Removed const to allow modification
    
    // Device credentials
    String device_token;
    String wifi_ssid;
    String wifi_password;
    
    // Cloud pins storage
    String cloudPins[32];

    // Mutex for protecting cloudPins[] array from race conditions
    TINKERIOT_MUTEX_TYPE cloudPinsMutex = TINKERIOT_MUTEX_INIT;

    // Timing
    unsigned long lastHeartbeat = 0;
    const unsigned long heartbeatInterval = 5000;
    unsigned long loginAttemptTime = 0;         // Track login attempt time
    const unsigned long loginTimeout = 10000;  // 10 second login timeout
    unsigned long firstConnectionAttempt = 0;   // Track first connection attempt
    int connectionFailureCount = 0;             // Count connection failures
    
    // Write handlers array
    TinkerIoTWriteHandler writeHandlers[32] = {nullptr};
    
    // Private methods
    void connectToWiFi();
    void setupWebSocket();
    void sendLogin();
    void handleTinkerIoTMessage(uint8_t* data, size_t length);
    void handleHardwareCommand(uint16_t msg_id, uint8_t* body, uint16_t length);
    void sendTinkerIoTMessage(uint8_t command, uint16_t msg_id, String body);
    void sendHardwareMessage(uint16_t msg_id, String body);
    void sendResponse(uint16_t msg_id, uint8_t status);
    void cloudRead(int pin, String value);
    
    // Static WebSocket event handler
    static void webSocketEventStatic(WStype_t type, uint8_t * payload, size_t length);
    void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
    
    // Static instance pointer for callback
    static TinkerIoTClass* instance;

public:
    // Constructor
    TinkerIoTClass();
    
    // ENHANCED begin methods with auto-registration
    void begin(const char* auth_token, const char* ssid, const char* password);
    void begin(const char* auth_token, const char* ssid, const char* password, const char* server, int port = 8008);
    void run();
    
    // Data sending methods (Device → App)
    void cloudWrite(int pin, String value);
    void cloudWrite(int pin, int value);
    void cloudWrite(int pin, float value);
    void cloudWrite(int pin, double value);
    
    // Connection status
    bool connected() { return isConnected && loginSent; }
    bool loginSuccess() { return loginSent; }           // Check if login was successful
    bool loginFailing() { return loginFailed; }        // Check if login failed
    bool websocketConnected() { return isConnected; }  // Check WebSocket connection only
    
    // Handler registration (internal use)
    void _registerWriteHandler(int pin, TinkerIoTWriteHandler handler);
};

// Global TinkerIoT object
extern TinkerIoTClass TinkerIoT;

// Global parameter object
extern TinkerIoTParam param;

// ===== ENHANCED MACROS WITH AUTO-REGISTRATION =====

// TINKERIOT_WRITE now auto-registers - no ATTACH needed!
#define TINKERIOT_WRITE(pin) \
    void tinkerIoTWriteHandler##pin(String value); \
    TinkerIoTAutoRegister autoReg##pin(pin, tinkerIoTWriteHandler##pin); \
    void tinkerIoTWriteHandler##pin(String value)

// Convenience macro for connected callback  
#define TINKERIOT_CONNECTED() void tinkerIoTConnectedHandler()

// TINKERIOT_ATTACH is now optional/deprecated but kept for backwards compatibility
#define TINKERIOT_ATTACH(pin) TinkerIoT._registerWriteHandler(pin, tinkerIoTWriteHandler##pin)

// Optional: Manual registration function (if auto-registration fails)
#define TINKERIOT_REGISTER_ALL() TinkerIoTAutoRegister::registerAll()

#endif // TINKERIOT_H
