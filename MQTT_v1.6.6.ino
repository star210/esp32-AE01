/* To Do list

  - add temp sensor DS18B20
  - Add ethernet
  - Move MQTT to class
  - Move preferences to class Nvs
  - Add NPT time server to wifi or time class ?

*/

#include "stdlib.h"

#include <PubSubClient.h>
#include <Preferences.h>

#include "settings.hpp"
#include "oled.hpp"
#include "outputs.hpp"
#include "inputs.hpp"
#include "timers.hpp"
#include "buttons.hpp"
#include "wifi.hpp"
#include "time.hpp"
#include "watchdog.hpp"
#include "defs.hpp"
#include "sdcard.hpp"
#include "memory.hpp"

// Define Externs
_Timer Timer;
_Input Input;
_Output Output;
_Wifi Wifi;
_Time Time;
_Nvs Nvs;

Buttons buttons;
Oled Oled;
WDT Wdt;
Sdcard Sdcard;

WiFiClient espClient;
PubSubClient client(espClient);
Preferences preferences;

char *inputMessage = "";
char *payloadAsChar = "";
bool publishInputMessage = false;

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io";
const int mqtt_port = 1883;
const char *mqtt_ID = "esp32a01";
// MQTT Credentials
const char *mqtt_username = "remote2";
const char *mqtt_password = "password2";
const char *mqtt_client = "ESP32A01";

int reconnectCounter = 0;
long lastReconnectAttempt = 0;

long lastUpdate = 0;
int counter = 1;
long lastTachoTime = 0;
int rpm;

bool inputBool = false;

using namespace std;

void setup() {
  delay(100);

  Nvs.begin();                                   // Update settings from nvs memory
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);  // this should be after Sensors.begin()
  Serial.print("Running software V");
  Serial.println(settings["version"]);
  Oled.begin();
  Oled.displayln("Starting....");
  delay(500);

  // setup MQTT broker
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);

  buttons.begin();
  Output.begin();
  Input.begin();
  Wifi.connect();
  Wdt.begin();
  Sdcard.begin();


  // Connect to MQTT broker
  reconnect();

}

void loop() {
  auto timeNow = millis();
  // Update loops
  Output.update();
  Timer.update();
  Wdt.update();

  publishInput();
  publishSystem();

  if (buttons.pressed() == Buttons::SELECT) {
    publishMQTT("button", "SELECT" );
  }

  inputMessage = Input.update();
  if (inputMessage != "none") {

    if (strcmp(inputMessage, "inFive/1") == 0) {
      Serial.println("Button Pushed");
      if (counter > 3) {
        counter = 1;
      }
      if (counter == 1) {
        Serial.println("relayTwo triggered");
        Output.start("relayTwo");
      }
      if (counter == 2) {
        Output.start("relayThree");
        Serial.println("relayThree triggered");
      }
      if (counter == 3) {
        Output.start("relayFour");
        Serial.println("relayFour triggered");
      }
      counter ++;
    }

    if (strcmp(inputMessage, "inSix/1") == 0) {
      Serial.println("transOne triggered");
      Output.start("transOne");
    }

    if (strcmp(inputMessage, "inOne/1") == 0) {
      Serial.println("relayOne triggered");
      Output.start("relayOne");
    }

    if (strcmp(inputMessage, "inSeven/1") == 0) {
      Serial.println("relayZero triggered");
      //      Timer.start("timerOne");
      Output.start("relayZero");
    }

    // Calculate RPM
    if (strcmp(inputMessage, "inZero/1") == 0) {
      unsigned int revTime = timeNow - lastTachoTime;   // Calculate millis per rev
      if (revTime >= 60000) {
        rpm = 0;                                        // Limit rpm to 0
      }
      else {
        rpm = 60000 / revTime;                          // Convert to rpm
      }
      lastTachoTime = timeNow;                          // Update timer
    }
    // Only publish input message if switched on
    if (publishInputMessage == true) {
      publishMQTT("input", inputMessage);
      Serial.println(inputMessage);
    }
  }

  if (Timer.state("timerTwo") == false) {
    if ( inputBool == false) {
      Timer.start("timerTwo");
      publishMQTT("input", "inZero/1");
      inputBool = true;
    }
    Timer.start("timerTwo");
    publishMQTT("input", "inZero/0");
    inputBool = false;
  }

  //  if (Timer.state("timerZero") == false) {
  //    Timer.start("timerZero");
  //    Output.start("transOne");
  //  }


  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 2000) {
      Serial.println("Connecting to client...");
      reconnect(); // Attempt to reconnect
      lastReconnectAttempt = now;
    }
  } else {
    // Client connected
    client.loop();
  }
}

