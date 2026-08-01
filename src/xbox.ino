#include <Arduino.h>
#include <BleCompositeHID.h>
#include <XboxGamepadDevice.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#include "webpage.h"

BleCompositeHID compositeHID("ESP32 SeriesX Controller", "Mystfit", 100);
XboxGamepadDevice* gamepad;
WebServer server(80);
Preferences prefs;

// ===================== WIFI AP =====================
const char* AP_SSID = "Fightstick-Config";
const char* AP_PASS = "config1234"; // min 8 caracteres

// ===================== PINS UTILISABLES =====================
// A verifier/ajuster selon le pinout exact de ton module ESP32-C6.
// GPIO16/17 exclus (UART console).
const uint8_t ALLOWED_PINS[] = {0,1,2,3,6,7,8,9,10,11,12,13,14,15,18,19,20,21,22,23};
const uint8_t NUM_ALLOWED_PINS = sizeof(ALLOWED_PINS) / sizeof(ALLOWED_PINS[0]);

// ===================== CANAUX (boutons + hat + gachettes) =====================
enum ChannelType { CH_BUTTON, CH_HAT, CH_TRIGGER };

struct Channel {
    const char* name;
    const char* prefKey;   // cle courte pour Preferences (max 15 car.)
    ChannelType type;
    uint16_t xboxCode;     // XBOX_BUTTON_* ou index hat (0=UP,1=LEFT,2=DOWN,3=RIGHT) ou 0/1 pour LT/RT
    uint8_t pin;           // pin actuellement assignee (chargee depuis prefs)
    bool currentState;     // true = appuye
    bool previousState;
};

Channel channels[] = {
    {"A",      "pA",  CH_BUTTON, XBOX_BUTTON_A,      0,  false, false},
    {"B",      "pB",  CH_BUTTON, XBOX_BUTTON_B,      1,  false, false},
    {"X",      "pX",  CH_BUTTON, XBOX_BUTTON_X,      2,  false, false},
    {"Y",      "pY",  CH_BUTTON, XBOX_BUTTON_Y,      3,  false, false},
    {"LB",     "pLB", CH_BUTTON, XBOX_BUTTON_LB,     6,  false, false},
    {"RB",     "pRB", CH_BUTTON, XBOX_BUTTON_RB,     7,  false, false},
    {"LS",     "pLS", CH_BUTTON, XBOX_BUTTON_LS,     8,  false, false},
    {"RS",     "pRS", CH_BUTTON, XBOX_BUTTON_RS,     9,  false, false},
    {"Start",  "pST", CH_BUTTON, XBOX_BUTTON_START,  10, false, false},
    {"Select", "pSE", CH_BUTTON, XBOX_BUTTON_SELECT, 11, false, false},
    {"Home",   "pHM", CH_BUTTON, XBOX_BUTTON_HOME,   12, false, false},
    {"Hat Up",    "pHU", CH_HAT, 0, 13, false, false},
    {"Hat Left",  "pHL", CH_HAT, 1, 14, false, false},
    {"Hat Down",  "pHD", CH_HAT, 2, 15, false, false},
    {"Hat Right", "pHR", CH_HAT, 3, 18, false, false},
    {"LT", "pLT", CH_TRIGGER, 0, 19, false, false},
    {"RT", "pRT", CH_TRIGGER, 1, 20, false, false},
};
const uint8_t NUM_CHANNELS = sizeof(channels) / sizeof(channels[0]);

// ===================== PREFERENCES (persistance des pins) =====================

void loadPinsFromPrefs() {
    prefs.begin("fightstick", false); // ecriture -> cree le namespace au 1er boot si absent
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        channels[i].pin = prefs.getUChar(channels[i].prefKey, channels[i].pin);
    }
    prefs.end();
}

void savePinToPrefs(uint8_t channelIndex) {
    prefs.begin("fightstick", false); // ecriture
    prefs.putUChar(channels[channelIndex].prefKey, channels[channelIndex].pin);
    prefs.end();
}

// ===================== SETUP DES PINS =====================

void setupChannelPins() {
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        pinMode(channels[i].pin, INPUT_PULLUP);
        channels[i].currentState = false;
        channels[i].previousState = false;
    }
}

