#include "OtaManager.h"

#include "config.h"
#include "firmware_version.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cJSON.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

#include <freertos/semphr.h>

#include <cstring>

extern const uint8_t kCaBundle[] asm("_binary_x509_crt_bundle_start");

static const char *kApSsid = "LoveDisplay-Setup";
static const char *kApPassword = "lovedisplay";
static const size_t kMaxAppBytes = 0x640000;
static const size_t kMaxManifestBytes = 2048;
static const size_t kMaxJpegBytes = 80000;

static Preferences prefs;
static WebServer portal(80);
static DNSServer dns;
static bool portalOn = false;
static bool saveRequested = false;
static char pendingSsid[33];
static char pendingPass[65];
static char savedSsid[33];
static char savedPass[65];
static bool haveWifi = false;
static bool wasConnected = false;
static uint32_t nextWifiAttempt = 0;
static uint32_t wifiBackoff = 5000;
static uint32_t nextOtaCheck = 0;
static uint32_t nextTriggerPoll = 0;
static uint32_t lastTriggerId = 0;
static bool otaScheduled = false;
static bool forceCheck = false;
static DisplayScene pendingScene = {"heart", "Happy Bday", "heart"};
static bool sceneDirty = false;
static DeskFaces pendingFaces = {};
static bool facesDirty = false;
static char loadedBackground[3][40] = {};
static SemaphoreHandle_t sceneLock = nullptr;

static const char kPortalPage[] = R"HTML(<!DOCTYPE html>
<html><head><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Love Display</title></head>
<body>
<h1>Love Display Wi-Fi</h1>
<form method="POST" action="/save">
<label>Wi-Fi name<br><input name="ssid" required maxlength="32"></label><br><br>
<label>Wi-Fi password<br><input name="password" type="password" maxlength="64"></label><br><br>
<button type="submit">Connect</button>
</form>
</body></html>)HTML";

static void trust(WiFiClientSecure &client) {
    client.setCACertBundle(kCaBundle);
    client.setHandshakeTimeout(15);
}

static bool isHttps(const String &url) {
    return url.startsWith("https://");
}

static String hostOf(const String &url) {
    if (!isHttps(url)) {
        return "";
    }
    int start = 8;
    int slash = url.indexOf('/', start);
    int colon = url.indexOf(':', start);
    int end = url.length();
    if (slash >= 0) {
        end = slash;
    }
    if (colon >= 0 && (slash < 0 || colon < slash)) {
        end = colon;
    }
    String host = url.substring(start, end);
    host.toLowerCase();
    return host;
}

static bool hostAllowed(const String &url) {
    String host = hostOf(url);
    return host == "github.com" || host == "raw.githubusercontent.com" ||
           host == "objects.githubusercontent.com" ||
           host == "release-assets.githubusercontent.com" ||
           host == "github-releases.githubusercontent.com";
}

static String resolveRedirect(const String &base, const String &location) {
    if (location.startsWith("https://") || location.startsWith("http://")) {
        return location;
    }
    int scheme = base.indexOf("://");
    if (scheme < 0) {
        return "";
    }
    int path = base.indexOf('/', scheme + 3);
    if (location.startsWith("/")) {
        String origin = path >= 0 ? base.substring(0, path) : base;
        return origin + location;
    }
    return "";
}

static bool parseVersion(const char *text, int &major, int &minor, int &patch) {
    int consumed = 0;
    if (sscanf(text, "%d.%d.%d%n", &major, &minor, &patch, &consumed) != 3) {
        return false;
    }
    return text[consumed] == '\0' && major >= 0 && minor >= 0 && patch >= 0;
}

static int compareVersion(const char *published) {
    int pMaj, pMin, pPat, iMaj, iMin, iPat;
    if (!parseVersion(published, pMaj, pMin, pPat) ||
        !parseVersion(FIRMWARE_VERSION, iMaj, iMin, iPat)) {
        return 0;
    }
    if (pMaj != iMaj) {
        return pMaj - iMaj;
    }
    if (pMin != iMin) {
        return pMin - iMin;
    }
    return pPat - iPat;
}

