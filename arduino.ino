#include <max6675.h>
#include <ArduinoWebsockets.h>
#include <ESP8266WiFi.h>
#include <AccelStepper.h>
#include <ArduinoJson.h>

const char* ssid = "Thecla en Rene-Router-2G";
const char* password = "Billendans1";
const char* websockets_server = "wss://www.weteling.com/websocket";

#define HALFSTEP 8

// Thermocouple* thermocouple;
MAX6675 thermocouple(D6, D7, D8);

// Motor pin definitions
#define motorPin1  D1     // IN1 on the ULN2003 driver 1
#define motorPin2  D2     // IN2 on the ULN2003 driver 1
#define motorPin3  D3     // IN3 on the ULN2003 driver 1
#define motorPin4  D4     // IN4 on the ULN2003 driver 1

// button pin
#define BUTTON_PIN D0
int switchState = 0;

// Initialize with pin sequence IN1-IN3-IN2-IN4 for using the AccelStepper with 28BYJ-48
AccelStepper stepper(HALFSTEP, motorPin1, motorPin3, motorPin2, motorPin4);

using namespace websockets;
WebsocketsClient client;

int minPos = 0;
void findMinPosition() {
  if (minPos != 0) {
    return;
  }
  if (digitalRead(BUTTON_PIN) == HIGH) {
    Serial.println("Min Position found");
    minPos = stepper.currentPosition();
    stepper.stop();
    stepper.setAcceleration(500.0);
    stepper.setSpeed(1000);
    return;
  }
  stepper.moveTo(stepper.currentPosition() - 100);
}

void updatePosition(int targetPos) {
  targetPos = targetPos + minPos;
  if (targetPos != stepper.currentPosition()) {
    Serial.printf("updatePosition moveto %d\n", targetPos);
    stepper.moveTo(targetPos);
  }
}

void onMessageCallback(WebsocketsMessage message) {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, message.data());
  if (doc["message"]["state"]) {
    serializeJson(doc["message"]["state"], Serial);
    Serial.println();
    updatePosition(doc["message"]["state"]["targetPos"]);
  }
}

unsigned long lastMillis;
void sendCurrentState() {
  //  only run every 5 sec
  // stepper.distanceToGo() != 0 ||
  if (minPos == 0 || !(millis() - lastMillis >= 5 * 1000UL)) {
    return;
  }
  lastMillis = millis();

  char buffer[500];
  DynamicJsonDocument doc(1024);
  DynamicJsonDocument dataJson(1024);
  DynamicJsonDocument identifierJson(1024);

  dataJson["action"] = "update";
  // dataJson["minPos"] = minPos;
  dataJson["pos"] = stepper.currentPosition() + (minPos * -1);
  dataJson["dist"] = stepper.distanceToGo();
  dataJson["temp"] = thermocouple.readCelsius();

  identifierJson["channel"] = "BbqChannel";

  char dataConst[200];
  char identifierConst[200];
  serializeJson(dataJson, dataConst);
  serializeJson(identifierJson, identifierConst);

  doc["command"] = "message";
  doc["identifier"] = "{\"channel\": \"BbqChannel\"}";
  doc["data"] = dataConst;

  serializeJsonPretty(doc, buffer);
  client.send(buffer);
}

void onEventsCallback(WebsocketsEvent event, String data) {
  Serial.println(data);
  if (event == WebsocketsEvent::ConnectionOpened) {
    Serial.println("Connnection Opened");
  }
  else if (event == WebsocketsEvent::ConnectionClosed) {
    Serial.println("Connnection Closed");
  }
  else if (event == WebsocketsEvent::GotPing) {
    Serial.println("Got a Ping!");
  }
  else if (event == WebsocketsEvent::GotPong) {
    Serial.println("Got a Pong!");
  }
}

void setupWifi() {
  // Connect to wifi
  WiFi.begin(ssid, password);

  // Wait some time to connect to wifi
  for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
    Serial.print(".");
    delay(1000);
  }

  // Setup Callbacks
  client.onMessage(onMessageCallback);
  client.onEvent(onEventsCallback);

  // Connect to server
  client.connect(websockets_server);

  // Subscribe to our channel
  client.send("{\"command\": \"subscribe\", \"identifier\": \"{\\\"channel\\\": \\\"BbqChannel\\\"}\"}");
}

void setup() {
  //  Set the boudrate of ther serial (for the console)
  Serial.begin(115200);

  // set the stepper
  stepper.setMaxSpeed(1000.0);
  stepper.setAcceleration(10000.0);
  stepper.setSpeed(1000);
  stepper.moveTo(0);

  // Setup wifi
  setupWifi();

  // input pin for the switch
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // wait for max chip to stabalise
  delay(1000);
}

void loop() {
  client.poll();
  stepper.run();
  findMinPosition();
  sendCurrentState();
}

