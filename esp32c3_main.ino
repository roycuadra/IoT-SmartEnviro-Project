#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_AHTX0.h>
#include <Wire.h>
#include <WebSocketsServer.h>
#include <SPIFFS.h>

// Wi-Fi AP configuration
const char* apSSID = "IOT SmartEnviro";
const char* apPassword = "cuadra12345";

// Pins for motion detection
const int buzzerPin = 1; 
const int ledPin = 4;
const int sensorPin = 5;
const unsigned long motionTimeout = 2000;

WebServer server(80);
WebSocketsServer webSocket(81);

Adafruit_AHTX0 aht;
bool sensorConnected = false;
String htmlContent;

// Timing constants
const unsigned long clientCheckInterval = 120000;
const unsigned long sensorPollInterval = 1000;

// Timing variables
unsigned long lastClientCheckMillis = 0;
unsigned long lastSensorCheck = 0;
unsigned long lastMotionTime = 0;

// Motion state
bool motionDetected = false;
bool apActive = true;

// Load HTML content once during setup
void loadHTMLContent() {
    File file = SPIFFS.open("/index.html", "r");
    if (file) {
        htmlContent = file.readString();
        file.close();
    } else {
        Serial.println("Failed to open index.html file");
    }
}

// Handle root page request
void handle_OnConnect() {
    String tempHtml = htmlContent;
    tempHtml.replace("%TEMP%", "loading..");
    tempHtml.replace("%HUMIDITY%", "loading..");
    tempHtml.replace("%HEATINDEX%", "loading..");
    server.send(200, "text/html", tempHtml);

    lastClientCheckMillis = millis(); // Reset client check timer
}

void handle_NotFound() {
    server.send(404, "text/plain", "Not found");
}

// Calculate heat index
float calculateHeatIndex(float temperature, float humidity) {
    if (humidity < 0 || humidity > 100 || temperature < -30 || temperature > 50) return NAN;

    float T = (temperature * 9.0 / 5.0) + 32;
    float HI = -42.379 + 2.04901523 * T + 10.14333127 * humidity 
               - 0.22475541 * T * humidity - 0.00683783 * T * T 
               - 0.05481717 * humidity * humidity 
               + 0.00122874 * T * T * humidity 
               + 0.00085282 * T * humidity * humidity 
               - 0.00000199 * T * T * humidity * humidity;

    return (HI - 32) * 5.0 / 9.0;
}

// Send updates via WebSocket
void sendWebSocketUpdates(float temperature, float humidity) {
    float heatIndex = calculateHeatIndex(temperature, humidity);
    String json = "{\"temp\":\"" + String(temperature, 1) + "\","
                  "\"humidity\":\"" + String(humidity, 1) + "\","
                  "\"heatIndex\":\"" + String(heatIndex, 1) + "\"}";
    webSocket.broadcastTXT(json);
}

// Monitor environment sensor
void checkEnvironmentSensor() {
    if (millis() - lastSensorCheck < sensorPollInterval) return;
    lastSensorCheck = millis();

    if (!sensorConnected) return;

    sensors_event_t humidityEvent, temperatureEvent;
    aht.getEvent(&humidityEvent, &temperatureEvent);

    if (isnan(temperatureEvent.temperature) || isnan(humidityEvent.relative_humidity)) {
        Serial.println("Failed to read from AHT30 sensor!");
        return;
    }

    static float lastTemperature = NAN;
    static float lastHumidity = NAN;

    if (temperatureEvent.temperature != lastTemperature || humidityEvent.relative_humidity != lastHumidity) {
        lastTemperature = temperatureEvent.temperature;
        lastHumidity = humidityEvent.relative_humidity;
        sendWebSocketUpdates(lastTemperature, lastHumidity);
    }
}

// Play buzzer pulse
void playBuzzerPulse() {
    for (int i = 0; i < 5; i++) {
        tone(buzzerPin, 3000);
        delay(100);
        noTone(buzzerPin);
        delay(100);
    }
}

// Check motion sensor
void checkMotionSensor() {
    bool currentMotionState = digitalRead(sensorPin);

    if (currentMotionState == HIGH && !motionDetected) {
        Serial.println("Motion detected!");
        motionDetected = true;
        lastMotionTime = millis();
        digitalWrite(ledPin, HIGH);
        playBuzzerPulse();
    }

    if (motionDetected && millis() - lastMotionTime >= motionTimeout) {
        Serial.println("Motion stopped!");
        motionDetected = false;
        digitalWrite(ledPin, LOW);
        noTone(buzzerPin);
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(ledPin, OUTPUT);
    pinMode(sensorPin, INPUT);
    pinMode(buzzerPin, OUTPUT);

    WiFi.softAP(apSSID, apPassword);
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed");
        return;
    }

    loadHTMLContent();
    sensorConnected = aht.begin();
    Serial.println(sensorConnected ? "AHT30 sensor initialized" : "AHT30 sensor not found");

    server.on("/", handle_OnConnect);
    server.onNotFound(handle_NotFound);
    server.begin();
    webSocket.begin();
    Serial.println("HTTP and WebSocket servers started");
}

void loop() {
    unsigned long currentMillis = millis();

    if (apActive) {
        server.handleClient();
        webSocket.loop();
        checkEnvironmentSensor();
        checkMotionSensor();

        if (WiFi.softAPgetStationNum() > 0) {
            lastClientCheckMillis = currentMillis;
        } else if (currentMillis - lastClientCheckMillis >= clientCheckInterval) {
            WiFi.softAPdisconnect(true);
            Serial.println("No clients for 2 minutes. AP turned off.");
            apActive = false;
        }
    }
}
