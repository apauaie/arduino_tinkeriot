#include "TinkerIoT.h"

// Global instances
TinkerIoTClass TinkerIoT;
TinkerIoTParam param;

// Static instance pointer for callback
TinkerIoTClass* TinkerIoTClass::instance = nullptr;

// ===== AUTO-REGISTRATION SYSTEM IMPLEMENTATION =====

// Initialize static member
TinkerIoTAutoRegister::HandlerNode* TinkerIoTAutoRegister::handlerList = nullptr;

void TinkerIoTAutoRegister::registerAll() {
    HandlerNode* current = handlerList;
    int registeredCount = 0;
    
    #ifdef TINKERIOT_PRINT
    if (current != nullptr) {
        TINKERIOT_PRINT.println("🔄 Auto-registering TINKERIOT_WRITE handlers.....");
    }
    #endif
    
    while (current != nullptr) {
        if (current->handler != nullptr) {
            TinkerIoT._registerWriteHandler(current->pin, current->handler);
            registeredCount++;
            
            #ifdef TINKERIOT_PRINT
            TINKERIOT_PRINT.print("✅ Auto-registered C");
            TINKERIOT_PRINT.print(current->pin);
            TINKERIOT_PRINT.println(" handler");
            #endif
        }
        current = current->next;
    }
    
    #ifdef TINKERIOT_PRINT
    if (registeredCount > 0) {
        TINKERIOT_PRINT.print("🎉 Auto-registered ");
        TINKERIOT_PRINT.print(registeredCount);
        TINKERIOT_PRINT.println(" handlers successfully!");
        TINKERIOT_PRINT.println("✅ All handlers auto-registered - no ATTACH needed!");
    } else {
        TINKERIOT_PRINT.println("ℹ️ No TINKERIOT_WRITE handlers found to register");
    }
    #endif
}

int TinkerIoTAutoRegister::getHandlerCount() {
    int count = 0;
    HandlerNode* current = handlerList;
    
    while (current != nullptr) {
        if (current->handler != nullptr) {
            count++;
        }
        current = current->next;
    }
    
    return count;
}

// Constructor - Enhanced with auto-registration info
TinkerIoTClass::TinkerIoTClass() {
    instance = this;
    // Initialize cloud pins
    for (int i = 0; i < 32; i++) {
        cloudPins[i] = "";
        writeHandlers[i] = nullptr;
    }
    // Initialize token validation variables
    tokenErrorReported = false;
    connectionFailureCount = 0;
    firstConnectionAttempt = 0;
    
  
}

void TinkerIoTClass::begin(const char* auth_token, const char* ssid, const char* password, const char* server, int port) {
    device_token = String(auth_token);
    wifi_ssid = String(ssid);
    wifi_password = String(password);
    server_host = server;
    server_port = port;

    if ( port == 8443) {
        use_ssl = true;
        #ifdef TINKERIOT_PRINT
        Serial.print("🔒 SSL enabled (port ");
        Serial.print(port);
        Serial.println(")");        
        #endif
    } else {
        use_ssl = false;
        #ifdef TINKERIOT_PRINT
        Serial.print("🔒 SSL disabled (port ");
        Serial.print(port);
        Serial.println(")");        
        #endif
    }
    
    #ifdef TINKERIOT_PRINT
    TINKERIOT_PRINT.println();
    TINKERIOT_PRINT.println("🚀 TinkerIoT ESP32 Client Starting...");
    TINKERIOT_PRINT.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    TINKERIOT_PRINT.print("🔑 Testing token: ");
    TINKERIOT_PRINT.println(auth_token);    TINKERIOT_PRINT.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    #endif
    
    connectToWiFi();
    setupWebSocket();
    
    // ===== AUTOMATIC HANDLER REGISTRATION =====
    #ifdef TINKERIOT_PRINT
    int handlerCount = TinkerIoTAutoRegister::getHandlerCount();
    if (handlerCount > 0) {
    TINKERIOT_PRINT.print("🔄 Found ");
    TINKERIOT_PRINT.print(handlerCount);
    TINKERIOT_PRINT.println(" TINKERIOT_WRITE handlers for auto-registration");    }
    #endif
    
    TinkerIoTAutoRegister::registerAll();
}

