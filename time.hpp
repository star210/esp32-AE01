#pragma once

class _Time {
  public:

    char* getUptime() {
      auto timeNow = millis();
      if (timeNow >= 3000000000) {
        highMillis = true;
      }
      if (timeNow <= 100000 && highMillis == true) {
        rollover++;
        highMillis = false;
      }
      long upSeconds = timeNow / 1000;
      seconds = upSeconds % 60;
      minutes = (upSeconds / 60) % 60;
      hours = (upSeconds / (60 * 60)) % 24;
      days = (rollover * 50) + (upSeconds / (60 * 60 * 24)); // 50 day rollover
      
      sprintf(timeStr, "%02d %02d:%02d:%02d",  days, hours, minutes, seconds);
      Serial.println(timeStr);    // Uncomment to serial print
      return timeStr;
    }

  public:
    char timeStr[30];
    long days, lastUpdate;
    int hours, minutes, seconds, rollover;
    bool highMillis = false;

};

extern _Time Time;
