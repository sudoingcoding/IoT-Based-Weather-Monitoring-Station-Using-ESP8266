#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <MQ135.h>
#include <ArduinoJson.h>

/* ================= WIFI & THINGSBOARD ================= */
#define WIFI_SSID     "Suddip"
#define WIFI_PASS     "*****"

#define TB_SERVER     "192.168.0.100"   // PC IP running ThingsBoard
#define TB_PORT       1883
#define DEVICE_TOKEN  "xHC8Py0pufrZ1c0zrcmP"

/* ================= SENSOR PINS ================= */
#define DHT_PIN   2     // D4
#define DHT_TYPE  DHT22
#define MQ135_PIN A0
#define RAIN_PIN  14    // D5

/* ================= OBJECTS ================= */
DHT dht(DHT_PIN, DHT_TYPE);
MQ135 mq135(MQ135_PIN);
WiFiClient espClient;
PubSubClient client(espClient);

/* ================= TIMING ================= */
unsigned long lastSend = 0;
const unsigned long sendInterval = 10000; // 10 seconds

/* ================= WIFI CONNECT ================= */
void setupWiFi() {
  Serial.print("Connecting to WiFi ");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  WiFi.setSleepMode(WIFI_NONE_SLEEP);   // IMPORTANT for MQTT stability
  Serial.println("\nWiFi connected");
  Serial.print("ESP8266 IP: ");
  Serial.println(WiFi.localIP());
}

/* ================= MQTT RECONNECT ================= */
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to ThingsBoard at ");
    Serial.println(TB_SERVER);

    String clientId = "ESP8266-" + String(ESP.getChipId());

    if (client.connect(clientId.c_str(), DEVICE_TOKEN, NULL)) {
      Serial.println("✅ Connected to ThingsBoard");
    } else {
      Serial.print("❌ Failed, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  delay(100);

  setupWiFi();

  client.setServer(TB_SERVER, TB_PORT);
  client.setKeepAlive(60);   // MQTT keepalive

  dht.begin();
  pinMode(RAIN_PIN, INPUT);
}

/* ================= LOOP ================= */
void loop() {
  if (!client.connected()) {
    reconnect();
  }

  client.loop();   // REQUIRED for MQTT

  if (millis() - lastSend > sendInterval) {
    lastSend = millis();

    float temperature = dht.readTemperature();
    float humidity    = dht.readHumidity();
    float airQuality  = mq135.getPPM();
    int rain           = digitalRead(RAIN_PIN);

    Serial.println("\n=== Sensor Readings ===");
    Serial.printf("Temperature : %.2f °C\n", temperature);
    Serial.printf("Humidity    : %.2f %%\n", humidity);
    Serial.printf("Air Quality : %.2f ppm\n", airQuality);
    Serial.print("Rain        : ");
    Serial.println(rain ? "Dry" : "Wet");
    Serial.println("======================");

    StaticJsonDocument<256> doc;
    if (!isnan(temperature)) doc["temperature"] = temperature;
    if (!isnan(humidity))    doc["humidity"] = humidity;
    doc["airQuality"] = airQuality;
    doc["rain"] = rain;

    char payload[256];
    serializeJson(doc, payload);

    bool published = client.publish("v1/devices/me/telemetry", payload);
    Serial.print("Published to ThingsBoard: ");
    Serial.println(published ? "Success ✅" : "Failed ❌");
  }
}
