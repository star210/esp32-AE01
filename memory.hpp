# pragma once
#include <Preferences.h>
#include <string>
#include "settings.hpp"
#include "stdlib.h"

using namespace std;

class _Memory {
  public:

    void begin() {
      preferences.begin("settings", false);
      Serial.println("Opened nvs.begin");
      for (auto &pair : settings) {
        auto name = pair.first;
        // Convert string to const char pointer to use in preferences
        const char* key = name.c_str();
        settings[name] = preferences.getInt(key, 0);
        Serial.println(key);
      }
      preferences.end();
      Serial.println("Settings updated from memory");
    }


    void get (char* payloadName, char* reply) {
      string payloadNameStr(payloadName);             // Then convert char* to string to use with unordered map
      for (auto &pair : settings) {
        auto name = pair.first;
        if (payloadNameStr == name) {
          int dataAsInt = settings[name];
          sprintf(reply, "%s/%d", payloadName, dataAsInt);
        }
      }
    }

    void set(char* payloadName, char* payloadData) {
      string payloadNameStr(payloadName);             // Then convert char* to string to use with unordered map
      int payloadDataAsInt = atoi(payloadData);        // Convert payload data to int when there is something to convert
      // Now find the setting
      for (auto &pair : settings) {
        auto name = pair.first;
        if (payloadNameStr == name) {
          // Update unordered map
          settings[name] = payloadDataAsInt;
          // Then store in memory
          preferences.begin("settings", false);  // read and write
          preferences.putInt(payloadName, payloadDataAsInt);
          // preferences.putFloat(name, payloadValue);
          preferences.end();
          Serial.print(payloadName);
          Serial.print("/");
          Serial.print(payloadDataAsInt);
          Serial.println(" Updated");
        }
      }
    }

    void save () {
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

    void erase () {
      preferences.begin("settings", false);
      preferences.clear();
      preferences.end();
      Serial.println("data erased in namespace settings");
      return;
    }

  public:
    Preferences preferences;
};

extern _Memory Memory;