// Main run function (call this in loop)
void TinkerIoTClass::run() {
    webSocket.loop();
    
    // Automatic token validation - Check for login timeout
    if (isConnected && !loginSent && !loginFailed) {
        if (millis() - loginAttemptTime > loginTimeout) {
            loginFailed = true;
            #ifdef TINKERIOT_PRINT
            TINKERIOT_PRINT.println();
            TINKERIOT_PRINT.println("❌ TOKEN VALIDATION FAILED!");
            TINKERIOT_PRINT.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            TINKERIOT_PRINT.println("🔑 ERROR: Invalid authentication token!");
            TINKERIOT_PRINT.println("💡 Your token appears to be incorrect.");
            TINKERIOT_PRINT.println();
            TINKERIOT_PRINT.println("🔧 Please check:");
            TINKERIOT_PRINT.println("   • Token spelling and format");
            TINKERIOT_PRINT.println("   • Token is not expired");
            TINKERIOT_PRINT.println("   • Device is properly registered");
            TINKERIOT_PRINT.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            TINKERIOT_PRINT.println();
            #endif
        }
    }
    
    // Send heartbeat periodically if connected
    if (millis() - lastHeartbeat > heartbeatInterval) {
        if (isConnected && loginSent) {
            // Optional: You can add automatic heartbeat data here
            // For example, send uptime or other system data
        }
        lastHeartbeat = millis();
    }
}

// OPTIMIZED: Cloud write methods (Device → App) - REMOVED blocking delay
void TinkerIoTClass::cloudWrite(int pin, String value) {
    if (pin < 0 || pin >= 32) {
        #ifdef TINKERIOT_DATA_DEBUG
        TINKERIOT_DATA_DEBUG.print("❌ Invalid pin number: ");
        TINKERIOT_DATA_DEBUG.println(pin);        
        #endif
        return;
    }
    
    // Enhanced connection check - protect against sending during login phase
    if (!isConnected || !loginSent || loginFailed) {
        #ifdef TINKERIOT_DATA_DEBUG
        TINKERIOT_DATA_DEBUG.print("❌ Cannot send C");
        TINKERIOT_DATA_DEBUG.print(pin);
        TINKERIOT_DATA_DEBUG.print("=");
        TINKERIOT_DATA_DEBUG.print(value);
        TINKERIOT_DATA_DEBUG.print(" - connection not ready (connected:");
        TINKERIOT_DATA_DEBUG.print(isConnected ? "Y" : "N");
        TINKERIOT_DATA_DEBUG.print(", login:");
        TINKERIOT_DATA_DEBUG.print(loginSent ? "Y" : "N");
        TINKERIOT_DATA_DEBUG.print(", failed:");
        TINKERIOT_DATA_DEBUG.print(loginFailed ? "Y" : "N");
        TINKERIOT_DATA_DEBUG.println(")");
        #endif
        return;
    }
    
    // Add small grace period after login to ensure connection stability
    if (loginSent && (millis() - loginAttemptTime) < 1000) {
        #ifdef TINKERIOT_DATA_DEBUG
        TINKERIOT_DATA_DEBUG.print("⏳ Waiting for connection to stabilize before sending C");
        TINKERIOT_DATA_DEBUG.print(pin);
        TINKERIOT_DATA_DEBUG.print("=");
        TINKERIOT_DATA_DEBUG.println(value);        
        #endif
        return;
    }
    
    // Store locally first
    cloudPins[pin] = value;
    
    // Create command string with explicit null terminators
    String command = "cw";
    command += '\0';
    command += String(pin);
    command += '\0';
    command += value;
    
    // Send to server
    #ifdef TINKERIOT_DATA_DEBUG
    TINKERIOT_DATA_DEBUG.print("📤 TinkerIoT.cloudWrite: C");
    TINKERIOT_DATA_DEBUG.print(pin);
    TINKERIOT_DATA_DEBUG.print(" = '");
    TINKERIOT_DATA_DEBUG.print(value);
    TINKERIOT_DATA_DEBUG.println("'");    
    #endif
    sendHardwareMessage(0, command);
    
    // REMOVED: delay(50) - this was causing slow responses!
}

void TinkerIoTClass::cloudWrite(int pin, int value) {
    cloudWrite(pin, String(value));
}

void TinkerIoTClass::cloudWrite(int pin, float value) {
    cloudWrite(pin, String(value, 2));
}

void TinkerIoTClass::cloudWrite(int pin, double value) {
    cloudWrite(pin, String(value, 2));
}

