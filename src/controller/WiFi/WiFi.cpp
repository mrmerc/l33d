#include <ESP8266WiFi.h>

const char *homeWiFiSSID = "V A S I L Y [ M Ξ R C ]";

const char *homeWiFiPassword = "MztcKT6h\\u}@D!gw";

IPAddress initWiFiStation() {
  WiFi.mode(WIFI_STA);

  WiFi.begin(homeWiFiSSID, homeWiFiPassword);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }

  Serial.println("WiFi connected!");
  Serial.println(WiFi.localIP());

  return WiFi.localIP();
}