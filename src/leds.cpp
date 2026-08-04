#include "leds.h"
#include <Preferences.h>

Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);
static Preferences ledPrefs;

struct LedChannelConfig {
    const char* name;
    const char* prefKeyIndex;
    const char* prefKeyColor;   // nullptr si couleur fixe
    uint8_t ledIndex;
    uint8_t r, g, b;
    bool colorConfigurable;
};

static LedChannelConfig ledChannels[] = {
    {"A",  "liA",  nullptr, 0,  0,   255, 0,   false},
    {"B",  "liB",  nullptr, 2,  255, 0,   0,   false},
    {"X",  "liX",  nullptr, 4,  0,   0,   255, false},
    {"Y",  "liY",  nullptr, 6,  255, 255, 0,   false},
    {"LB", "liLB", "lcLB",  8,  255, 140, 0,   true},
    {"RB", "liRB", "lcRB",  10, 128, 0,   128, true},
    {"LT", "liLT", "lcLT",  12, 0,   255, 255, true},
    {"RT", "liRT", "lcRT",  14, 255, 0,   255, true},
};
static const uint8_t NUM_LED_CHANNELS = sizeof(ledChannels) / sizeof(ledChannels[0]);

static void saveLedIndexToPrefs(uint8_t i) {
    ledPrefs.begin("fightstick", false);
    ledPrefs.putUChar(ledChannels[i].prefKeyIndex, ledChannels[i].ledIndex);
    ledPrefs.end();
}

static void saveLedColorToPrefs(uint8_t i) {
    if (!ledChannels[i].colorConfigurable) return;
    uint32_t packed = ((uint32_t)ledChannels[i].r << 16) | ((uint32_t)ledChannels[i].g << 8) | ledChannels[i].b;
    ledPrefs.begin("fightstick", false);
    ledPrefs.putUInt(ledChannels[i].prefKeyColor, packed);
    ledPrefs.end();
}

static bool isLedIndexAllowed(int idx) {
    return (idx >= 0 && idx <= LED_COUNT - LEDS_PER_BUTTON && idx % 2 == 0);
}

void loadLedConfigFromPrefs() {
    ledPrefs.begin("fightstick", false);
    for (uint8_t i = 0; i < NUM_LED_CHANNELS; i++) {
        ledChannels[i].ledIndex = ledPrefs.getUChar(ledChannels[i].prefKeyIndex, ledChannels[i].ledIndex);
        if (ledChannels[i].colorConfigurable) {
            uint32_t packed = ledPrefs.getUInt(ledChannels[i].prefKeyColor,
                ((uint32_t)ledChannels[i].r << 16) | ((uint32_t)ledChannels[i].g << 8) | ledChannels[i].b);
            ledChannels[i].r = (packed >> 16) & 0xFF;
            ledChannels[i].g = (packed >> 8) & 0xFF;
            ledChannels[i].b = packed & 0xFF;
        }
    }
    ledPrefs.end();
}

void setupLeds() {
    pixels.begin();
    pixels.setBrightness(80);
    pixels.clear();
    pixels.show();
}

void updateLedForChannel(const char* channelName, bool pressed) {
    for (uint8_t i = 0; i < NUM_LED_CHANNELS; i++) {
        if (strcmp(ledChannels[i].name, channelName) == 0) {
            uint32_t color = pressed
                ? pixels.Color(ledChannels[i].r, ledChannels[i].g, ledChannels[i].b, 0)
                : pixels.Color(0, 0, 0, 0);
            pixels.setPixelColor(ledChannels[i].ledIndex, color);
            pixels.setPixelColor(ledChannels[i].ledIndex + 1, color);
            return;
        }
    }
}

static void handleLedChannels() {
    // La reference server est capturee via la lambda dans setupLedWebRoutes
}

void setupLedWebRoutes(WebServer& server) {
    server.on("/ledchannels", HTTP_GET, [&server]() {
        String json = "[";
        for (uint8_t i = 0; i < NUM_LED_CHANNELS; i++) {
            json += "{\"name\":\"" + String(ledChannels[i].name) + "\"";
            json += ",\"ledIndex\":" + String(ledChannels[i].ledIndex);
            json += ",\"colorConfigurable\":" + String(ledChannels[i].colorConfigurable ? "true" : "false");
            char hexColor[8];
            sprintf(hexColor, "#%02X%02X%02X", ledChannels[i].r, ledChannels[i].g, ledChannels[i].b);
            json += ",\"color\":\"" + String(hexColor) + "\"}";
            if (i < NUM_LED_CHANNELS - 1) json += ",";
        }
        json += "]";
        server.send(200, "application/json", json);
    });

    server.on("/ledconfig", HTTP_POST, [&server]() {
        if (!server.hasArg("plain")) {
            server.send(400, "text/plain", "Body manquant");
            return;
        }
        String body = server.arg("plain");

        int newIndexes[NUM_LED_CHANNELS];
        String newColors[NUM_LED_CHANNELS];

        for (uint8_t i = 0; i < NUM_LED_CHANNELS; i++) {
            newIndexes[i] = ledChannels[i].ledIndex;
            newColors[i] = "";

            String keyIdx = "\"idx" + String(i) + "\":";
            int idxPos = body.indexOf(keyIdx);
            if (idxPos != -1) {
                int start = idxPos + keyIdx.length();
                int end = body.indexOf(',', start);
                int endBrace = body.indexOf('}', start);
                if (end == -1 || (endBrace != -1 && endBrace < end)) end = endBrace;
                newIndexes[i] = body.substring(start, end).toInt();
            }

            if (ledChannels[i].colorConfigurable) {
                String keyCol = "\"col" + String(i) + "\":\"";
                int colPos = body.indexOf(keyCol);
                if (colPos != -1) {
                    int start = colPos + keyCol.length();
                    int end = body.indexOf('"', start);
                    newColors[i] = body.substring(start, end);
                }
            }
        }

        for (uint8_t i = 0; i < NUM_LED_CHANNELS; i++) {
            if (!isLedIndexAllowed(newIndexes[i])) {
                server.send(400, "text/plain", "Index LED invalide pour " + String(ledChannels[i].name) + ": " + String(newIndexes[i]));
                return;
            }
        }

        for (uint8_t i = 0; i < NUM_LED_CHANNELS; i++) {
            for (uint8_t j = i + 1; j < NUM_LED_CHANNELS; j++) {
                bool overlap = !(newIndexes[i] + 1 < newIndexes[j] || newIndexes[j] + 1 < newIndexes[i]);
                if (overlap) {
                    server.send(400, "text/plain", "Conflit LED: " + String(ledChannels[i].name) + " et " + String(ledChannels[j].name));
                    return;
                }
            }
        }

        for (uint8_t i = 0; i < NUM_LED_CHANNELS; i++) {
            if (ledChannels[i].ledIndex != newIndexes[i]) {
                ledChannels[i].ledIndex = newIndexes[i];
                saveLedIndexToPrefs(i);
            }
            if (ledChannels[i].colorConfigurable && newColors[i].length() == 7 && newColors[i][0] == '#') {
                long colorVal = strtol(newColors[i].substring(1).c_str(), NULL, 16);
                ledChannels[i].r = (colorVal >> 16) & 0xFF;
                ledChannels[i].g = (colorVal >> 8) & 0xFF;
                ledChannels[i].b = colorVal & 0xFF;
                saveLedColorToPrefs(i);
            }
        }

        server.send(200, "text/plain", "OK");
    });
}