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
        TINKERIOT_PRINT.println("🔄 Auto-registering TINKERIOT_WRITE handlers...");
    }
    #endif
    
    while (current != nullptr) {
        if (current->handler != nullptr) {
            TinkerIoT._registerWriteHandler(current->pin, current->handler);
            registeredCount++;
            
            #ifdef TINKERIOT_PRINT
            TINKERIOT_PRINT.printf("✅ Auto-registered C%d handler\n", current->pin);
            #endif
        }
        current = current->next;
    }
    
    #ifdef TINKERIOT_PRINT
    if (registeredCount > 0) {
        TINKERIOT_PRINT.printf("🎉 Auto-registered %d handlers successfully!\n", registeredCount);
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
    
    #ifdef TINKERIOT_PRINT
    // Show auto-registration info
    int pendingHandlers = TinkerIoTAutoRegister::getHandlerCount();
    if (pendingHandlers > 0) {
        TINKERIOT_PRINT.printf("📋 TinkerIoT initialized - %d handlers ready for auto-registration\n", pendingHandlers);
    }
    #endif
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
        Serial.printf("🔒 SSL enabled (port %d)\n", port);
        #endif
    } else {
        use_ssl = false;
        #ifdef TINKERIOT_PRINT
        Serial.printf("🔓 SSL disabled (port %d)\n", port);
        #endif
    }
    
    #ifdef TINKERIOT_PRINT
    TINKERIOT_PRINT.begin(9600);
    TINKERIOT_PRINT.println();
    TINKERIOT_PRINT.println("🚀 TinkerIoT ESP32 Client Starting...");
    TINKERIOT_PRINT.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    TINKERIOT_PRINT.printf("🔑 Testing token: %s\n", auth_token);
    TINKERIOT_PRINT.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    #endif
    
    connectToWiFi();
    setupWebSocket();
    
    // ===== AUTOMATIC HANDLER REGISTRATION =====
    #ifdef TINKERIOT_PRINT
    int handlerCount = TinkerIoTAutoRegister::getHandlerCount();
    if (handlerCount > 0) {
        TINKERIOT_PRINT.printf("🔄 Found %d TINKERIOT_WRITE handlers for auto-registration\n", handlerCount);
    }
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
        TINKERIOT_DATA_DEBUG.printf("❌ Invalid pin number: %d\n", pin);
        #endif
        return;
    }
    
    // Enhanced connection check - protect against sending during login phase
    if (!isConnected || !loginSent || loginFailed) {
        #ifdef TINKERIOT_DATA_DEBUG
        TINKERIOT_DATA_DEBUG.printf("❌ Cannot send C%d=%s - connection not ready (connected:%s, login:%s, failed:%s)\n", 
                                   pin, value.c_str(), 
                                   isConnected ? "Y" : "N",
                                   loginSent ? "Y" : "N", 
                                   loginFailed ? "Y" : "N");
        #endif
        return;
    }
    
    // Add small grace period after login to ensure connection stability
    if (loginSent && (millis() - loginAttemptTime) < 1000) {
        #ifdef TINKERIOT_DATA_DEBUG
        TINKERIOT_DATA_DEBUG.printf("⏳ Waiting for connection to stabilize before sending C%d=%s\n", pin, value.c_str());
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
    TINKERIOT_DATA_DEBUG.printf("📤 TinkerIoT.cloudWrite: C%d = '%s'\n", pin, value.c_str());
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
        TINKERIOT_PRINT.printf("✅ Handler registered for C%d\n", pin);
        #endif
    } else {
        #ifdef TINKERIOT_PRINT
        TINKERIOT_PRINT.printf("❌ Invalid pin number for handler: %d\n", pin);
        #endif
    }
}

// WiFi connection
void TinkerIoTClass::connectToWiFi() {
    #ifdef TINKERIOT_PRINT
    TINKERIOT_PRINT.print("📶 Connecting to WiFi: ");
    TINKERIOT_PRINT.println(wifi_ssid);
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
        webSocket.beginSSL(server_host, server_port, websocket_path);
    } else {
        webSocket.begin(server_host, server_port, websocket_path);
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
                    TINKERIOT_PRINT.printf("🔄 Connection attempt %d failed, retrying...\n", connectionFailureCount);
                    #endif
                }
            }
            // Don't show repeated "WebSocket Disconnected" after token error reported
            
            isConnected = false;
            loginSent = false;
            break;
            
        case WStype_CONNECTED:
            #ifdef TINKERIOT_PRINT
            TINKERIOT_PRINT.printf("✅ WebSocket Connected to: %s\n", payload);
            #endif
            isConnected = true;
            loginFailed = false;
            connectionFailureCount = 0;  // Reset failure count on successful connection
            delay(1000);
            sendLogin();
            break;
            
        case WStype_BIN:
            #ifdef TINKERIOT_DATA_DEBUG
            TINKERIOT_DATA_DEBUG.printf("📨 Received binary message (%d bytes)\n", length);
            #endif
            handleTinkerIoTMessage(payload, length);
            break;
            
        case WStype_TEXT:
            #ifdef TINKERIOT_DATA_DEBUG
            TINKERIOT_DATA_DEBUG.printf("📨 Received text message: %s\n", payload);
            #endif
            break;
            
        case WStype_ERROR:
            #ifdef TINKERIOT_PRINT
            TINKERIOT_PRINT.printf("❌ WebSocket Error: %s\n", payload);
            #endif
            break;
            
        default:
            #ifdef TINKERIOT_DATA_DEBUG
            TINKERIOT_DATA_DEBUG.printf("🤔 Unknown WebSocket event: %d\n", type);
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
    TINKERIOT_DATA_DEBUG.printf("📋 CMD: %d, ID: %d, LEN: %d\n", command, msg_id, body_length);
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
                TINKERIOT_DATA_DEBUG.printf("📬 Response: %s (%d)\n", 
                    status == SUCCESS ? "SUCCESS" : "ERROR", status);
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
                                TINKERIOT_PRINT.printf("❓ ERROR: Authentication failed (Code: %d)\n", status);
                                TINKERIOT_PRINT.println("💡 Contact support for assistance.");
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
            TINKERIOT_DATA_DEBUG.printf("🤔 Unknown command: %d\n", command);
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
    TINKERIOT_DATA_DEBUG.printf("🔧 Hardware command: %s\n", command.c_str());
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
            TINKERIOT_DATA_DEBUG.printf("📥 Cloud write: C%d = %s\n", pin, value.c_str());
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
        TINKERIOT_DATA_DEBUG.printf("📤 Cloud read: C%d = %s\n", pin, value.c_str());
        #endif
        
        String response = "cw\0" + String(pin) + "\0" + value;
        sendHardwareMessage(msg_id, response);
    }
}

// Handle cloud read (App → Device) - Enhanced with better debugging
void TinkerIoTClass::cloudRead(int pin, String value) {
    if (pin >= 0 && pin < 32 && writeHandlers[pin] != nullptr) {
        #ifdef TINKERIOT_DATA_DEBUG
        TINKERIOT_DATA_DEBUG.printf("📥 Calling TINKERIOT_WRITE(C%d) handler with value: %s\n", pin, value.c_str());
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
        TINKERIOT_DATA_DEBUG.printf("🔧 No TINKERIOT_WRITE handler for pin C%d: %s\n", pin, value.c_str());
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
    TINKERIOT_DATA_DEBUG.printf("📤 Sending TinkerIoT message: CMD=%d, ID=%d, LEN=%d\n", command, msg_id, bodyLength);
    #endif
    
    webSocket.sendBIN(message, 5 + bodyLength);
    delete[] message;
}