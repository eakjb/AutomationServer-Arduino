#include <ArduinoJson.h>

#include <SPI.h>
#include <Ethernet.h>

#define INPUTS false
#define OUTPUTS true
#define DOOR true

#if DOOR
bool doorState = false;
long lastDoorChange = millis();
#define DOOR_PIN 5
#define LIGHT_PIN 2
#define TOGGLE_PIN 3
bool buttonState = false;
#endif

byte mac[] = {
  0xB0, 0x0B, 0x1E, 0x5D, 0x1C, 0x3D
};
IPAddress ip(192, 168, 0, 60); //remember to update registration in setup()

EthernetServer server(80);

const byte PIN_COUNT = 5;
bool analogPins[] = {false, false, false, false, false, false};
bool used[] = {false,false,true,false,true};
byte analogStates[PIN_COUNT];

void setup() {
//  Serial.begin(9600);

  //Delcare output pins

  pinMode(2, OUTPUT);
  pinMode(4, OUTPUT);

  //End

  Ethernet.begin(mac, ip);

  delay(500);
//  Serial.println("R");
  
  String payload = "{ \"name\": \"JakeRoom\", \"address\": \"http://192.168.0.60\" }\r";
  char path[] = "/api/v1/Nodes";
  
  sendPayloadToServer(payload,path);
  sendPinNotification(2);
  sendPinNotification(4);

  server.begin();
  Serial.println(Ethernet.localIP());
}

void sendPayloadToServer(String &payload,char path[]) {
  Serial.println(path);
  Serial.println(payload);
  
  EthernetClient client;
  // if you get a connection, report back via serial:
  if (client.connect("192.168.0.50", 80)) {
    Serial.println('c');
    // Make a HTTP request:
    client.print("POST ");
    client.print(path);
    client.println(" HTTP/1.1");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(payload.length());
    client.println();
    client.println(payload);
    client.stop();
  } else {
    // kf you didn't get a connection to the server:
    Serial.println('f');
  }
  Serial.println('d');
}

String httpStatus;
String httpBody;

String httpReq;
String httpReqMethod;
String httpReqPath;

boolean useRes;

int parseInteger(String str) {
  if (str == "2") return 2;
  if (str == "3") return 3;
  if (str == "4") return 4;
  if (str == "10") return 5;
  if (str == "13") return 13;
  return -1;
}