// Register write handler (internal use) - Enhanced with better debugging
void TinkerIoTClass::_registerWriteHandler(int pin, TinkerIoTWriteHandler handler) {
    if (pin >= 0 && pin < 32) {
        writeHandlers[pin] = handler;
        #ifdef TINKERIOT_PRINT
        TINKERIOT_PRINT.print("✅ Handler registered for C");
        TINKERIOT_PRINT.println(pin);        
        #endif
    } else {
        #ifdef TINKERIOT_PRINT
        TINKERIOT_PRINT.print("❌ Invalid pin number for handler: ");
        TINKERIOT_PRINT.println(pin);       
         #endif
    }
}

// WiFi connection
void TinkerIoTClass::connectToWiFi() {
    #if defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_SAMD_MKRWIFI1010)
    while (!TINKERIOT_PRINT) {
        3; // Wait for serial port
      }
    #endif
    #ifdef TINKERIOT_PRINT
    TINKERIOT_PRINT.print("📶 Connecting to WiFi: ");
    TINKERIOT_PRINT.println(wifi_ssid);
    #endif
    
    // Nano 33 IoT WiFi initialization
    #if defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_SAMD_MKRWIFI1010)
    // Check for WiFi module
    if (WiFi.status() == WL_NO_MODULE) {
        #ifdef TINKERIOT_PRINT
        TINKERIOT_PRINT.println("❌ WiFi module not found!");
        #endif
        while (true); // Don't continue
    }
    #endif
    
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        #ifdef TINKERIOT_PRINT
        TINKERIOT_PRINT.print(".");
        #endif
    }
    
    #ifdef TINKERIOT_PRINT
    TINKERIOT_PRINT.println();
    TINKERIOT_PRINT.println("✅ WiFi connected!");
    TINKERIOT_PRINT.print("📍 IP address: ");
    TINKERIOT_PRINT.println(WiFi.localIP());
    #endif
}

// WebSocket setup
void TinkerIoTClass::setupWebSocket() {
    String websocket_path = "/hardware/" + device_token;
    
    #ifdef TINKERIOT_PRINT
    TINKERIOT_PRINT.print("🔌 Connecting to TinkerIoT: ");
    #endif
    
    if (use_ssl) {
        // For Nano 33 IoT with WiFiNINA, this should work
        webSocket.beginSSL(server_host, server_port, websocket_path.c_str());
        
        // Optional: Disable SSL certificate verification if needed
        // webSocket.setSSLClientCertKey(...); // For client certificates
        
    } else {
        webSocket.begin(server_host, server_port, websocket_path.c_str());
    }
    
    webSocket.onEvent(webSocketEventStatic);
    webSocket.setReconnectInterval(5000);
}

// Static WebSocket event handler
void TinkerIoTClass::webSocketEventStatic(WStype_t type, uint8_t * payload, size_t length) {
    if (instance) {
        instance->webSocketEvent(type, payload, length);
    }
}

