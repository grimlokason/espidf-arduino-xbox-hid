#include <BleCompositeHID.h>
#include <XboxGamepadDevice.h>

static const uint8_t LED_PIN = 5;
static XboxSeriesXControllerDeviceConfiguration gamepadConfig;
static XboxGamepadDevice gamepad(&gamepadConfig);
static BleCompositeHID compositeHID("ESP32 SeriesX Controller", "Mystfit", 100);

void setup()
{
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    BLEHostConfiguration hostConfig = gamepadConfig.getIdealHostConfiguration();

    compositeHID.addDevice(&gamepad);
    compositeHID.begin(hostConfig);

    Serial.printf("HID started (VID=0x%04X PID=0x%04X)\n", hostConfig.getVid(), hostConfig.getPid());
}

void loop()
{
    if (compositeHID.isConnected()) {
        digitalWrite(LED_PIN, LOW);
    } else {
        digitalWrite(LED_PIN, HIGH);
    }
    delay(20);
}