static bool shaMatches(const uint8_t digest[32], const char *hex) {
    if (hex == nullptr || strlen(hex) != 64) {
        return false;
    }
    for (int i = 0; i < 32; ++i) {
        char pair[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
        char *end = nullptr;
        long value = strtol(pair, &end, 16);
        if (end != pair + 2 || value != digest[i]) {
            return false;
        }
    }
    return true;
}

static void sendPortal() {
    portal.send(200, "text/html", kPortalPage);
}

static void redirectPortal() {
    portal.sendHeader("Location", "http://192.168.4.1/", true);
    portal.send(302, "text/plain", "");
}

static void handleSave() {
    String ssid = portal.arg("ssid");
    String pass = portal.arg("password");
    ssid.trim();
    if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 64) {
        portal.send(400, "text/plain", "Enter a Wi-Fi name.");
        return;
    }
    strncpy(pendingSsid, ssid.c_str(), sizeof(pendingSsid) - 1);
    pendingSsid[sizeof(pendingSsid) - 1] = '\0';
    strncpy(pendingPass, pass.c_str(), sizeof(pendingPass) - 1);
    pendingPass[sizeof(pendingPass) - 1] = '\0';
    saveRequested = true;
    Serial.printf("ota: received Wi-Fi credentials for \"%s\"\n", pendingSsid);
    portal.send(200, "text/html",
                "<p>Trying that network. You can close this page.</p>");
}

static void startPortal() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(kApSsid, kApPassword);
    dns.start(53, "*", WiFi.softAPIP());
    portal.on("/", HTTP_GET, sendPortal);
    portal.on("/save", HTTP_POST, handleSave);
    portal.on("/generate_204", HTTP_GET, redirectPortal);
    portal.on("/gen_204", HTTP_GET, redirectPortal);
    portal.on("/hotspot-detect.html", HTTP_GET, redirectPortal);
    portal.on("/connecttest.txt", HTTP_GET, redirectPortal);
    portal.on("/ncsi.txt", HTTP_GET, redirectPortal);
    portal.onNotFound(redirectPortal);
    portal.begin();
    portalOn = true;
    Serial.println("ota: join LoveDisplay-Setup, password lovedisplay, then open http://192.168.4.1");
}

static void stopPortal() {
    if (!portalOn) {
        return;
    }
    portal.stop();
    dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    portalOn = false;
}

static void rememberWifi(const char *ssid, const char *pass) {
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();
    strncpy(savedSsid, ssid, sizeof(savedSsid) - 1);
    savedSsid[sizeof(savedSsid) - 1] = '\0';
    strncpy(savedPass, pass, sizeof(savedPass) - 1);
    savedPass[sizeof(savedPass) - 1] = '\0';
    haveWifi = savedSsid[0] != '\0';
}

static void clearWifi() {
    prefs.begin("wifi", false);
    prefs.clear();
    prefs.end();
    savedSsid[0] = '\0';
    savedPass[0] = '\0';
    haveWifi = false;
    wasConnected = false;
    otaScheduled = false;
    WiFi.disconnect(true, true);
    Serial.println("ota: Wi-Fi settings cleared");
    if (!portalOn) {
        startPortal();
    }
}

static bool loadWifi() {
    prefs.begin("wifi", true);
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    prefs.end();
    if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 64) {
        return false;
    }
    strncpy(savedSsid, ssid.c_str(), sizeof(savedSsid) - 1);
    savedSsid[sizeof(savedSsid) - 1] = '\0';
    strncpy(savedPass, pass.c_str(), sizeof(savedPass) - 1);
    savedPass[sizeof(savedPass) - 1] = '\0';
    return true;
}

