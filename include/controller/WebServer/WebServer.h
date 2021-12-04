#ifndef L33D_WEBSERVER_H
#define L33D_WEBSERVER_H

#include <ESP8266WebServer.h>
#include <controller/AppOptions/AppOptions.h>

class WebServer {
public:
  WebServer(int port, AppOptions &appOptions);
  void init();
  void initSSDP();
  void handleHttpClient();
  void changeSettings();

private:
  ESP8266WebServer _server;
  AppOptions &_appOptions;
  void handleNotFound();
  void handleRoot();
};

#endif
