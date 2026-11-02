// Minimal WiFi/Arduino stubs so radar/conn.h + radargauge.h compile on host
// for the headless render test.
#ifndef HOST_STUB_WIFI_H
#define HOST_STUB_WIFI_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <cstdarg>

#define WL_CONNECTED 3
#define WIFI_STA 1

struct SerialStub {
    void printf(const char* fmt, ...) {
        va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
    }
    void println(const char* s) { printf("%s\n", s); }
    void print(const char* s) { printf("%s", s); }
};
extern SerialStub Serial;

class WiFiClient {
public:
    bool connect(const char*, int) { return false; }
    bool connected() { return false; }
    void stop() {}
    int read(uint8_t*, size_t) { return -1; }
    void println(const char*) {}
};

class WiFiClass {
public:
    void mode(int) {}
    void begin(const char*, const char*) {}
    int status() { return 0; }
    void disconnect() {}
};
extern WiFiClass WiFi;

#endif