static bool tryJoin(const char *ssid, const char *pass, uint32_t timeoutMs) {
    WiFi.mode(portalOn ? WIFI_AP_STA : WIFI_STA);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.disconnect(false);
    WiFi.begin(ssid, pass);
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (portalOn) {
            dns.processNextRequest();
            portal.handleClient();
        }
        if (WiFi.status() == WL_CONNECTED) {
            return true;
        }
        delay(50);
    }
    return false;
}

static void serviceSerial() {
    static char line[32];
    static size_t used = 0;
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\r') {
            continue;
        }
        if (c != '\n') {
            if (used + 1 < sizeof(line)) {
                line[used++] = c;
            }
            continue;
        }
        line[used] = '\0';
        used = 0;
        if (strcmp(line, "wifi-reset") == 0) {
            clearWifi();
        } else if (strcmp(line, "check") == 0) {
            forceCheck = true;
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("ota: checking now");
            } else {
                Serial.println("ota: check requested, waiting for Wi-Fi");
            }
        } else if (strcmp(line, "status") == 0) {
            Serial.printf("ota: version %s wifi %s manifest %s\n", FIRMWARE_VERSION,
                          WiFi.status() == WL_CONNECTED ? "up" : "down",
                          GITHUB_MANIFEST_URL[0] == '\0' ? "unset" : "set");
        }
    }
}

static bool streamToFlash(HTTPClient &http, const char *expectedSha) {
    int announced = http.getSize();
    if (announced > (int)kMaxAppBytes) {
        Serial.println("ota: firmware is larger than the OTA slot. Current firmware kept.");
        return false;
    }
    size_t expected = announced > 0 ? (size_t)announced : 0;
    if (!Update.begin(expected > 0 ? expected : UPDATE_SIZE_UNKNOWN)) {
        Serial.printf("ota: could not start update (%s). Current firmware kept.\n",
                      Update.errorString());
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);

    uint8_t buf[1024];
    size_t written = 0;
    uint32_t lastData = millis();
    bool failed = false;
    while (!failed) {
        int avail = stream->available();
        if (avail > 0) {
            size_t want = (size_t)avail;
            if (want > sizeof(buf)) {
                want = sizeof(buf);
            }
            int n = stream->readBytes(buf, want);
            if (n <= 0) {
                failed = true;
                break;
            }
            mbedtls_sha256_update(&sha, buf, (size_t)n);
            if (Update.write(buf, (size_t)n) != (size_t)n) {
                Serial.printf("ota: flash write failed (%s). Current firmware kept.\n",
                              Update.errorString());
                failed = true;
                break;
            }
            written += (size_t)n;
            lastData = millis();
            if (written > kMaxAppBytes) {
                Serial.println("ota: download exceeded the OTA slot. Current firmware kept.");
                failed = true;
                break;
            }
            if (expected > 0 && written >= expected) {
                break;
            }
        } else if (!http.connected()) {
            break;
        } else if (millis() - lastData > 20000) {
            Serial.println("ota: download timed out. Current firmware kept.");
            failed = true;
            break;
        } else {
            delay(1);
        }
    }

    uint8_t digest[32];
    mbedtls_sha256_finish(&sha, digest);
    mbedtls_sha256_free(&sha);

    if (failed || (expected > 0 && written != expected) || written == 0) {
        Update.abort();
        if (!failed) {
            Serial.println("ota: download ended early. Current firmware kept.");
        }
        return false;
    }
    if (!shaMatches(digest, expectedSha)) {
        Update.abort();
        Serial.println("ota: SHA-256 mismatch. Update aborted. Current firmware kept.");
        return false;
    }
    if (!Update.end(expected == 0)) {
        Update.abort();
        Serial.printf("ota: install failed (%s). Current firmware kept.\n", Update.errorString());
        return false;
    }
    Serial.printf("ota: SHA-256 verified (%u bytes). Rebooting into the new firmware.\n",
                  (unsigned)written);
    delay(200);
    ESP.restart();
    return true;
}

