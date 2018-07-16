#define APP_NAME                "Camera Latency"
#define APP_VERSION             "1.0.0"
#define APP_AUTHOR              "richard.osterloh@gmail.com"
#define MANUFACTURER			"OXSIGHT"

#define DEBUG_SUPPORT   1
#define DEBUG_MESSAGE_MAX_LENGTH    164

#if DEBUG_SUPPORT
    #define DEBUG_MSG(...) Serial.printf(__VA_ARGS__)
    #define DEBUG_MSG_P(...) Serial.printf(__VA_ARGS__)
#endif

#ifndef DEBUG_MSG
    #define DEBUG_MSG(...)
    #define DEBUG_MSG_P(...)
#endif