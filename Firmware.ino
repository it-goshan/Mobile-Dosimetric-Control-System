#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>
#include <SPI.h>
#include <SD.h>

#define GEIGER_PIN D2
#define GPS_RX D4
#define GPS_TX -1
#define SD_CS D0

const char* ssid = "Your_SSID";
const char* password = "Your_PASSWORD";

const char* server = "http://server.com/data?d=";

SoftwareSerial gpsSerial(GPS_RX, GPS_TX);
TinyGPSPlus gps;

volatile unsigned long pulseCount = 0;
unsigned long lastMeasureTime = 0;
unsigned int uSvPerHour = 0;

void ICACHE_RAM_ATTR countPulse() {
  pulseCount++;
}

void setup() {
  Serial.begin(9600);
  gpsSerial.begin(9600);

  pinMode(GEIGER_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(GEIGER_PIN), countPulse, FALLING);

  if (SD.begin(SD_CS)) {
    Serial.println("SD card OK.");
  } else {
    Serial.println("SD init failed.");
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected.");
  } else {
    Serial.println("\nWi-Fi not available. Using SD fallback.");
  }

  lastMeasureTime = millis();
}

void loop() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastMeasureTime >= 10000) {
    detachInterrupt(digitalPinToInterrupt(GEIGER_PIN));
    unsigned long counts = pulseCount;
    pulseCount = 0;
    lastMeasureTime = currentMillis;

    uSvPerHour = counts * 0.0057;

    String gpsData = gps.location.isValid()
      ? String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6)
      : "0.000000,0.000000";

    String dataString = String(counts) + "," + String(uSvPerHour) + "," + gpsData;
    Serial.println("Data: " + dataString);

    if (WiFi.status() == WL_CONNECTED) {
      sendToServer(dataString);
    } else {
      saveToSD(dataString);
    }

    attachInterrupt(digitalPinToInterrupt(GEIGER_PIN), countPulse, FALLING);
  }
}

void sendToServer(String data) {
  WiFiClient client;
  String url = String(server) + data;
  Serial.println("Requesting URL: " + url);

  if (client.connect("yourserver.com", 80)) {
    client.print(String("GET ") + "/data?d=" + data + " HTTP/1.1\r\n" +
                 "Host: yourserver.com\r\n" +
                 "Connection: close\r\n\r\n");
    delay(1000);
    while (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    }
    client.stop();
  } else {
    Serial.println("Connection failed.");
  }
}

void saveToSD(String data) {
  File dataFile = SD.open("log.txt", FILE_WRITE);
  if (dataFile) {
    dataFile.println(data);
    dataFile.close();
    Serial.println("Saved to SD.");
  } else {
    Serial.println("SD write failed.");
  }
}