//Credit: http://arduino.stackexchange.com/questions/1013/how-do-i-split-an-incoming-string
String getDelimeted(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {
    0, -1
  };
  int maxIndex = data.length() - 1;
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

// /
void root(JsonObject& res) {
  if (httpReqMethod == "GET") {
    JsonObject& data = res.createNestedObject("data");

    data["display_name"] = "Jake's Node";
    data["id"] = 0;

#if INPUTS
    data["inputs"] = true;
#endif
#if OUTPUTS
    data["outputs"] = true;
#endif
  } else {
    notAllowed();
  }
}

#if INPUTS

// /inputs
void rootInputs(JsonObject& res) {
  if (httpReqMethod == "GET") {
    JsonArray& data = res.createNestedArray("data");

    for (int i = 0; i < 4; i++) {
      JsonObject& item = data.createNestedObject();
      item["node_id"] = i;
      String s = "A" + String(i);
      item["display_name"] = s;
      //item["units"] = "";
      item["min"] = 0;
      item["max"] = 1023;

      item["value"] = analogRead(i);
    }
  } else {
    notAllowed();
  }
}

#endif
#if OUTPUTS

void getDigitalOutputState(JsonObject& state, boolean active) {
  if (active) {
    state["display_name"] = "On";
    state["is_on"] = true;
    state["value"] = HIGH;
  } else {
    state["display_name"] = "Off";
    state["is_on"] = false;
    state["value"] = LOW;
  }
}

void getAnalogOutputState(JsonObject& state, int val) {
  state["display_name"] = String(val);
  state["value"] = String(val);
}

void getOutput(JsonObject& digital, int pin_id) {
  digital["output_id"] = pin_id;
  digital["display_name"] = "Pin " + String(pin_id);

  JsonObject& states = digital.createNestedObject("states");

  if (analogPins[pin_id]) {
    digital["type"] = "range";
    JsonObject& state = digital.createNestedObject("state");

    getAnalogOutputState(state, analogStates[pin_id]);

    digital["state"] = state;

    states["max"] = 255;
    states["min"] = 0;
  } else {
    digital["type"] = "switch";

    JsonObject& high = states.createNestedObject("on");
    getDigitalOutputState(high, true);

    JsonObject& low = states.createNestedObject("off");
    getDigitalOutputState(low, false);

    digital["state"] = digitalRead(pin_id) ? high : low;
  }
}

// /outputs
void rootOutputs(JsonObject& res) {
  if (httpReqMethod == "GET") {
    JsonArray& data = res.createNestedArray("data");

    //Just pins 2 and 3 for now
    JsonObject& digital = data.createNestedObject();
    getOutput(digital, 2);
    JsonObject& digital2 = data.createNestedObject();
    getOutput(digital2, 4);

  } else {
    notAllowed();
  }
}

// /outputs/{id}
void rootOutputsSpecific(JsonObject& res) {
  JsonObject& data = res.createNestedObject("data");
  if (httpReqMethod == "GET") {
    getOutput(data, parseInteger(getDelimeted(httpReqPath, '/', 2)));
  } else {
    notAllowed();
  }
}

// /outputs/{id}/state
void rootOutputsSpecificState(JsonObject& res, JsonObject& req) {
  JsonObject& data = res.createNestedObject("data");
  int pin = parseInteger(getDelimeted(httpReqPath, '/', 2));

  if (httpReqMethod == "GET") {
    if (analogPins[pin]) {
      getAnalogOutputState(data, analogStates[pin]);
    } else {
      getDigitalOutputState(data, digitalRead(pin));
    }
  } else if (httpReqMethod == "PUT") {
    if (analogPins[pin]) {
      analogWrite(pin, req["value"]);
      analogStates[pin] = req["value"];
    } else {
      digitalWrite(pin, req["value"]);
    }
    noContent();
  } else {
    notAllowed();
  }
}
#endif

void noContent() {
  httpBody = "";
  httpStatus = "204 NO CONTENT";
  useRes = false;
}

void notImplemented() {
  httpBody = "\"Not implemented.\"";
  httpStatus = "501 NOT IMPLEMENTED";
  useRes = false;
}

void notAllowed() {
  httpBody = "\"Not allowed.\"";
  httpStatus = "405 NOT ALLOWED";
  useRes = false;
}

void notFound() {
  httpBody = "\"Path not found.\"";
  httpStatus = "404 NOT FOUND";
  useRes = false;
}

//Handle queries at the HTTP layer
void loop() {
  bool states[PIN_COUNT];
  for (byte i = 0; i<PIN_COUNT; i++) {
    states[i] = digitalRead(i);
  }
  
  EthernetClient client = server.available();
  if (client) {
    Serial.println('n');//new client");

//    Serial.println("fl");//--------Begin First Line----------");
    //Read the first line only into httpReq
    httpReq = "";
    while (client.connected()) {

      if (client.available()) {
        char c = client.read();
        Serial.print(c);
        if (c == '\n') break;
        httpReq += c;
      }
    }
//    Serial.println("el");//--------End First Line----------");

    //Throw out the http headers #TrimTheFat
    boolean currentLineIsBlank = true;
    char contentLengthHeader[] = "content-length: ";
    boolean lineMatches = true;
    byte lineIndex = 0;
    int contentLength = 0;
    while (client.connected()) {
  
      if (client.available()) {
        char c = client.read();
        Serial.print(c);
        
        if (lineIndex>15||c!=contentLengthHeader[lineIndex]) {
          lineMatches = false;
        }
        if (lineIndex>=15&&lineMatches) {
          contentLength = client.parseInt();
          //Serial.print("!!DEEZNUTZ!!");
        }
        lineIndex++;
        
        if (c == '\n' && currentLineIsBlank) {
          break;
        }


        if (c == '\n') {
          currentLineIsBlank = true;
          lineIndex=0;
          lineMatches=true;
        }
        else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
//    Serial.println("-EH-");


    //Throw out the headers and read the rest of the request
    int len = contentLength;
    if (len==0) len=64;
    char body[len];
    body[0] = '{';
    body[1] = '}';
    int i = 0;
    currentLineIsBlank = false;
    int waited = 0;
    while (client.connected()&&(i<contentLength||contentLength==0)) {

      if (client.available()) {
        char c = client.read();
        Serial.print(c);
        body[i++] = c;

        if (c == '\n' && currentLineIsBlank) {
          break;
        }

        if (c == '\n') {
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          currentLineIsBlank = false;
        }
      } else if (contentLength==0) {
        break;
      }
    }
//    Serial.println("\n-EB-");

    //Default headers
    httpStatus = "200 OK";
    httpBody = "";
    useRes = true;

    //Index the method and path
    int mIndex = httpReq.indexOf(" ");
    int pIndex = httpReq.indexOf(" ", mIndex + 1);

    //Parse method and path
    httpReqMethod = httpReq.substring(0, mIndex);
    httpReqPath = httpReq.substring(mIndex + 1, pIndex);

    //Create response buffer
    StaticJsonBuffer<380> buf;
    JsonObject& res = buf.createObject();
    JsonObject& req = buf.parseObject(body);

    res["millis"] = millis();
    res["isWrapped"] = true;

    //---------Declare Paths Here---------------

    if (httpReqMethod == "OPTIONS") {
      httpBody = "\n";
      useRes=false;
    } else if (httpReqPath == "/") {
      root(res);
    }
#if INPUTS
    else if (httpReqPath == "/inputs") {
      rootInputs(res);
    }
#endif
#if OUTPUTS
    else if (httpReqPath.indexOf("/outputs") >= 0) {
      int index2 = httpReqPath.indexOf("/", 1);
      if (index2 >= 0) {
        if (httpReqPath.indexOf("/", index2 + 1) >= 0) {
          rootOutputsSpecificState(res, req);
        } else {
          rootOutputsSpecific(res);
        }
      } else {
        rootOutputs(res);
      }
    }
#endif
    else {
      notFound();
    }

    //-------End Declare Paths Here-------------
    String acaPrefix = "Access-Control-Allow-";
    client.println("HTTP/1.1 " + httpStatus);
    client.println("Content-Type: application/json");
    client.print(acaPrefix);
    client.println("Origin: *");
    client.print(acaPrefix);
    client.println("Methods: GET,POST,PUT");
    client.print(acaPrefix);
    client.println("Headers: Content-Type");
    //client.println("Connection: close"); // the connection will be closed after completion of the response

    client.println();

//    Serial.println("d");
    if (useRes) {
      res.prettyPrintTo(client);
    } else {
      client.print(httpBody);
    }

    delay(10);

    client.stop();
    Serial.println('d');
  }
  
#if DOOR
  bool newState = digitalRead(DOOR_PIN);
  if (newState!=doorState&&millis()-lastDoorChange>500) {

    sendPinNotification(DOOR_PIN);
//    digitalWrite(lightPin,!newState);
//    Serial.println("Change detected");
    doorState=newState;
    lastDoorChange = millis();

    if (newState==LOW) {
      Serial.println('L');
      digitalWrite(LIGHT_PIN,HIGH);
    } else {
      Serial.println('H');
    }
  }
  bool newButtonState = digitalRead(TOGGLE_PIN);
  if (newButtonState&&!buttonState&&millis()-lastDoorChange>500) {
    digitalWrite(LIGHT_PIN,!digitalRead(LIGHT_PIN));
    lastDoorChange = millis();
  }
  buttonState=newButtonState;
#endif

  for (byte i = 0; i<PIN_COUNT; i++) {
    if (used[i]&&states[i] != digitalRead(i)) {
      Serial.print('P');
      Serial.println(i);
//      Serial.println(" changed");
      sendPinNotification(i);
    }
  }
}

void sendPinNotification(int i) {
//      Serial.println(availableMemory());
      String payload = String("{\"title\": \"Pin ") + String(i) + String(" is Now ") + 
      String(digitalRead(i)?"On":"Off") + String("\"}\r");
//      Serial.println(payload.length());
//      Serial.println(availableMemory());
      char path[] = "/api/v1/Notifications";
      sendPayloadToServer(payload,path);
}
//int availableMemory() {
//  int size = 2048; // Use 2048 with ATmega328
//  byte *buf;
//
//  while ((buf = (byte *) malloc(--size)) == NULL)
//    ;
//
//  free(buf);
//
//  return size;
//}