// WebSocket event handler
void TinkerIoTClass::webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            // Track connection failures for token detection
            if (!tokenErrorReported) {
                connectionFailureCount++;
                
                // Record first connection attempt time
                if (firstConnectionAttempt == 0) {
                    firstConnectionAttempt = millis();
                }
                
                // After 3 failures or 15 seconds, assume invalid token
                if (connectionFailureCount >= 3 || (millis() - firstConnectionAttempt > 15000)) {
                    tokenErrorReported = true;
                    #ifdef TINKERIOT_PRINT
                    TINKERIOT_PRINT.println();
                    TINKERIOT_PRINT.println("❌ TOKEN VALIDATION FAILED!");
                    TINKERIOT_PRINT.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
                    TINKERIOT_PRINT.println("🔑 ERROR: Invalid authentication token!");
                    TINKERIOT_PRINT.println("💡 Server is rejecting the connection.");
                    TINKERIOT_PRINT.println("📊 Multiple connection attempts failed.");
                    TINKERIOT_PRINT.println();
                    TINKERIOT_PRINT.println("🔧 Quick fixes:");
                    TINKERIOT_PRINT.println("   • Double-check your TINKERIOT_AUTH_TOKEN");
                    TINKERIOT_PRINT.println("   • Verify token format is correct");
                    TINKERIOT_PRINT.println("   • Ensure device is properly registered");
                    TINKERIOT_PRINT.println("   • Restart ESP32 to retry");
                    TINKERIOT_PRINT.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
                    TINKERIOT_PRINT.println();
                    #endif
                } else {
                    // Show limited disconnection messages
                    #ifdef TINKERIOT_PRINT
                    TINKERIOT_PRINT.print("🔄 Connection attempt ");
                    TINKERIOT_PRINT.print(connectionFailureCount);
                    TINKERIOT_PRINT.println(" failed, retrying...");
                    #endif
                }
            }
            // Don't show repeated "WebSocket Disconnected" after token error reported
            
            isConnected = false;
            loginSent = false;
            break;
            
        case WStype_CONNECTED:
            #ifdef TINKERIOT_PRINT
            TINKERIOT_PRINT.print("✅ WebSocket Connected to: ");
            TINKERIOT_PRINT.println((char*)payload);            
            #endif
            isConnected = true;
            loginFailed = false;
            connectionFailureCount = 0;  // Reset failure count on successful connection
            delay(1000);
            sendLogin();
            break;
            
        case WStype_BIN:
            #ifdef TINKERIOT_DATA_DEBUG
            TINKERIOT_DATA_DEBUG.print("📨 Received binary message (");
            TINKERIOT_DATA_DEBUG.print(length);
            TINKERIOT_DATA_DEBUG.println(" bytes)");            
            #endif
            handleTinkerIoTMessage(payload, length);
            break;
            
        case WStype_TEXT:
            #ifdef TINKERIOT_DATA_DEBUG
            TINKERIOT_DATA_DEBUG.print("📨 Received text message: ");
            TINKERIOT_DATA_DEBUG.println((char*)payload);
            #endif
            break;
            
        case WStype_ERROR:
            #ifdef TINKERIOT_PRINT
            TINKERIOT_PRINT.print("❌ WebSocket Error: ");
            TINKERIOT_PRINT.println((char*)payload);            
            #endif
            break;
            
        default:
            #ifdef TINKERIOT_DATA_DEBUG
            TINKERIOT_DATA_DEBUG.print("🤔 Unknown WebSocket event: ");
            TINKERIOT_DATA_DEBUG.println(type);            
            #endif
            break;
    }
}

