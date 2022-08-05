#pragma once

class _Time {
  public:

    char* getUptime() {
      upSeconds = millis() / 1000;
      seconds = upSeconds % 60;
      minutes = (upSeconds / 60) % 60;
      hours = (upSeconds / (60 * 60)) % 24;
      days =  upSeconds / (60 * 60 * 24); 
      
      sprintf(timeStr, "%02d %02d:%02d:%02d",  days, hours, minutes, seconds);
      Serial.println(timeStr);    // Uncomment to serial print
      return timeStr;
    }

  public:
    char timeStr[32];
    long upSeconds, days, lastUpdate;
    int hours, minutes, seconds;


};

extern _Time Time;