static bool downloadRelease(const char *url, const char *expectedSha) {
    String current = url;
    WiFiClientSecure client;
    for (int hop = 0; hop < 5; ++hop) {
        if (!isHttps(current) || !hostAllowed(current)) {
            Serial.println("ota: download URL is not an https GitHub release address");
            return false;
        }
        client.stop();
        trust(client);
        HTTPClient http;
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        http.setConnectTimeout(15000);
        http.setTimeout(20000);
        http.setUserAgent("LoveDisplay-OTA");
        if (!http.begin(client, current)) {
            Serial.println("ota: could not open the firmware download");
            return false;
        }
        int code = http.GET();
        if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
            String next = resolveRedirect(current, http.getLocation());
            http.end();
            if (next.length() == 0) {
                Serial.println("ota: redirect was empty");
                return false;
            }
            current = next;
            continue;
        }
        if (code != 200) {
            Serial.printf("ota: download HTTP %d. Current firmware kept.\n", code);
            http.end();
            return false;
        }
        Serial.printf("ota: downloading release from %s\n", hostOf(current).c_str());
        bool ok = streamToFlash(http, expectedSha);
        http.end();
        return ok;
    }
    Serial.println("ota: too many redirects. Current firmware kept.");
    return false;
}

static bool fetchManifest(String &body) {
    String current = GITHUB_MANIFEST_URL;
    WiFiClientSecure client;
    for (int hop = 0; hop < 4; ++hop) {
        if (!isHttps(current) || !hostAllowed(current)) {
            Serial.println("ota: manifest URL must be https on GitHub");
            return false;
        }
        client.stop();
        trust(client);
        HTTPClient http;
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        http.setConnectTimeout(15000);
        http.setTimeout(15000);
        http.setUserAgent("LoveDisplay-OTA");
        if (!http.begin(client, current)) {
            Serial.println("ota: could not open the manifest");
            return false;
        }
        int code = http.GET();
        if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
            String next = resolveRedirect(current, http.getLocation());
            http.end();
            if (next.length() == 0) {
                Serial.println("ota: manifest redirect was empty");
                return false;
            }
            current = next;
            continue;
        }
        if (code == 429 || code == 403) {
            Serial.printf("ota: manifest HTTP %d, will retry later\n", code);
            http.end();
            return false;
        }
        if (code != 200) {
            Serial.printf("ota: manifest HTTP %d\n", code);
            http.end();
            return false;
        }
        int size = http.getSize();
        if (size > (int)kMaxManifestBytes) {
            Serial.println("ota: manifest is too large");
            http.end();
            return false;
        }
        body = http.getString();
        http.end();
        if (body.length() == 0 || body.length() > kMaxManifestBytes) {
            Serial.println("ota: manifest was empty or too large");
            return false;
        }
        return true;
    }
    Serial.println("ota: too many manifest redirects");
    return false;
}

static void loadTriggerId() {
    prefs.begin("ota", true);
    lastTriggerId = prefs.getUInt("check_id", 0);
    prefs.end();
}

static void rememberTriggerId(uint32_t id) {
    lastTriggerId = id;
    prefs.begin("ota", false);
    prefs.putUInt("check_id", id);
    prefs.end();
}

// Looks at firmware/check.json. A higher id asks the board to check the release.
static void pollRemoteTrigger() {
    String url = GITHUB_MANIFEST_URL;
    if (!url.endsWith("manifest.json")) {
        return;
    }
    url.replace("manifest.json", "check.json");

    WiFiClientSecure client;
    trust(client);
    HTTPClient http;
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.setConnectTimeout(10000);
    http.setTimeout(10000);
    http.setUserAgent("LoveDisplay-OTA");
    if (!http.begin(client, url)) {
        return;
    }
    int code = http.GET();
    if (code != 200) {
        http.end();
        static bool logged = false;
        if (!logged) {
            Serial.printf("ota: remote check file HTTP %d\n", code);
            logged = true;
        }
        return;
    }
    String body = http.getString();
    http.end();
    cJSON *root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        return;
    }
    cJSON *id = cJSON_GetObjectItem(root, "id");
    if (!cJSON_IsNumber(id) || id->valuedouble < 0 || id->valuedouble > 4294967295.0) {
        cJSON_Delete(root);
        return;
    }
    uint32_t requested = (uint32_t)id->valuedouble;
    cJSON_Delete(root);
    if (requested > lastTriggerId) {
        Serial.printf("ota: remote check %u\n", requested);
        rememberTriggerId(requested);
        forceCheck = true;
    }
}