// Handle TinkerIoT protocol messages
void TinkerIoTClass::handleTinkerIoTMessage(uint8_t* data, size_t length) {
    if (length < 5) {
        #ifdef TINKERIOT_DATA_DEBUG
        TINKERIOT_DATA_DEBUG.println("❌ Invalid message length");
        #endif
        return;
    }
    
    uint8_t command = data[0];
    uint16_t msg_id = (data[1] << 8) | data[2];
    uint16_t body_length = (data[3] << 8) | data[4];
    
    #ifdef TINKERIOT_DATA_DEBUG
    TINKERIOT_DATA_DEBUG.print("📋 CMD: ");
    TINKERIOT_DATA_DEBUG.print(command);
    TINKERIOT_DATA_DEBUG.print(", ID: ");
    TINKERIOT_DATA_DEBUG.print(msg_id);
    TINKERIOT_DATA_DEBUG.print(", LEN: ");
    TINKERIOT_DATA_DEBUG.println(body_length);    
    #endif
    
    switch (command) {
        case PING:
            #ifdef TINKERIOT_DATA_DEBUG
            TINKERIOT_DATA_DEBUG.println("🏓 Ping received - sending response");
            #endif
            sendResponse(msg_id, SUCCESS);
            break;
            
        case HARDWARE:
            if (length > 5) {
                handleHardwareCommand(msg_id, data + 5, body_length);
            }
            break;
            
        case RESPONSE:
            if (length > 5) {
                uint8_t status = data[5];
                
                #ifdef TINKERIOT_DATA_DEBUG
                TINKERIOT_DATA_DEBUG.print("📬 Response: ");
                TINKERIOT_DATA_DEBUG.print(status == SUCCESS ? "SUCCESS" : "ERROR");
                TINKERIOT_DATA_DEBUG.print(" (");
                TINKERIOT_DATA_DEBUG.print(status);
                TINKERIOT_DATA_DEBUG.println(")");                    
                #endif
                
                // Handle login response specifically
                if (!loginSent && !loginFailed) {
                    if (status == SUCCESS) {
                        loginSent = true;
                        loginAttemptTime = millis(); // Record successful login time
                        #ifdef TINKERIOT_PRINT
                        TINKERIOT_PRINT.println();
                        TINKERIOT_PRINT.println("✅ TOKEN VALIDATED SUCCESSFULLY!");
                        TINKERIOT_PRINT.println("🎉 Authentication complete - Ready to use!");
                        TINKERIOT_PRINT.println();
                        #endif
                    } else {
                        loginFailed = true;
                        #ifdef TINKERIOT_PRINT
                        TINKERIOT_PRINT.println();
                        TINKERIOT_PRINT.println("❌ TOKEN VALIDATION FAILED!");
                        TINKERIOT_PRINT.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
                        
                        // Provide specific error messages based on status code
                        switch(status) {
                            case INVALID_TOKEN:
                                TINKERIOT_PRINT.println("🔑 ERROR: Invalid authentication token!");
                                TINKERIOT_PRINT.println("💡 The token format or value is incorrect.");
                                break;
                            case NOT_AUTHENTICATED:
                                TINKERIOT_PRINT.println("🔒 ERROR: Authentication failed!");
                                TINKERIOT_PRINT.println("💡 Token may be expired or revoked.");
                                break;
                            case NOT_REGISTERED:
                                TINKERIOT_PRINT.println("📝 ERROR: Device not registered!");
                                TINKERIOT_PRINT.println("💡 Please register your device first.");
                                break;
                            case DEVICE_NOT_IN_NETWORK:
                                TINKERIOT_PRINT.println("🌐 ERROR: Device not in network!");
                                TINKERIOT_PRINT.println("💡 Check your device configuration.");
                                break;
                            case QUOTA_LIMIT:
                                TINKERIOT_PRINT.println("📊 ERROR: Too many requests!");
                                TINKERIOT_PRINT.println("💡 Wait before retrying connection.");
                                break;
                            default:
                            TINKERIOT_PRINT.print("❓ ERROR: Authentication failed (Code: ");
                            TINKERIOT_PRINT.print(status);
                            TINKERIOT_PRINT.println(")");                     TINKERIOT_PRINT.println("💡 Contact support for assistance.");
                                break;
                        }
                        
                        TINKERIOT_PRINT.println();
                        TINKERIOT_PRINT.println("🔧 Quick fixes:");
                        TINKERIOT_PRINT.println("   • Double-check your TINKERIOT_AUTH_TOKEN");
                        TINKERIOT_PRINT.println("   • Verify device registration");
                        TINKERIOT_PRINT.println("   • Restart ESP32 to retry");
                        TINKERIOT_PRINT.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
                        TINKERIOT_PRINT.println();
                        #endif
                    }
                }
            }
            break;
            
        default:
            #ifdef TINKERIOT_DATA_DEBUG
            TINKERIOT_DATA_DEBUG.print("🤔 Unknown command: ");
            TINKERIOT_DATA_DEBUG.println(command);            
            #endif
            break;
    }
}

// Handle hardware commands
void TinkerIoTClass::handleHardwareCommand(uint16_t msg_id, uint8_t* body, uint16_t length) {
    String command = "";
    for (int i = 0; i < length; i++) {
        command += (char)body[i];
    }
    
    #ifdef TINKERIOT_DATA_DEBUG
    TINKERIOT_DATA_DEBUG.print("🔧 Hardware command: ");
    TINKERIOT_DATA_DEBUG.println(command);    
    #endif
    
    int firstNull = command.indexOf('\0');
    int secondNull = command.indexOf('\0', firstNull + 1);
    
    if (firstNull == -1) return;
    
    String cmdType = command.substring(0, firstNull);
    String pinStr = command.substring(firstNull + 1, secondNull > 0 ? secondNull : command.length());
    
    int pin = pinStr.toInt();
    
    if (cmdType == "cw") {
        // Cloud write - server writing to device
        if (secondNull > 0) {
            String value = command.substring(secondNull + 1);
            #ifdef TINKERIOT_DATA_DEBUG
            TINKERIOT_DATA_DEBUG.print("📥 Cloud write: C");
            TINKERIOT_DATA_DEBUG.print(pin);
            TINKERIOT_DATA_DEBUG.print(" = ");
            TINKERIOT_DATA_DEBUG.println(value);            
            #endif
            
            if (pin >= 0 && pin < 32) {
                cloudPins[pin] = value;
                cloudRead(pin, value);
            }
        }
        sendResponse(msg_id, SUCCESS);
        
    } else if (cmdType == "cr") {
        // Cloud read - server reading from device
        String value = "";
        if (pin >= 0 && pin < 32) {
            value = cloudPins[pin];
        }
        
        #ifdef TINKERIOT_DATA_DEBUG
        TINKERIOT_DATA_DEBUG.print("📤 Cloud read: C");
        TINKERIOT_DATA_DEBUG.print(pin);
        TINKERIOT_DATA_DEBUG.print(" = ");
        TINKERIOT_DATA_DEBUG.println(value);        
        #endif
        
        String response = "cw\0" + String(pin) + "\0" + value;
        sendHardwareMessage(msg_id, response);
    }
}

