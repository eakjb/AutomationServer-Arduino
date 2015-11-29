#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <ArduinoJson.h>

const char* ssid = "OrangeGiraffe";
const char* password = "Lizards1";

#define DEV true
#define INPUTS true
#define OUTPUTS true

#define ENABLE_NOTIFICATIONS true
#if ENABLE_NOTIFICATIONS

#define NOTIFICATION_DELAY 2200

#endif

//-----DIGITAL OUTPUT-----
#define ENABLE_DIGITAL_OUTPUTS true

#if ENABLE_DIGITAL_OUTPUTS

#define DIGITAL_OUTPUT_COUNT 2
const int digitalOutputs[DIGITAL_OUTPUT_COUNT] = {0,3};
char* digitalOutputsLabels[DIGITAL_OUTPUT_COUNT] = {"Pin 0","Pin 3"};
char* digitalOutputsOnLabels[DIGITAL_OUTPUT_COUNT] = {"On","1"};
char* digitalOutputsOffLabels[DIGITAL_OUTPUT_COUNT] = {"Off","2"};
int digitalOutputsStates[DIGITAL_OUTPUT_COUNT] = {0,0};

#endif

//-----DIGITAL INPUT-----
#define ENABLE_DIGITAL_INPUTS false

#if ENABLE_DIGITAL_INPUTS

#define DIGITAL_INPUT_COUNT 1
const int digitalInputs[DIGITAL_INPUT_COUNT] = {0};
char* digitalInputsLabels[DIGITAL_INPUT_COUNT] = {"Pin 0"};
char* digitalInputsOnLabels[DIGITAL_INPUT_COUNT] = {"On"};
char* digitalInputsOffLabels[DIGITAL_INPUT_COUNT] = {"Off"};
int digitalInputsStates[DIGITAL_INPUT_COUNT] = {0};

#endif

//-----DHT-----
#define ENABLE_DHT false
#if ENABLE_DHT

#include "DHT.h"

#define DHTPIN 4     // what pin we're connected to
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
#endif
//-----END DHT-----
//-----Ultrasonic-----
#define ENABLE_ULTRASONIC false
#if ENABLE_ULTRASONIC

#define TRIGGER 4
#define ECHO 5

#define CM_PER_MICROSECOND 0.034029
#endif
//-----END Ultrasonic-----

MDNSResponder mdns;

ESP8266WebServer server(80);

const char host[] = "192.168.0.50";
const int httpPort = 80;

