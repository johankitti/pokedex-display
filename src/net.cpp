#include "net.h"
#include "config.h"
#include "display.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <esp_heap_caps.h>

static uint8_t* allocBuf(size_t n) {
    uint8_t* p = (uint8_t*)heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
    if (!p) p = (uint8_t*)malloc(n);   // fall back to internal RAM
    return p;
}

void netFree(uint8_t* buf) {
    if (buf) free(buf);   // free() routes to the right heap on ESP32
}

bool netStart(bool forcePortal) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    WiFiManager wm;
    wm.setConfigPortalTimeout(0);     // keep the portal open until configured
    // State 0: the moment the setup AP opens, show join instructions + the portal
    // address on the panel. Fires only when the portal actually opens, so a normal
    // boot with saved creds connects instantly and never shows this.
    wm.setAPCallback([](WiFiManager*) {
        String ip = WiFi.softAPIP().toString();
        displaySetup(AP_SETUP_SSID, ip.c_str());
        Serial.printf("[net] setup AP '%s' open at http://%s\n",
                      AP_SETUP_SSID, ip.c_str());
    });

    bool ok = forcePortal ? wm.startConfigPortal(AP_SETUP_SSID)
                          : wm.autoConnect(AP_SETUP_SSID);

    if (ok && WiFi.status() == WL_CONNECTED) {
        Serial.printf("[net] IP %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    return false;
}

bool netEnsure() {
    if (WiFi.status() == WL_CONNECTED) return true;
    WiFi.reconnect();                 // use saved creds; never opens the portal
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
        delay(200);
    }
    return WiFi.status() == WL_CONNECTED;
}

bool httpGetBinary(const char* url, uint8_t** outBuf, size_t* outLen) {
    *outBuf = nullptr;
    *outLen = 0;
    if (!netEnsure()) return false;

    WiFiClientSecure client;
    client.setInsecure();   // sprites are public; skip cert pinning

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) return false;

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[net] GET %d for %s\n", code, url);
        http.end();
        return false;
    }

    int len = http.getSize();             // Content-Length, or -1 if unknown
    WiFiClient* stream = http.getStreamPtr();

    size_t cap = (len > 0) ? (size_t)len : 8192;
    uint8_t* buf = allocBuf(cap);
    if (!buf) { http.end(); return false; }

    size_t got = 0;
    uint32_t lastData = millis();
    while (http.connected() && (len < 0 || got < (size_t)len)) {
        size_t avail = stream->available();
        if (avail) {
            if (got + avail > cap) {                 // grow when length unknown
                size_t ncap = cap * 2;
                while (ncap < got + avail) ncap *= 2;
                uint8_t* nbuf = allocBuf(ncap);
                if (!nbuf) { free(buf); http.end(); return false; }
                memcpy(nbuf, buf, got);
                free(buf);
                buf = nbuf;
                cap = ncap;
            }
            int r = stream->readBytes(buf + got, avail);
            if (r > 0) { got += r; lastData = millis(); }
        } else {
            if (millis() - lastData > HTTP_TIMEOUT_MS) break;
            delay(2);
        }
    }
    http.end();

    if (got == 0 || (len > 0 && got < (size_t)len)) {
        free(buf);
        return false;
    }
    *outBuf = buf;
    *outLen = got;
    return true;
}
