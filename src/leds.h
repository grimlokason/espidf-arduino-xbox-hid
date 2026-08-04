#ifndef LEDS_H
#define LEDS_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WebServer.h>

#define LED_PIN 3           // TODO: ta vraie pin (broche DIN de la chaine)
#define LED_COUNT 16
#define LEDS_PER_BUTTON 2

extern Adafruit_NeoPixel pixels;

void setupLeds();
void loadLedConfigFromPrefs();
void updateLedForChannel(const char* channelName, bool pressed);
void setupLedWebRoutes(WebServer& server);

#endif