static void copyField(char *dest, size_t size, const char *value);

static bool safeBackgroundName(const char *name) {
    if (name == nullptr || name[0] == '\0') {
        return false;
    }
    size_t length = strlen(name);
    if (length < 6 || length >= 40 || strcmp(name + length - 4, ".jpg") != 0) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        char c = name[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.';
        if (!ok) {
            return false;
        }
    }
    return true;
}

static uint8_t *downloadBackground(const char *name, size_t &outLength) {
    outLength = 0;
    String url = GITHUB_MANIFEST_URL;
    if (!url.endsWith("manifest.json")) {
        return nullptr;
    }
    url.replace("manifest.json", "backgrounds/");
    url += name;

    WiFiClientSecure client;
    for (int hop = 0; hop < 4; ++hop) {
        if (!isHttps(url) || !hostAllowed(url)) {
            Serial.println("companion: background URL is not on GitHub");
            return nullptr;
        }
        client.stop();
        trust(client);
        HTTPClient http;
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        http.setConnectTimeout(10000);
        http.setTimeout(20000);
        http.setUserAgent("LoveDisplay-OTA");
        if (!http.begin(client, url)) {
            return nullptr;
        }
        int code = http.GET();
        if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
            String next = resolveRedirect(url, http.getLocation());
            http.end();
            if (next.length() == 0) {
                return nullptr;
            }
            url = next;
            continue;
        }
        if (code != 200) {
            Serial.printf("companion: background HTTP %d\n", code);
            http.end();
            return nullptr;
        }
        int size = http.getSize();
        if (size <= 0 || size > (int)kMaxJpegBytes) {
            Serial.println("companion: background is missing or too large");
            http.end();
            return nullptr;
        }
        uint8_t *buffer = (uint8_t *)ps_malloc((size_t)size);
        if (buffer == nullptr) {
            buffer = (uint8_t *)malloc((size_t)size);
        }
        if (buffer == nullptr) {
            http.end();
            return nullptr;
        }
        WiFiClient *stream = http.getStreamPtr();
        size_t got = 0;
        uint32_t last = millis();
        while (got < (size_t)size && http.connected()) {
            int available = stream->available();
            if (available > 0) {
                int n = stream->readBytes(buffer + got, (size_t)size - got);
                if (n > 0) {
                    got += (size_t)n;
                    last = millis();
                }
            } else if (millis() - last > 15000) {
                break;
            } else {
                delay(1);
            }
        }
        http.end();
        if (got != (size_t)size || buffer[0] != 0xFF || buffer[1] != 0xD8) {
            free(buffer);
            Serial.println("companion: background download was incomplete");
            return nullptr;
        }
        outLength = got;
        return buffer;
    }
    return nullptr;
}

static void fillBackground(BackgroundFile &slot, int index, cJSON *faces, const char *key) {
    slot.jpeg = nullptr;
    slot.length = 0;
    slot.name[0] = '\0';
    if (!cJSON_IsObject(faces)) {
        copyField(slot.name, sizeof(slot.name), loadedBackground[index]);
        return;
    }
    cJSON *value = cJSON_GetObjectItem(faces, key);
    if (!cJSON_IsString(value)) {
        copyField(slot.name, sizeof(slot.name), loadedBackground[index]);
        return;
    }
    const char *wanted = value->valuestring;
    if (wanted[0] == '\0') {
        loadedBackground[index][0] = '\0';
        return;
    }
    if (!safeBackgroundName(wanted)) {
        copyField(slot.name, sizeof(slot.name), loadedBackground[index]);
        return;
    }
    copyField(slot.name, sizeof(slot.name), wanted);
    if (strcmp(loadedBackground[index], wanted) == 0) {
        return;
    }
    size_t length = 0;
    uint8_t *jpeg = downloadBackground(wanted, length);
    if (jpeg == nullptr) {
        copyField(slot.name, sizeof(slot.name), loadedBackground[index]);
        return;
    }
    slot.jpeg = jpeg;
    slot.length = length;
    copyField(loadedBackground[index], sizeof(loadedBackground[index]), wanted);
    Serial.printf("companion: background %s\n", wanted);
}

