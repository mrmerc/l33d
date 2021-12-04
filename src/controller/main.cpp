#include "Arduino.h"

#include "controller/WiFi/WiFi.h"
#include <WiFiUDP.h>
#include <controller/AppOptions/AppOptions.h>
#include <controller/WebServer/WebServer.h>
#include <shared/L33DCommon.h>
#include <shared/constants.h>

AppOptions appOptions;

WebServer webServer(HTTP_PORT, appOptions);

void setup() {
  delay(500);

  Serial.begin(115200);

  initWiFiStation();

  webServer.init();
  webServer.initSSDP();
}

void loop() {
  webServer.handleHttpClient();

  delay(1);
}