// ===================== COMBO D'ACTIVATION DU MODE CONFIG =====================
// Mets ici les noms des boutons a maintenir ensemble au demarrage pour activer le WiFi.
// Doit correspondre exactement au champ "name" d'un canal dans channels[].
// Exemples : {"Select", "Home"}, {"LB", "RB"}, {"Start", "Select", "Home"} (3+ boutons aussi possible)
const char* CONFIG_MODE_COMBO[] = {"Start", "Home"};
const uint8_t CONFIG_MODE_COMBO_SIZE = sizeof(CONFIG_MODE_COMBO) / sizeof(CONFIG_MODE_COMBO[0]);

// ===================== VARIABLE GLOBALE MODE CONFIG =====================
bool configModeActive = false;

// ===================== DETECTION DE LA COMBO AU BOOT =====================

bool checkConfigModeCombo() {
    // Petite pause pour laisser le temps physique d'appuyer/stabiliser les entrees
    delay(150);

    for (uint8_t c = 0; c < CONFIG_MODE_COMBO_SIZE; c++) {
        bool found = false;
        for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
            if (strcmp(channels[i].name, CONFIG_MODE_COMBO[c]) == 0) {
                found = true;
                if (digitalRead(channels[i].pin) != LOW) {
                    return false; // ce bouton de la combo n'est pas appuye
                }
                break;
            }
        }
        if (!found) {
            Serial.println("ATTENTION: bouton '" + String(CONFIG_MODE_COMBO[c]) + "' introuvable dans channels[]");
            return false;
        }
    }
    return true; // tous les boutons de la combo sont appuyes
}

// ===================== WEB SERVER =====================

void handleRoot() {
    server.send_P(200, "text/html", PAGE_HTML);
}

void handleChannels() {
    String json = "{\"allowedPins\":[";
    for (uint8_t i = 0; i < NUM_ALLOWED_PINS; i++) {
        json += String(ALLOWED_PINS[i]);
        if (i < NUM_ALLOWED_PINS - 1) json += ",";
    }
    json += "],\"channels\":[";
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        json += "{\"name\":\"" + String(channels[i].name) + "\",\"pin\":" + String(channels[i].pin) + "}";
        if (i < NUM_CHANNELS - 1) json += ",";
    }
    json += "]}";
    server.send(200, "application/json", json);
}

void handleStatus() {
    String json = "[";
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        json += "{\"name\":\"" + String(channels[i].name) + "\",\"pin\":" + String(channels[i].pin) +
                ",\"pressed\":" + (channels[i].currentState ? "true" : "false") + "}";
        if (i < NUM_CHANNELS - 1) json += ",";
    }
    json += "]";
    server.send(200, "application/json", json);
}

bool isPinAllowed(uint8_t pin) {
    for (uint8_t i = 0; i < NUM_ALLOWED_PINS; i++) {
        if (ALLOWED_PINS[i] == pin) return true;
    }
    return false;
}

void handleConfig() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Body manquant");
        return;
    }
    String body = server.arg("plain");

    // 1. Parser toutes les nouvelles pins dans un tableau temporaire
    int newPins[NUM_CHANNELS];
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        newPins[i] = channels[i].pin; // valeur par defaut = pin actuelle, au cas ou absente du body

        String key = "\"ch" + String(i) + "\":";
        int idx = body.indexOf(key);
        if (idx == -1) continue;
        int start = idx + key.length();
        int end = body.indexOf(',', start);
        int endBrace = body.indexOf('}', start);
        if (end == -1 || endBrace < end) end = endBrace;
        newPins[i] = body.substring(start, end).toInt();
    }

    // 2. Valider que chaque nouvelle pin est autorisee
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        if (!isPinAllowed(newPins[i])) {
            server.send(400, "text/plain", "Pin invalide pour " + String(channels[i].name) + ": " + String(newPins[i]));
            return;
        }
    }

    // 3. Valider qu'il n'y a aucun conflit DANS L'ENSEMBLE DES NOUVELLES VALEURS
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        for (uint8_t j = i + 1; j < NUM_CHANNELS; j++) {
            if (newPins[i] == newPins[j]) {
                server.send(400, "text/plain", "Conflit: " + String(channels[i].name) + " et " + String(channels[j].name) + " utilisent tous les deux GPIO " + String(newPins[i]));
                return;
            }
        }
    }

    // 4. Tout est valide -> appliquer et sauvegarder uniquement ce qui a change
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        if (channels[i].pin != newPins[i]) {
            channels[i].pin = newPins[i];
            pinMode(channels[i].pin, INPUT_PULLUP);
            channels[i].currentState = false;
            channels[i].previousState = false;
            savePinToPrefs(i);
        }
    }

    server.send(200, "text/plain", "OK");
}

