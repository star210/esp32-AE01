/* To Do list

  - add temp sensor DS18B20
  - Add ethernet
  - Move MQTT to class
  - Move preferences to class Nvs
  - Add 
  
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

// Define Externs
_Timer Timer;
_Input Input;
_Output Output;
_Wifi Wifi;
_Time Time;

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
// MQTT Credentials
const char *mqtt_username = "remote1";
const char *mqtt_password = "password";

int reconnectCounter = 0;
long lastReconnectAttempt = 0;

long lastUpdate = 0;
int counter = 1;
long lastTachoTime = 0;
int rpm;

using namespace std;

void setup() {
  Wdt.begin();
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);  // this should be after Sensors.begin()
  updateMap();
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
    client.publish("button", "SELECT" );
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
      client.publish("input", inputMessage);
      Serial.println(inputMessage);
    }
  }

  //  if (Timer.state("timerOne") == false) {            
  //    Output.start("relayTwo");                          
  //  }

  if (Timer.state("timerZero") == false) {
    Timer.start("timerZero");
    Output.start("transOne");            
  }


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
  // Break message down
  char *payloadName = strtok(payloadAsChar, "/");
  char *payloadData = strtok('\0', "/");
  // Then convert char* to string to use with unordered map
  string payloadNameStr(payloadName);

  Serial.print("Message arrived in topic: ");
  Serial.print(topic);
  Serial.print(" Message: ");
  Serial.println(payloadAsChar);

  // If topic is a set
  if (strcmp(topic, "set") == 0) {
    for (auto &pair : settings) {
      auto name = pair.first;
      if (payloadNameStr == name) {
        // read the payload as float and store in the settings
        payloadData[length] = '\0'; // Make payload a string by NULL terminating it.
        int payloadValue = atoi((char *)payloadData);
        //float payloadValue = atof(payload);  // read as float
        // Update unordered map
        settings[name] = payloadValue;
        // Convert string to const char pointer to use in preferences
        //   const char* key = payloadAsChar.c_str();
        // Then store in memory
        preferences.begin("settings", false);  // read and write
        preferences.putInt(payloadName, payloadValue);
        // preferences.putFloat(name, payloadValue);
        preferences.end();

        // Join string and int delimited by a "/"
        char dataArray[20];
        snprintf(dataArray, sizeof dataArray, "%s/%d", payloadName, payloadValue);
        // Copy char array to a char pointer so return a pointer
        char *result = (char *)malloc(strlen(dataArray) + 1);
        strcpy(result, dataArray);
        client.publish("reply", result);

        Serial.print(payloadName);
        Serial.print(" ");
        Serial.println(payloadValue);
        Serial.println("Updated and saved to memory");
        return;
      }
    }
  }

  // If topic is a get
  if (strcmp(topic, "get") == 0) {
    for (auto &pair : settings) {
      auto name = pair.first;
      if (payloadNameStr == name) {
        auto name = pair.first;
        // convert to char to publish
        int dataAsInt = settings[name];

        // Join string and int delimited by a "/"
        char dataArray[20];
        snprintf(dataArray, sizeof dataArray, "%s/%d", payloadName, dataAsInt);
        // Copy char array to a char pointer so return a pointer
        char *result = (char *)malloc(strlen(dataArray) + 1);
        strcpy(result, dataArray);
        client.publish("reply", result);

        //  Turn the data into a C string
        //  char dataArray[10];
        //  sprintf(dataArray, "%d", dataAsInt);
        //  client.publish("reply", dataArray);

        Serial.print("Reply: ");
        Serial.println(result);
        return;
      }
    }
  }

  if (strcmp(topic, "output") == 0) {
    Output.start(payloadAsChar);
    return;
  }

  if (strcmp(topic, "timer") == 0) {
    Timer.start(payloadAsChar);
    return;
  }

  if (strcmp(topic, "system") == 0) {

    if (strcmp(payloadAsChar, "publish") == 0) {
      publishInputMessage = ! publishInputMessage;
      Serial.print("Publish input messages: ");
      Serial.println(publishInputMessage);
    }

    if (strcmp(payloadAsChar, "erase") == 0) {
      preferences.begin("settings", false);
      preferences.clear();
      preferences.end();
      Serial.println("data erased in namespace settings");
      return;
    }

    if (strcmp(payloadAsChar, "restart") == 0) {
      Serial.println("Resetting ESP32");
      ESP.restart();
      return;
    }

    if (strcmp(payloadAsChar, "print") == 0) {
      for (auto &pair : settings) {
        auto name = pair.first;
        Serial.println(settings[name]);
      }
      return;
    }
    if (strcmp(payloadAsChar, "save") == 0) {
      // write each setting to preferences
      preferences.begin("settings", false);
      for (auto &pair : settings) {
        auto name = pair.first;
        // Convert string to const char pointer to use in preferences
        const char* key = name.c_str();
        // Save to memory
        preferences.putInt(key, pair.second);
      }
      preferences.end();
      Serial.println("Data saved to memory");
      return;
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

void  updateMap() {
  preferences.begin("settings", false);
  for (auto &pair : settings) {
    auto name = pair.first;
    // Convert string to const char pointer to use in preferences
    const char* key = name.c_str();
    settings[name] = preferences.getInt(key, 0);
  }
  preferences.end();
  Serial.println("Map updated from memory");
}

void publishInput() {
  if (publishInputMessage == true) {                     // Only publish if switched on
    char* inputMessage = Input.update();
    if (inputMessage != "none") {
      client.publish("input", inputMessage);
    }
  }
}

void publishSystem () {
  if (Timer.state("timerOne") == false) {
    client.publish("wifi", Wifi.getRssiAsQuality());
    client.publish("uptime", Time.getUptime());
    client.publish("tacho", rpmToPtr(rpm));
    Timer.start("timerOne");                              // Retrigger for next time
  }
}


void reSubscribe () {
  client.subscribe("timer");
  client.subscribe("output");
  client.subscribe("system");
  client.subscribe("set");
  client.subscribe("get");
  Serial.println("Connected, subscribing... ");
  Oled.displayln("MQTT Connected ");
  Oled.displayln("Subscribing... ");
}

void reconnect() {
  Serial.println("Attempting MQTT connection... ");
  Oled.displayln("Attempting connection ");
  if (client.connect("ESP32", mqtt_username, mqtt_password)) {
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
