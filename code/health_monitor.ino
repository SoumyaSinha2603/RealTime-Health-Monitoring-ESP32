#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"

#define DHTPIN 21
#define DHTTYPE DHT11
#define ONE_WIRE_BUS 5
#define SDA_PIN 22
#define SCL_PIN 23

DHT dht(DHTPIN, DHTTYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
PulseOximeter pox;

const char* ssid = "iPhone 13 Pro ";
const char* password = "Soumya0362";
const char* thingSpeakApiKey = "CX7ZZOHK58Y1F4DJ";

String patientID = "";

float bpm = 0;
float spo2 = 0;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ Connected to WiFi");

  dht.begin();
  sensors.begin();
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!pox.begin()) {
    Serial.println("❌ MAX30100 not found!");
  } else {
    Serial.println("✅ MAX30100 initialized");
    // pox.setIRLedCurrent(MAX30100::LED_CURR_11MA); // optional
  }
}

void readPulseData() {
  Serial.println("Place finger on MAX30100 sensor...");
  unsigned long start = millis();
  unsigned long duration = 10000; // Collect for 10 seconds

  float totalBPM = 0, totalSpO2 = 0;
  int count = 0;

  while (millis() - start < duration) {
    pox.update();

    float currentBPM = pox.getHeartRate();
    float currentSpO2 = pox.getSpO2();

    if (currentBPM > 30 && currentBPM < 180 && currentSpO2 > 70) {
      totalBPM += currentBPM;
      totalSpO2 += currentSpO2;
      count++;
    }
    delay(100); // Non-blocking
  }

  if (count > 0) {
    bpm = totalBPM / count;
    spo2 = totalSpO2 / count;
  } else {
    bpm = 0;
    spo2 = 0;
  }

  Serial.print("BPM: "); Serial.println(bpm);
  Serial.print("SpO2: "); Serial.println(spo2);
}

void loop() {
  Serial.println("\nEnter Patient ID: ");
  while (Serial.available() == 0) {}
  patientID = Serial.readStringUntil('\n');
  patientID.trim();

  readPulseData();

  float roomTemp = dht.readTemperature();
  float humidity = dht.readHumidity();

  sensors.requestTemperatures();
  float bodyTemp = sensors.getTempCByIndex(0);

  // --- Debug Output ---
  Serial.println("----- Sensor Readings -----");
  Serial.print("Room Temp: "); Serial.println(roomTemp);
  Serial.print("Body Temp: "); Serial.println(bodyTemp);
  Serial.print("Humidity : "); Serial.println(humidity);
  Serial.print("BPM       : "); Serial.println(bpm);
  Serial.print("SpO2      : "); Serial.println(spo2);
  Serial.println("----------------------------");

  if (isnan(roomTemp) || isnan(humidity) || bodyTemp == -127.0 || bpm == 0 || spo2 == 0) {
    Serial.println("❌ Invalid sensor data. Skipping upload.\n");
    return;
  }

  HTTPClient http;
  String url = "http://api.thingspeak.com/update?api_key=" + String(thingSpeakApiKey)
               + "&field1=" + String(roomTemp)
               + "&field2=" + String(bodyTemp)
               + "&field3=" + String(humidity)
               + "&field4=" + String(spo2)
               + "&field5=" + String(bpm)
               + "&field6=" + String(patientID);

  http.begin(url);
  int response = http.GET();
  if (response > 0) {
    Serial.println("✅ Data uploaded to ThingSpeak!");
  } else {
    Serial.println("❌ Upload failed.");
  }
  http.end();

  Serial.println("✅ Ready for next patient.\n");
  delay(5000);
}