static bool allowedIcon(const char *icon) {
    return strcmp(icon, "heart") == 0 || strcmp(icon, "star") == 0 || strcmp(icon, "sparkle") == 0 ||
           strcmp(icon, "smile") == 0 || strcmp(icon, "flower") == 0 || strcmp(icon, "sun") == 0 ||
           strcmp(icon, "moon") == 0 || strcmp(icon, "note") == 0;
}

static void copyField(char *dest, size_t size, const char *value) {
    strncpy(dest, value, size - 1);
    dest[size - 1] = '\0';
}

static void pollDisplay() {
    String url = GITHUB_MANIFEST_URL;
    if (!url.endsWith("manifest.json")) {
        return;
    }
    url.replace("manifest.json", "display.json");

    WiFiClientSecure client;
    trust(client);
    HTTPClient http;
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.setConnectTimeout(10000);
    http.setTimeout(10000);
    http.setUserAgent("LoveDisplay-OTA");
    if (!http.begin(client, url)) {
        return;
    }
    int code = http.GET();
    if (code != 200) {
        http.end();
        return;
    }
    String body = http.getString();
    http.end();
    cJSON *root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        return;
    }
    cJSON *mode = cJSON_GetObjectItem(root, "mode");
    cJSON *message = cJSON_GetObjectItem(root, "message");
    cJSON *icon = cJSON_GetObjectItem(root, "icon");
    if (!cJSON_IsString(mode) || !cJSON_IsString(message) || !cJSON_IsString(icon)) {
        cJSON_Delete(root);
        return;
    }
    DisplayScene next = pendingScene;
    if (strcmp(mode->valuestring, "text") == 0 || strcmp(mode->valuestring, "emoji") == 0 ||
        strcmp(mode->valuestring, "heart") == 0) {
        copyField(next.mode, sizeof(next.mode), mode->valuestring);
    }
    copyField(next.message, sizeof(next.message), message->valuestring);
    if (allowedIcon(icon->valuestring)) {
        copyField(next.icon, sizeof(next.icon), icon->valuestring);
    }

    DeskFaces faces = {};
    faces.message = next;
    copyField(faces.active, sizeof(faces.active), "message");
    cJSON *active = cJSON_GetObjectItem(root, "active");
    if (cJSON_IsString(active) && (strcmp(active->valuestring, "message") == 0 ||
                                   strcmp(active->valuestring, "clock") == 0 ||
                                   strcmp(active->valuestring, "animation") == 0)) {
        copyField(faces.active, sizeof(faces.active), active->valuestring);
    }
    cJSON *backgrounds = cJSON_GetObjectItem(root, "backgrounds");
    const char *keys[3] = {"message", "clock", "animation"};
    for (int i = 0; i < 3; ++i) {
        fillBackground(faces.backgrounds[i], i, backgrounds, keys[i]);
    }
    cJSON_Delete(root);

    if (sceneLock == nullptr) {
        for (int i = 0; i < 3; ++i) {
            free(faces.backgrounds[i].jpeg);
        }
        return;
    }
    xSemaphoreTake(sceneLock, portMAX_DELAY);
    bool changed = strcmp(pendingScene.mode, next.mode) != 0 || strcmp(pendingScene.message, next.message) != 0 ||
                   strcmp(pendingScene.icon, next.icon) != 0 || strcmp(pendingFaces.active, faces.active) != 0;
    for (int i = 0; i < 3 && !changed; ++i) {
        changed = strcmp(pendingFaces.backgrounds[i].name, faces.backgrounds[i].name) != 0 ||
                  faces.backgrounds[i].jpeg != nullptr;
    }
    if (changed) {
        for (int i = 0; i < 3; ++i) {
            if (faces.backgrounds[i].jpeg == nullptr) {
                faces.backgrounds[i].jpeg = pendingFaces.backgrounds[i].jpeg;
                faces.backgrounds[i].length = pendingFaces.backgrounds[i].length;
            } else if (pendingFaces.backgrounds[i].jpeg != nullptr &&
                       pendingFaces.backgrounds[i].jpeg != faces.backgrounds[i].jpeg) {
                free(pendingFaces.backgrounds[i].jpeg);
            }
        }
        pendingScene = next;
        pendingFaces = faces;
        sceneDirty = true;
        facesDirty = true;
        Serial.printf("ota: new screen %s / %s\n", next.mode, next.icon);
    } else {
        for (int i = 0; i < 3; ++i) {
            free(faces.backgrounds[i].jpeg);
        }
    }
    xSemaphoreGive(sceneLock);
}

