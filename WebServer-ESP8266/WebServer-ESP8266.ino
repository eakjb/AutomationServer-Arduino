#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <ArduinoJson.h>

const char* ssid = "MegaBoss";
const char* password = "Lizards2";

#define INPUTS true
#define OUTPUTS false

//-----DHT-----
#define ENABLE_DHT true
#if ENABLE_DHT

#include "DHT.h"

#define DHTPIN 2     // what pin we're connected to
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
#endif
//-----END DHT-----

MDNSResponder mdns;

ESP8266WebServer server(82);

const int led = 0;

void setup(void) {
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);


  Serial.begin(115200);
  WiFi.begin(ssid, password);
  WiFi.config(IPAddress(192, 168, 2, 61), IPAddress(192, 168, 2, 1), IPAddress(255, 255, 255, 0));
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (mdns.begin("esp8266", WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }

  //---REGISTER---
  Serial.println("Registering...");
  WiFiClient client;
  char host[] = "192.168.2.50";
  const int httpPort = 80;
  if (client.connect(host, httpPort)) {
    String url = "/api/v1/Nodes";
    char payload[] = "{ \"name\": \"ESP8266_Sensor1\", \"address\": \"http://192.168.0.137:82\" }\r";
    client.print(String("POST ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Content-Type: application/json\r\n" +
                 "Content-Length: " + String(strlen(payload)) + "\r\n" +
                 "Connection: close\r\n\r\n" + payload + "\r\n");
    // Read all the lines of the reply from server and print them to Serial
    while (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    Serial.println("Registered.");
  } else {
    Serial.println("connection failed");
  }
  //---END REGISTER---


  server.on("/", []() {
    const int bufSize = 500;
    StaticJsonBuffer<bufSize> buf;
    JsonObject& data = buf.createObject();

    data["display_name"] = "ESP8266_Sensor1";
    data["id"] = 1;

#if INPUTS
    data["inputs"] = true;
#endif
#if OUTPUTS
    data["outputs"] = true;
#endif

    char b[bufSize];
    data.prettyPrintTo(b, bufSize);
    server.send(200, "application/json", b);
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "Path Not Found.");
  });

#if INPUTS
  server.on("/inputs", []() {
    const int bufSize = 800;
    StaticJsonBuffer<bufSize> buf;
    JsonArray& data = buf.createArray();

#if ENABLE_DHT

    float h = dht.readHumidity();
    float c = dht.readTemperature();
    float f = dht.readTemperature(true);

    if (isnan(h) || isnan(c) || isnan(f)) {
      Serial.println("Failed to read from DHT sensor!");
      server.send(500, "text/plain", "Failed sensor read.");
    } else {
      JsonObject& temperatureF = data.createNestedObject();
      temperatureF["value"] = f;
      temperatureF["display_name"] = "Temperature (°F)";
      temperatureF["unit"] = "Degrees Fahrenheit";
      temperatureF["unit_abbreviation"] = "°F";
      temperatureF["min"] = -20;
      temperatureF["max"] = 120;

      JsonObject& temperatureC = data.createNestedObject();
      temperatureC["value"] = c;
      temperatureC["display_name"] = "Temperature (°C)";
      temperatureC["unit"] = "Degrees Celsius";
      temperatureC["unit_abbreviation"] = "°C";
      temperatureC["min"] = -29;
      temperatureC["max"] = 49;

      JsonObject& humidity = data.createNestedObject();
      humidity["value"] = h;
      humidity["display_name"] = "Humidity (%)";
      humidity["unit"] = "Percentage";
      humidity["unit_abbreviation"] = "%";
      humidity["min"] = 0;
      humidity["max"] = 100;
#endif

      char b[bufSize];
      data.prettyPrintTo(b, bufSize);
      server.send(200, "application/json", b);

#if ENABLE_DHT
    }
#endif

  });
#endif

  server.onNotFound([]() {
    server.send(404, "text/plain", "Path Not Found.");
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
}
