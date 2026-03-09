#include <Servo.h>
#include <DHT.h>
#include <SoftwareSerial.h>

// ---------- PIN ASSIGNMENTS ----------
#define LDRLeftPin A0
#define LDRRightPin A1
#define VoltageSensorPin A2
#define TDSPin A3
#define DHTPin 7
#define BulbPin 8
#define ServoPin 9

#define ESP_RX 2  // Arduino RX (to ESP8266 TX)
#define ESP_TX 3  // Arduino TX (to ESP8266 RX via voltage divider)

Servo sunServo;
int servoPos = 90; // Center position

#define DHTTYPE DHT11
DHT dht(DHTPin, DHTTYPE);

SoftwareSerial espSerial(ESP_RX, ESP_TX);

// ---------- WIFI CONFIGURATION ----------
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
#define TS_APIKEY "your_thingspeak_api_key" // Replace with your ThingSpeak API key

// ---------- PROJECT PARAMETERS ----------
const float bulbVoltageThreshold = 1.0;  // Volts, set according to your setup
const int ldrSensitivity = 50;            // LDR difference sensitivity for tracking

void setup() {
  Serial.begin(9600);
  espSerial.begin(115200); // ESP-01 default baud rate

  pinMode(BulbPin, OUTPUT);
  sunServo.attach(ServoPin);
  sunServo.write(servoPos);
  dht.begin();
  delay(2000); // Allow sensors & ESP8266 to stabilize

  // ESP8266 WiFi Setup
  espSerial.println("AT+RST");   // Reset ESP-01
  delay(2000);
  espSerial.println("AT+CWMODE=1"); // Station mode
  delay(1000);
  espSerial.print("AT+CWJAP=\"");
  espSerial.print(WIFI_SSID);
  espSerial.print("\",\"");
  espSerial.print(WIFI_PASSWORD);
  espSerial.println("\"");
  delay(7000); // Allow WiFi to connect
}

// Upload sensor data every 60 seconds
unsigned long lastUpload = 0;
const unsigned long uploadInterval = 60000;

void loop() {
  // --- LDR Sun Tracking ---
  int ldrLeft = analogRead(LDRLeftPin);
  int ldrRight = analogRead(LDRRightPin);
  int diff = ldrLeft - ldrRight;

  if (diff > ldrSensitivity && servoPos > 0)
    servoPos -= 1;
  else if (diff < -ldrSensitivity && servoPos < 180)
    servoPos += 1;
  sunServo.write(servoPos);

  // --- Voltage Sensor ---
  int rawVoltage = analogRead(VoltageSensorPin);
  float panelVoltage = (rawVoltage / 1023.0) * 5.0 * 5.0; // Adjust multiplier to match sensor divider

  // --- Bulb Control ---
  if (panelVoltage >= bulbVoltageThreshold)
    digitalWrite(BulbPin, HIGH);
  else
    digitalWrite(BulbPin, LOW);

  // --- TDS Sensor ---
  int tdsRaw = analogRead(TDSPin);
  float tdsValue = (tdsRaw / 1023.0) * 5.0 * 1000; // Calibrate as needed

  // --- DHT11 Sensor ---
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // --- Serial Output (for debugging) ---
  Serial.print("Panel V: "); Serial.print(panelVoltage, 2); Serial.print(" | ");
  Serial.print("Bulb: "); Serial.print((panelVoltage >= bulbVoltageThreshold) ? "ON" : "OFF"); Serial.print(" | ");
  Serial.print("Servo: "); Serial.print(servoPos); Serial.print(" | ");
  Serial.print("TDS: "); Serial.print(tdsValue, 0); Serial.print(" | ");
  if (isnan(temperature) || isnan(humidity)) {
    Serial.print("Temp: -- C, Humidity: -- %");
  } else {
    Serial.print("Temp: "); Serial.print(temperature, 1); Serial.print("C, ");
    Serial.print("Humidity: "); Serial.print(humidity, 1); Serial.print("%");
  }
  Serial.println();

  // --- WiFi Data Upload Section ---
  if (millis() - lastUpload > uploadInterval) {
    lastUpload = millis();

    // Format GET request for ThingSpeak (adjust for your endpoint)
    String getStr = "GET /update?api_key=";
    getStr += TS_APIKEY;
    getStr += "&field1=" + String(panelVoltage, 2);
    getStr += "&field2=" + String(tdsValue, 0);
    getStr += "&field3=" + String(temperature, 1);
    getStr += "&field4=" + String(humidity, 1);
    getStr += " HTTP/1.1\r\nHost: api.thingspeak.com\r\nConnection: close\r\n\r\n";

    espSerial.println("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80");
    delay(2000);

    espSerial.print("AT+CIPSEND=");
    espSerial.println(getStr.length());
    delay(200);

    espSerial.print(getStr);
    delay(4500); // Wait for data to send
    espSerial.println("AT+CIPCLOSE");
    delay(500);
  }

  delay(200); // Smooth servo movement
}