void setupWebServer() {
    WiFi.mode(WIFI_AP);
    WiFi.setTxPower(WIFI_POWER_8_5dBm); // au lieu du max par défaut (~20dBm)
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.println("Point d'acces WiFi demarre. SSID: " + String(AP_SSID));
    Serial.println("IP: " + WiFi.softAPIP().toString());

    server.on("/", handleRoot);
    server.on("/channels", handleChannels);
    server.on("/status", handleStatus);
    server.on("/config", HTTP_POST, handleConfig);
    server.begin();
}

// ===================== SETUP =====================

void setup()
{
    Serial.begin(115200);

    loadPinsFromPrefs();
    setupChannelPins(); // pinMode() doit etre fait AVANT de lire les boutons

    configModeActive = checkConfigModeCombo();

    XboxSeriesXControllerDeviceConfiguration* config = new XboxSeriesXControllerDeviceConfiguration();
    BLEHostConfiguration hostConfig = config->getIdealHostConfiguration();

    gamepad = new XboxGamepadDevice(config);
    gamepad->resetInputs();
    compositeHID.addDevice(gamepad);

    Serial.println("Starting composite HID device...");
    compositeHID.begin(hostConfig);

    if (configModeActive) {
        Serial.println("Mode configuration active");
        setupWebServer();
    } else {
        Serial.println("Mode jeu normal (WiFi desactive)");
    }
}

// ===================== SUIVI DE L'ETAT DE CONNEXION =====================
bool wasConnected = false;

// ===================== LOOP =====================

void loop()
{
    if (configModeActive) {
        server.handleClient();
    }

    bool isConnected = compositeHID.isConnected();

    // Detection du passage deconnecte -> connecte
    if (isConnected && !wasConnected) {
        Serial.println("Hote connecte, envoi de l'etat initial des sticks au centre");
        gamepad->setLeftThumb(0, 0);
        gamepad->setRightThumb(0, 0);
        gamepad->sendGamepadReport();
    }
    wasConnected = isConnected;

    if (!isConnected) {
        delay(5);
        return;
    }

    bool changed = false;
    uint8_t hatFlags = (uint8_t)XboxDpadFlags::NONE;
    bool hatUp = false, hatLeft = false, hatDown = false, hatRight = false;

    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        bool raw = (digitalRead(channels[i].pin) == LOW); // LOW = appuye (pull-up)
        channels[i].currentState = raw;

        if (channels[i].type == CH_HAT) {
            if (channels[i].xboxCode == 0) hatUp = raw;
            if (channels[i].xboxCode == 1) hatLeft = raw;
            if (channels[i].xboxCode == 2) hatDown = raw;
            if (channels[i].xboxCode == 3) hatRight = raw;
        }

        if (raw != channels[i].previousState) {
            channels[i].previousState = raw;
            changed = true;

            if (channels[i].type == CH_BUTTON) {
                if (raw) gamepad->press(channels[i].xboxCode);
                else gamepad->release(channels[i].xboxCode);
            } else if (channels[i].type == CH_TRIGGER) {
                if (channels[i].xboxCode == 0)
                    gamepad->setLeftTrigger(raw ? XBOX_TRIGGER_MAX : XBOX_TRIGGER_MIN);
                else
                    gamepad->setRightTrigger(raw ? XBOX_TRIGGER_MAX : XBOX_TRIGGER_MIN);
            }
        }
    }

    // Hat : directions opposees annulees
    if (hatUp && hatDown)    { hatUp = false; hatDown = false; }
    if (hatLeft && hatRight) { hatLeft = false; hatRight = false; }

    if (hatUp)    hatFlags |= (uint8_t)XboxDpadFlags::NORTH;
    if (hatDown)  hatFlags |= (uint8_t)XboxDpadFlags::SOUTH;
    if (hatLeft)  hatFlags |= (uint8_t)XboxDpadFlags::WEST;
    if (hatRight) hatFlags |= (uint8_t)XboxDpadFlags::EAST;

    static uint8_t lastHatFlags = 0xFF;
    if (hatFlags != lastHatFlags) {
        lastHatFlags = hatFlags;
        if (hatFlags == (uint8_t)XboxDpadFlags::NONE)
            gamepad->releaseDPad();
        else
            gamepad->pressDPadDirectionFlag((XboxDpadFlags)hatFlags);
        changed = true;
    }

    if (changed)
        gamepad->sendGamepadReport();

    delay(4);
}