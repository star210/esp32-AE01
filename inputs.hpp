#pragma once
#include "settings.hpp"

struct inputData {
  char* inputName;
  int inputPin;
  bool inputState;
  unsigned long inputStartTime;
}

#define NUM_INPUTS 8

inputData[NUM_INPUTS] = {
  "inZero", 18, false, 0,              // AE01 Pin 18,  AE01-R Pin 14
  "inOne", 39, false, 0,              // AE01 Pin 39,  AE01-R Pin 32
  "inTwo", 34, false, 0,              // AE01 Pin 34,  AE01-R Pin 21
  "inThree", 35, false, 0,              // AE01 Pin 35,  AE01-R Pin 22
  "inFour", 19, false, 0,              // AE01 Pin 19,  AE01-R Pin 33
  "inFive", 21, false, 0,              // AE01 Pin 21,  AE01-R Pin 34
  "inSix", 22, false, 0,              // AE01 Pin 22,  AE01-R Pin 35
  "inSeven", 23, false, 0              // AE01 Pin 23,  AE01-R Pin 25
};

class _Input {
  public:

    void begin() {
      // Set pinMode for all pins
      for (int i = 0; i < NUM_INPUTS; i++) {
        pinMode(inputData[i].inputPin, INPUT_PULLUP);
      }
    }

    bool state(char* messageName) {
      for (int i = 0; i < NUM_OUTPUTS; i++) {
        if (strcmp(messageName, inputData[i].inputName) == 0) {
          return inputData[i].inputState;
        }
      }
    }

    bool update(char* inputMessage) {
      for (int i = 0; i < NUM_INPUTS; i++) {
        auto timeNow = millis();
        int inputReading = digitalRead(inputData[i].inputPin);

        // Check if digitalRead matches on state and that it is has been off before
        if (inputReading != inputData[i].inputState) {     // If pin is equal to triggered state
          inputData[i].inputStartTime = timeNow;  // start timer
        }
        // Check if still on after debounce time
        if (timeNow - inputData[i].inputStartTime >= settings["inputDelay"]) {
          inputData[i].inputState = !inputData[i].inputState;
          // Only print and publish if true
          //  if (inputData[i].inputState == true) {                 // Uncomment if true only needed
          // Publish MQTT input trigger high message
          Serial.print("Input: ");
          Serial.print(inputData[i].inputName);
          Serial.print(" State: ");
          Serial.println(inputData[i].inputState);
          
          sprintf(inputMessage, "%s/%d", inputData[i].inputName, inputData[i].inputState);
          return true;
        }
      }
      return false;
    }

  public:

static char inputMessage[32];
};

extern _Input Input;