// Returns true when GitHub answered and no retry is needed yet.
static bool checkManifest() {
    if (GITHUB_MANIFEST_URL[0] == '\0') {
        static bool logged = false;
        if (!logged) {
            Serial.println("ota: GITHUB_MANIFEST_URL is empty, skipping");
            logged = true;
        }
        return true;
    }

    String body;
    if (!fetchManifest(body)) {
        return false;
    }

    cJSON *root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        Serial.println("ota: manifest is not valid JSON");
        return false;
    }
    cJSON *version = cJSON_GetObjectItem(root, "version");
    cJSON *fileUrl = cJSON_GetObjectItem(root, "url");
    cJSON *sha = cJSON_GetObjectItem(root, "sha256");
    cJSON *status = cJSON_GetObjectItem(root, "status");
    bool valid = cJSON_IsString(version) && cJSON_IsString(fileUrl) && cJSON_IsString(sha) &&
                 cJSON_IsString(status);
    if (!valid) {
        Serial.println("ota: manifest is missing version, url, sha256, or status");
        cJSON_Delete(root);
        return false;
    }

    const char *published = version->valuestring;
    const char *statusText = status->valuestring;
    Serial.printf("ota: installed %s, published %s, status %s\n", FIRMWARE_VERSION, published,
                  statusText);
    if (strcmp(statusText, "enabled") != 0) {
        Serial.println("ota: release is not enabled");
        cJSON_Delete(root);
        return true;
    }
    int cmp = compareVersion(published);
    if (cmp <= 0) {
        Serial.println("ota: no newer firmware");
        cJSON_Delete(root);
        return true;
    }
    if (strlen(sha->valuestring) != 64) {
        Serial.println("ota: sha256 must be 64 hex characters");
        cJSON_Delete(root);
        return false;
    }

    String url = fileUrl->valuestring;
    String shaCopy = sha->valuestring;
    cJSON_Delete(root);
    return downloadRelease(url.c_str(), shaCopy.c_str());
}