void callback(char* topic, byte * payload, unsigned int length) {

  //Conver *byte to char*
  payload[length] = '\0';   //First terminate payload with a NULL
  payloadAsChar = (char*)payload;
  // Break topic down
  char *payloadId = strtok(topic, "/");
  char *payloadFunc = strtok('\0', "/");
  // Break payload down
  char *payloadName = strtok(payloadAsChar, "/");
  char *payloadData = strtok('\0', "/");

  Serial.print("ID: ");
  Serial.print(payloadId);
  Serial.print(" Function: ");
  Serial.print(payloadFunc);
  Serial.print(" Name: ");
  Serial.print(payloadName);
  Serial.print(" Data: ");
  Serial.println(payloadData);

  // If topic is a set
  if (strcmp(payloadFunc, "set") == 0) {
    Nvs.set(payloadName, payloadData);
    char* reply = Nvs.get(payloadName);
    publishMQTT("reply", reply);
  }
  // If topic is a get
  if (strcmp(payloadFunc, "get") == 0) {
    char* reply = Nvs.get(payloadName);
    publishMQTT("reply", reply);
  }

  if (strcmp(payloadFunc, "output") == 0) {
    Output.process(payloadAsChar, payloadData);
  }

  if (strcmp(payloadFunc, "timer") == 0) {
    Timer.start(payloadAsChar);
    return;
  }

  if (strcmp(payloadFunc, "system") == 0) {

    if (strcmp(payloadName, "publish") == 0) {
      publishInputMessage = ! publishInputMessage;
      Serial.print("Publish input messages: ");
      Serial.println(publishInputMessage);
    }

    if (strcmp(payloadName, "restart") == 0) {
      Serial.println("Resetting ESP32");
      ESP.restart();
      return;
    }

    if (strcmp(payloadName, "save") == 0) {
      Nvs.save();
    }

    if (strcmp(payloadName, "erase") == 0) {
      Nvs.erase();
    }
  }
  // End of MQTT callback
}

char* rpmToPtr (int rpmAsInt) {
  // Convert to char pointer to publish
  char rpmChr[10];
  snprintf(rpmChr, sizeof rpmChr, "%d", rpmAsInt);
  // Copy char array to a char pointer so return a pointer
  char *rpmPtr = (char *)malloc(strlen(rpmChr) + 1);
  strcpy(rpmPtr, rpmChr);
  Serial.print("RPM: ");
  Serial.println(rpmPtr);
  return rpmPtr;
}


void publishInput() {
  if (publishInputMessage == true) {                     // Only publish if switched on
    char* inputMessage = Input.update();
    if (inputMessage != "none") {
      publishMQTT("input", inputMessage);
    }
  }
}

void publishMQTT(char* topic, char* payload) {
  char dataArray[30];
  snprintf(dataArray, sizeof dataArray, "%s/%s", mqtt_ID, topic);
  char *result = (char *)malloc(strlen(dataArray) + 1);                 // Copy char array to a char pointer so return a pointer
  strcpy(result, dataArray);
  client.publish(result, payload);
  Serial.print(result);
  Serial.print("/");
  Serial.println(payload);
}

void subscribeMQTT(char* topic) {
  char dataArray[30];
  snprintf(dataArray, sizeof dataArray, "%s/%s", mqtt_ID, topic);
  char *result = (char *)malloc(strlen(dataArray) + 1);                 // Copy char array to a char pointer so return a pointer
  strcpy(result, dataArray);
  client.subscribe(result);
  Serial.println(result);
}

void publishSystem () {
  if (Timer.state("timerOne") == false) {
    publishMQTT("wifi", Wifi.getRssiAsQuality());
    publishMQTT("uptime", Time.getUptime());
    publishMQTT("tacho", rpmToPtr(rpm));
    Timer.start("timerOne");                              // Retrigger for next time
  }
}


void reSubscribe () {
  subscribeMQTT("timer");
  subscribeMQTT("output");
  subscribeMQTT("system");
  subscribeMQTT("set");
  subscribeMQTT("get");
  Serial.println("Connected, subscribing... ");
  Oled.displayln("MQTT Connected ");
  Oled.displayln("Subscribing... ");
}

void reconnect() {
  Serial.println("Attempting MQTT connection... ");
  Oled.displayln("Attempting connection ");
  if (client.connect(mqtt_client, mqtt_username, mqtt_password)) {
    reSubscribe();
    reconnectCounter = 0;  // reset counter
  }
  else if (reconnectCounter > 5) {
    Serial.println("Resetting ESP32");
    delay(500);
    // ESP.restart();
  }
  else {
    reconnectCounter ++;
    Serial.print("Attempt: ");
    Serial.print(reconnectCounter);
    Serial.print(" failed, Error: ");
    Serial.print(client.state());
    Serial.print(" Retrying in 5 seconds");
  }
}
/*

  -4 : MQTT_CONNECTION_TIMEOUT - the server didn't respond within the keepalive time
  -3 : MQTT_CONNECTION_LOST - the network connection was broken
  -2 : MQTT_CONNECT_FAILED - the network connection failed
  -1 : MQTT_DISCONNECTED - the client is disconnected cleanly
  0 : MQTT_CONNECTED - the client is connected
  1 : MQTT_CONNECT_BAD_PROTOCOL - the server doesn't support the requested version of MQTT
  2 : MQTT_CONNECT_BAD_CLIENT_ID - the server rejected the client identifier
  3 : MQTT_CONNECT_UNAVAILABLE - the server was unable to accept the connection
  4 : MQTT_CONNECT_BAD_CREDENTIALS - the username/password were rejected
  5 : MQTT_CONNECT_UNAUTHORIZED - the client was not authorized to connect
*/