void sendPayloadToServer(String payload, String path) {
  Serial.println("Sending payload...");
  WiFiClient client;
  if (client.connect(host, httpPort)) {
    client.print(String("POST ") + path + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Content-Type: application/json\r\n" +
                 "Content-Length: " + String(payload.length()) + "\r\n" +
                 "Connection: close\r\n\r\n" + payload + "\r\n");
    // Read all the lines of the reply from server and print them to Serial
    while (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    Serial.println("Done.");
  } else {
    Serial.println("Failed.");
  }
  client.stop();
  Serial.print(String("POST ") + path + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Content-Type: application/json\r\n" +
               "Content-Length: " + String(payload.length()) + "\r\n" +
               "Connection: close\r\n\r\n" + payload + "\r\n");
  delay(1000);
}

#if ENABLE_DIGITAL_OUTPUTS
void getOutput(JsonObject& output, int i) {
    int pin = digitalOutputs[i];
    JsonObject& states = output.createNestedObject("states");
    JsonObject& state = output.createNestedObject("state");

    JsonObject& onstate = states.createNestedObject("on");
    onstate["display_name"] = digitalOutputsOnLabels[i];
    onstate["is_on"] = true;
    onstate["value"] = 1;

    JsonObject& off = states.createNestedObject("off");
    off["display_name"] = digitalOutputsOffLabels[i];
    off["is_on"] = false;
    off["value"] = 0;

    state["display_name"] = digitalRead(digitalOutputs[i]) ? digitalOutputsOnLabels[i] : digitalOutputsOffLabels[i];
    state["is_on"] = digitalRead(digitalOutputs[i]) == 1;
    state["value"] = digitalRead(digitalOutputs[i]);

    output["display_name"] = digitalOutputsLabels[i];
    output["output_id"] = digitalOutputs[i];
    output["type"] = "switch";
}
#endif

void setup(void) {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  WiFi.config(IPAddress(192, 168, 0, 61), IPAddress(192, 168, 0, 1), IPAddress(255, 255, 255, 0));
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
  sendPayloadToServer("{ \"name\": \"ESP8266_Sensor1\", \"address\": \"http://192.168.0.61\" }\r"
                      , "/api/v1/Nodes");
  //---END REGISTER---

  bool pin0Used = false;
  //Set input pins to inputs
#if ENABLE_DIGITAL_INPUTS

  for (int i = 0; i < DIGITAL_INPUT_COUNT; i++) {
    int pin = digitalInputs[i];
    if (pin == 0) pin0Used = true;
    Serial.print("Initializing pin ");
    Serial.println(i);
    pinMode(pin, INPUT);
  }

#endif
#if ENABLE_DIGITAL_OUTPUTS

  for (int i = 0; i < DIGITAL_OUTPUT_COUNT; i++) {
    int pin = digitalOutputs[i];
    if (pin == 0) pin0Used = true;
    Serial.print("Initializing pin ");
    Serial.println(i);
    pinMode(pin, OUTPUT);
  }

#endif

  if (!pin0Used) {
    pinMode(0, OUTPUT);
    digitalWrite(0, HIGH);
  }

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

#if DEV
  server.on("/crash", []() {
    Serial.println("Now crashing the ESP...");

    const int bufSize = 6900;
    StaticJsonBuffer<bufSize> buf;
    JsonObject& data = buf.createObject();

    char b[bufSize];
    data.prettyPrintTo(b, bufSize);
    server.send(200, "application/json", b);
  });
#endif

  server.onNotFound([]() {
    server.send(404, "text/plain", "Path Not Found.");
  });

#if OUTPUTS
#if ENABLE_DIGITAL_OUTPUTS
  for (int i = 0; i < DIGITAL_OUTPUT_COUNT; i++) {
    int pin = digitalOutputs[i];
    Serial.print("Pin ");
    Serial.println(pin);

    String path = "/outputs/" + String(pin);
    char pathB[path.length()+1];
    path.toCharArray(pathB,path.length()+1);

    server.on(pathB, HTTP_GET, [i]() {
      const int bufSize = 1200;
      StaticJsonBuffer<bufSize> buf;
      JsonObject& output = buf.createObject();
      
      getOutput(output,i);

      char b[bufSize];
      output.prettyPrintTo(b, bufSize);
      server.send(200, "application/json", b);
    });

    String path1 = "/outputs/" + String(pin) + "/state";
    char path1B[path.length()+1];
    path1.toCharArray(path1B,path1.length()+1);
    server.on(path1B, HTTP_PUT, [i]() {
      Serial.println("Beginning put");
      const int bufSize = 500;
      StaticJsonBuffer<bufSize> buf;
      String a = String(server.arg(0));
      int leftIndex = a.indexOf("{");
      int rightIndex = a.lastIndexOf("}")+1;
      if (leftIndex == -1 || rightIndex ==-1) {
        server.send(400, "YOU FUCKED UP");
        return;
      }
      char b[bufSize];
      a.substring(leftIndex,rightIndex).toCharArray(b,bufSize);
      Serial.println(b);
      Serial.println("parsing");
      JsonObject& req = buf.parseObject(b);
      Serial.println("parsed");
      req.prettyPrintTo(Serial);

      Serial.print("cval: ");
      int j = req["value"];
      Serial.println(j);
      digitalWrite(digitalOutputs[i], req["value"]);

      Serial.println("Writing shit");
      server.sendHeader("Access-Control-Allow-Methods", "GET,POST,PUT");
      server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
      server.send(204, "NO CONTENT");
    });

    server.on(path1B, [i]() {
      const int bufSize = 300;
      StaticJsonBuffer<bufSize> buf;
      JsonObject& state = buf.createObject();
      
      state["display_name"] = digitalRead(digitalOutputs[i]) ? digitalOutputsOnLabels[i] : digitalOutputsOffLabels[i];
      state["is_on"] = digitalRead(digitalOutputs[i]) == 1;
      state["value"] = digitalRead(digitalOutputs[i]);
  
      char b[bufSize];
      state.prettyPrintTo(b, bufSize);
      server.sendHeader("Access-Control-Allow-Methods", "GET,POST,PUT");
      server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
      server.send(200, "application/json", b);
      Serial.println("Preflight done");
    });
  }
#endif

  server.on("/outputs", HTTP_GET, []() {
    const int bufSize = 1200;
    StaticJsonBuffer<bufSize> buf;
    JsonArray& data = buf.createArray();

#if ENABLE_DIGITAL_OUTPUTS
    Serial.println("Reading Digital Outputs...");

    for (int i = 0; i < DIGITAL_OUTPUT_COUNT; i++) {
      int pin = digitalOutputs[i];
      Serial.print("Pin ");
      Serial.println(pin);

      JsonObject& output = data.createNestedObject();
      getOutput(output,i);
    }

#endif

    char b[bufSize];
    data.prettyPrintTo(b, bufSize);
    server.send(200, "application/json", b);
  });
#endif

#if INPUTS
  server.on("/inputs", []() {
    const int bufSize = 1200;
    StaticJsonBuffer<bufSize> buf;
    JsonArray& data = buf.createArray();

#if ENABLE_DIGITAL_INPUTS
    Serial.println("Reading Digital Inputs...");

    for (int i = 0; i < DIGITAL_INPUT_COUNT; i++) {
      int pin = digitalInputs[i];
      Serial.print("Pin ");
      Serial.println(pin);

      JsonObject& input = data.createNestedObject();
      JsonArray& states = input.createNestedArray("states");
      JsonObject& state = input.createNestedObject("state");

      JsonObject& onstate = states.createNestedObject();
      onstate["display_name"] = digitalInputsOnLabels[i];
      onstate["is_on"] = true;
      onstate["value"] = 1;

      JsonObject& off = states.createNestedObject();
      off["display_name"] = digitalInputsOffLabels[i];
      off["is_on"] = false;
      off["value"] = 0;

      state["display_name"] = digitalRead(digitalInputs[i]) ? digitalInputsOnLabels[i] : digitalInputsOffLabels[i];
      state["is_on"] = digitalRead(digitalInputs[i]) == 1;
      state["value"] = digitalRead(digitalInputs[i]);

      input["display_name"] = digitalInputsLabels[i];
      input["input_id"] = digitalInputs[i];
    }

#endif

#if ENABLE_DHT
    Serial.println("Reading DHT...");

    float h = dht.readHumidity();
    float c = dht.readTemperature();
    float f = dht.readTemperature(true);

    if (isnan(h) || isnan(c) || isnan(f)) {
      Serial.println("Failed to read from DHT sensor!");
      Serial.print(h);
      Serial.print(',');
      Serial.print(c);
      Serial.print(',');
      Serial.print(f);
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

#if ENABLE_DHT
    }
#endif

#if ENABLE_ULTRASONIC
    Serial.println("Reading Ultrasonic...");
    digitalWrite(TRIGGER, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER, HIGH);
    delayMicroseconds(5);
    digitalWrite(TRIGGER, LOW);

    int pulse = pulseIn(ECHO, HIGH);

    int d = pulse * CM_PER_MICROSECOND;

    JsonObject& distance = data.createNestedObject();
    distance["value"] = d;
    distance["display_name"] = "Distance (cm)";
    distance["unit"] = "Centimeters";
    distance["unit_abbreviation"] = "(cm)";
    distance["min"] = 0;
    distance["max"] = 1000;
#endif

    char b[bufSize];
    data.prettyPrintTo(b, bufSize);
    server.send(200, "application/json", b);

  });
#endif

  server.onNotFound([]() {
    server.send(404, "text/plain", "Path Not Found.");
  });

  server.begin();
  Serial.println("HTTP server started");
}

#if ENABLE_NOTIFICATIONS
#if ENABLE_DIGITAL_INPUTS
void sendDigitalInputNotification(int i) {
  Serial.println("Sending digital input notification");
  delay(100);
  int pin = digitalInputs[i];
  sendPayloadToServer("{ \"title\": \"" + String(digitalInputsLabels[i]) + " is now " +
                      (digitalRead(pin) ? String(digitalInputsOnLabels[i]) : String(digitalInputsOffLabels[i])) + "\"}\r"
                      , "/api/v1/Notifications");
}
#endif
#if ENABLE_DIGITAL_OUTPUTS
void sendDigitalOutputNotification(int i) {
  Serial.println("Sending digital output notification");
  delay(100);
  int pin = digitalOutputs[i];
  sendPayloadToServer("{ \"title\": \"" + String(digitalOutputsLabels[i]) + " is now " +
                      (digitalRead(pin) ? String(digitalOutputsOnLabels[i]) : String(digitalOutputsOffLabels[i])) + "\"}\r"
                      , "/api/v1/Notifications");
}
#endif
#if ENABLE_DHT
float lastNotifiedTemperature = -1000;
void sendTemperatureNotification(float t) {
  sendPayloadToServer("{ \"title\": \"Temperature is now " + String(t) + "°F\"}\r"
                      , "/api/v1/Notifications");
  lastNotifiedTemperature = t;
}

float lastNotifiedHumidity = -1000;
void sendHumidityNotification(float h) {
  sendPayloadToServer("{ \"title\": \"Humidity is now " + String(h) + "%\"}\r"
                      , "/api/v1/Notifications");
  lastNotifiedHumidity = h;
}

#endif
long lastNotified = 0;
#endif

void loop(void) {
#if ENABLE_NOTIFICATIONS

#if ENABLE_DIGITAL_INPUTS

  for (int i = 0; i < DIGITAL_INPUT_COUNT; i++) {
    int pin = digitalInputs[i];
    //    Serial.print(digitalInputsLabels[i]);
    //    Serial.print(" is ");
    //    Serial.println(digitalRead(pin));
    if ((digitalRead(pin) != digitalInputsStates[i]) &&
        (millis() - lastNotified > NOTIFICATION_DELAY)) {
      sendDigitalInputNotification(i);
      lastNotified = millis();
      digitalInputsStates[i] = digitalRead(pin);
    }
  }

#endif

#if ENABLE_DIGITAL_OUTPUTS

  for (int i = 0; i < DIGITAL_OUTPUT_COUNT; i++) {
    int pin = digitalOutputs[i];
    if ((digitalRead(pin) != digitalOutputsStates[i]) &&
        (millis() - lastNotified > NOTIFICATION_DELAY)) {
      sendDigitalOutputNotification(i);
      lastNotified = millis();
      digitalOutputsStates[i] = digitalRead(pin);
    }
  }

#endif


#if ENABLE_DHT
  float h = dht.readHumidity();
  float f = dht.readTemperature(true);
  float h1 = dht.readHumidity();
  float f1 = dht.readTemperature(true);

  Serial.print(abs(h - h1));
  Serial.print(',');
  Serial.println(abs(f - f1));
  if (abs(h - h1) < 1 && abs(f - f1) < 1 &&
      millis() - lastNotified > NOTIFICATION_DELAY &&
      (abs(lastNotifiedTemperature - f) >= 5 ||
       abs(lastNotifiedHumidity - h) >= 10)) {
    sendTemperatureNotification(f);
    sendHumidityNotification(h);
    lastNotified = millis();
  }
#endif
#endif

  server.handleClient();
  delay(10);
}