// Handle cloud read (App → Device) - Enhanced with better debugging
void TinkerIoTClass::cloudRead(int pin, String value) {
    if (pin >= 0 && pin < 32 && writeHandlers[pin] != nullptr) {
        #ifdef TINKERIOT_DATA_DEBUG
        TINKERIOT_DATA_DEBUG.print("📥 Calling TINKERIOT_WRITE(C");
        TINKERIOT_DATA_DEBUG.print(pin);
        TINKERIOT_DATA_DEBUG.print(") handler with value: ");
        TINKERIOT_DATA_DEBUG.println(value);        
        #endif
        
        // Update global param object
        param.setValue(value);
        
        // Call the registered handler function
        writeHandlers[pin](value);
        
        // Echo the pin to send the value to dashboard
        cloudWrite(pin, value);
        
        // Don't store control pin values for pins 0-10 to prevent echo
        if (pin > 10) {
            cloudPins[pin] = value;
        }
    } else {
        #ifdef TINKERIOT_DATA_DEBUG
        TINKERIOT_DATA_DEBUG.print("🔧 No TINKERIOT_WRITE handler for pin C");
        TINKERIOT_DATA_DEBUG.print(pin);
        TINKERIOT_DATA_DEBUG.print(": ");
        TINKERIOT_DATA_DEBUG.println(value);        
        #endif
        
        if (pin >= 0 && pin < 32) {
            cloudPins[pin] = value;
        }
    }
}

// Send login message
void TinkerIoTClass::sendLogin() {
    #ifdef TINKERIOT_PRINT
    TINKERIOT_PRINT.println("🔐 Sending login message...");
    #endif
    
    loginAttemptTime = millis();  // Record login attempt time
    sendTinkerIoTMessage(LOGIN, 1, "");
}

// Send hardware message
void TinkerIoTClass::sendHardwareMessage(uint16_t msg_id, String body) {
    sendTinkerIoTMessage(HARDWARE, msg_id, body);
}

// Send response
void TinkerIoTClass::sendResponse(uint16_t msg_id, uint8_t status) {
    uint8_t responseData[6];
    responseData[0] = RESPONSE;
    responseData[1] = (msg_id >> 8) & 0xFF;
    responseData[2] = msg_id & 0xFF;
    responseData[3] = 0;
    responseData[4] = 1;
    responseData[5] = status;
    
    webSocket.sendBIN(responseData, 6);
}

// Send TinkerIoT message
void TinkerIoTClass::sendTinkerIoTMessage(uint8_t command, uint16_t msg_id, String body) {
    if (!isConnected) {
        #ifdef TINKERIOT_DATA_DEBUG
        TINKERIOT_DATA_DEBUG.println("❌ Not connected - cannot send message");
        #endif
        return;
    }
    
    uint16_t bodyLength = body.length();
    uint8_t* message = new uint8_t[5 + bodyLength];
    
    message[0] = command;
    message[1] = (msg_id >> 8) & 0xFF;
    message[2] = msg_id & 0xFF;
    message[3] = (bodyLength >> 8) & 0xFF;
    message[4] = bodyLength & 0xFF;
    
    for (int i = 0; i < bodyLength; i++) {
        message[5 + i] = body[i];
    }
    
    #ifdef TINKERIOT_DATA_DEBUG
    TINKERIOT_DATA_DEBUG.print("📤 Sending TinkerIoT message: CMD=");
    TINKERIOT_DATA_DEBUG.print(command);
    TINKERIOT_DATA_DEBUG.print(", ID=");
    TINKERIOT_DATA_DEBUG.print(msg_id);
    TINKERIOT_DATA_DEBUG.print(", LEN=");
    TINKERIOT_DATA_DEBUG.println(bodyLength);    
    #endif
    
    webSocket.sendBIN(message, 5 + bodyLength);
    delete[] message;
}