static void networkTask(void *) {
    haveWifi = loadWifi();
    loadTriggerId();
    if (!haveWifi) {
        startPortal();
    } else {
        WiFi.mode(WIFI_STA);
        WiFi.persistent(false);
        WiFi.setAutoReconnect(true);
        Serial.printf("ota: connecting to \"%s\"\n", savedSsid);
        WiFi.begin(savedSsid, savedPass);
        nextWifiAttempt = millis();
    }

    for (;;) {
        serviceSerial();
        if (portalOn) {
            dns.processNextRequest();
            portal.handleClient();
            if (saveRequested) {
                saveRequested = false;
                Serial.printf("ota: trying \"%s\"\n", pendingSsid);
                if (tryJoin(pendingSsid, pendingPass, 20000)) {
                    rememberWifi(pendingSsid, pendingPass);
                    pendingPass[0] = '\0';
                    stopPortal();
                    wasConnected = true;
                    otaScheduled = false;
                    Serial.printf("ota: Wi-Fi connected, IP %s\n", WiFi.localIP().toString().c_str());
                } else {
                    pendingPass[0] = '\0';
                    Serial.println("ota: could not join that network. Setup page is still available.");
                }
            }
        } else if (haveWifi) {
            bool connected = WiFi.status() == WL_CONNECTED;
            if (connected && !wasConnected) {
                wasConnected = true;
                wifiBackoff = 5000;
                otaScheduled = false;
                nextTriggerPoll = millis() + OTA_TRIGGER_INTERVAL_MS;
                Serial.printf("ota: Wi-Fi connected, IP %s\n", WiFi.localIP().toString().c_str());
            } else if (!connected && wasConnected) {
                wasConnected = false;
                otaScheduled = false;
                Serial.println("ota: Wi-Fi lost, retrying without restarting");
            } else if (!connected && millis() - nextWifiAttempt >= wifiBackoff) {
                Serial.printf("ota: reconnecting to \"%s\"\n", savedSsid);
                WiFi.disconnect(false);
                WiFi.begin(savedSsid, savedPass);
                nextWifiAttempt = millis();
                wifiBackoff = wifiBackoff < 60000 ? wifiBackoff * 2 : 60000;
            }

            if (connected) {
                uint32_t now = millis();
                if ((int32_t)(now - nextTriggerPoll) >= 0) {
                    nextTriggerPoll = now + OTA_TRIGGER_INTERVAL_MS;
                    pollRemoteTrigger();
                    pollDisplay();
                }
                if (!otaScheduled) {
                    nextOtaCheck = now + OTA_FIRST_CHECK_MS;
                    otaScheduled = true;
                }
                if (forceCheck || (int32_t)(now - nextOtaCheck) >= 0) {
                    forceCheck = false;
                    bool settled = checkManifest();
                    nextOtaCheck = millis() + (settled ? OTA_CHECK_INTERVAL_MS : OTA_RETRY_MS);
                }
            }
        }
        delay(20);
    }
}

void OtaManager::onDisplayFailed() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (running != nullptr && esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        Serial.println("ota: new firmware failed display startup. Rebooting so the previous firmware returns.");
        delay(100);
        ESP.restart();
    }
}

bool OtaManager::takeScene(DisplayScene &out) {
    if (sceneLock == nullptr || !sceneDirty) {
        return false;
    }
    xSemaphoreTake(sceneLock, portMAX_DELAY);
    out = pendingScene;
    sceneDirty = false;
    xSemaphoreGive(sceneLock);
    return true;
}

bool OtaManager::takeFaces(DeskFaces &out) {
    if (sceneLock == nullptr || !facesDirty) {
        return false;
    }
    xSemaphoreTake(sceneLock, portMAX_DELAY);
    out = pendingFaces;
    for (int i = 0; i < 3; ++i) {
        pendingFaces.backgrounds[i].jpeg = nullptr;
        pendingFaces.backgrounds[i].length = 0;
    }
    pendingScene = pendingFaces.message;
    sceneDirty = false;
    facesDirty = false;
    xSemaphoreGive(sceneLock);
    return true;
}

void OtaManager::begin() {
    if (sceneLock == nullptr) {
        sceneLock = xSemaphoreCreateMutex();
    }
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running != nullptr) {
        Serial.printf("ota: running slot %s\n", running->label);
    }
    esp_err_t marked = esp_ota_mark_app_valid_cancel_rollback();
    if (marked == ESP_OK) {
        Serial.println("ota: this firmware is marked valid");
    }

    xTaskCreatePinnedToCore(networkTask, "ota", 20480, nullptr, 1, nullptr, 0);
}
