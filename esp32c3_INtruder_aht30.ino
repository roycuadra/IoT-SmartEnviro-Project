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
const int buzzerPin = 1; // GPIO1 for the passive buzzer
const int ledPin = 4;               // Pin connected to the LED
const int sensorPin = 5;            // Pin connected to the sensor
const unsigned long motionTimeout = 2000; 

unsigned long lastMotionTime = 0;   
bool motionDetected = false;        

// Function to pulse the buzzer tone
void playBuzzerPulse() {
    for (int i = 0; i < 5; i++) { 
        tone(buzzerPin, 3000); 
        delay(100);            
        noTone(buzzerPin);     
        delay(100);            
    }
}

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

Adafruit_AHTX0 aht;
bool sensorConnected = false;
String htmlContent;  

// Time tracking for AP client check
unsigned long lastClientCheckMillis = 0;
const long clientCheckInterval = 120000; // 2 minutes in milliseconds

// Load HTML file into memory once during setup to reduce SPIFFS access
void loadHTMLContent() {
    File file = SPIFFS.open("/index.html", "r");
    if (!file) {
        Serial.println("Failed to open index.html file");
        return;
    }
    htmlContent = file.readString();
    file.close();
}

// Handle root page request
void handle_OnConnect() {
    String tempHtml = htmlContent;
    tempHtml.replace("%TEMP%", "loading..");
    tempHtml.replace("%HUMIDITY%", "loading..");
    tempHtml.replace("%HEATINDEX%", "loading..");
    server.send(200, "text/html", tempHtml);

    // Reset the last client check time whenever a client connects
    lastClientCheckMillis = millis();
}

void handle_NotFound() {
    server.send(404, "text/plain", "Not found");
}

// Calculate heat index with optimized precomputed constants
float calculateHeatIndex(float temperature, float humidity) {
    if (humidity < 0 || humidity > 100 || temperature < -30 || temperature > 50) return NAN;

    float T = (temperature * 9.0 / 5.0) + 32;
    float HI = -42.379 + 2.04901523 * T +
               10.14333127 * humidity -
               0.22475541 * T * humidity -
               0.00683783 * T * T -
               0.05481717 * humidity * humidity +
               0.00122874 * T * T * humidity +
               0.00085282 * T * humidity * humidity -
               0.00000199 * T * T * humidity * humidity;

    return (HI - 32) * 5.0 / 9.0;
}

// Send data via WebSocket
void sendWebSocketUpdates(float temperature, float humidity) {
    float heatIndex = calculateHeatIndex(temperature, humidity);
    String json = "{\"temp\":\"" + String(temperature, 1) +
                  "\",\"humidity\":\"" + String(humidity, 1) +
                  "\",\"heatIndex\":\"" + String(heatIndex, 1) + "\"}";
    webSocket.broadcastTXT(json);
}

// Check sensor and send updates if values change
void checkEnvironmentSensor() {
    static unsigned long lastSensorCheck = 0;
    const unsigned long interval = 1000; // Poll every 1 second

    if (millis() - lastSensorCheck < interval) return;
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

// Check motion and control LED
void checkMotionSensor() {
    bool currentMotionState = digitalRead(sensorPin); // Read the current sensor value

    // If motion is detected
    if (currentMotionState == HIGH) {
        if (!motionDetected) {
            // First time detecting motion, log and turn the LED on
            Serial.println("Motion detected!");
            motionDetected = true;
            lastMotionTime = millis(); // Store the time when motion was first detected
            digitalWrite(ledPin, HIGH); // Turn the LED on
            playBuzzerPulse(); // Play pulsing buzzer tone
        }
    } else {
        // Motion is no longer detected, check if motion stopped long enough
        if (motionDetected && millis() - lastMotionTime >= motionTimeout) {
            Serial.println("Motion stopped!");
            motionDetected = false; 
            digitalWrite(ledPin, LOW); 
            noTone(buzzerPin); 
        }
    }
}

void setup() {
    Serial.begin(115200);

    // Setup pins
    pinMode(ledPin, OUTPUT);
    pinMode(sensorPin, INPUT);
    pinMode(buzzerPin, OUTPUT);

    Serial.println("Starting fresh...");

    WiFi.softAP(apSSID, apPassword);
    Serial.println("Access Point started");
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());

    if (!SPIFFS.begin(true)) {
        Serial.println("An error has occurred while mounting SPIFFS");
        return;
    }

    loadHTMLContent();  // Load and cache HTML content

    sensorConnected = aht.begin();
    if (!sensorConnected) {
        Serial.println("Could not find AHT30 sensor!");
    } else {
        Serial.println("AHT30 sensor initialized successfully.");
    }

    server.on("/", handle_OnConnect);
    server.onNotFound(handle_NotFound);
    server.begin();
    webSocket.begin();

    Serial.println("HTTP and WebSocket servers started");
}

void loop() {
    unsigned long currentMillis = millis();
    server.handleClient();
    webSocket.loop();
    checkEnvironmentSensor();
    checkMotionSensor();

    // Check for connected clients
    if (WiFi.softAPgetStationNum() > 0) {
        lastClientCheckMillis = currentMillis; 
    } else if (currentMillis - lastClientCheckMillis >= clientCheckInterval) {
        WiFi.softAPdisconnect(true); 
        Serial.println("No clients connected for 2 minutes. AP turned off.");
    }
}
