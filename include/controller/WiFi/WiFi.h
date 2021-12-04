#ifndef L33D_WIFI_H
#define L33D_WIFI_H

#include <IPAddress.h>

extern const char *homeWiFiSSID;

extern const char *homeWiFiPassword;

IPAddress initWiFiStation();

#endif
