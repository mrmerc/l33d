#include "controller/WebServer/WebServer.h"
#include <ESP8266SSDP.h>
#include <ESP8266WebServer.h>
#include <controller/AppOptions/AppOptions.h>
#include <shared/L33DCommon.h>
#include <shared/constants.h>

WebServer::WebServer(int port, AppOptions &appOptions) : _server(port), _appOptions(appOptions){};

void WebServer::handleRoot() {
  _server.send(HttpResponse::SUCCESS, "text/plain", "hello from esp8266!");
}

void WebServer::handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += _server.uri();
  message += "\nMethod: ";
  message += (_server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += _server.args();
  message += "\n";

  for (uint8_t i = 0; i < _server.args(); i++) {
    message += " " + _server.argName(i) + ": " + _server.arg(i) + "\n";
  }

  _server.send(HttpResponse::NOT_FOUND, "text/plain", message);
}

void WebServer::handleHttpClient() {
  _server.handleClient();
}

void WebServer::changeSettings() {
  for (uint8_t i = 0; i < _server.args(); i++) {
    String optionKey = _server.argName(i);
    String optionValue = _server.arg(i);

    if (optionKey.length() == 0 || optionValue.length() == 0) {
      continue;
    }

    _appOptions.setOption(optionKey, optionValue);
  }

  String message = "Settings\n\n";

  std::vector<appOption> options = _appOptions.getOptions();

  for (uint8_t i = 0; i < options.size(); i++) {
    message += options[i].key + " - " + options[i].value + "\n";
  }

  _server.send(HttpResponse::SUCCESS, "text/plain", message);
}

void WebServer::init() {
  _server.on("/", std::bind(&WebServer::handleRoot, this));

  // TODO: set HTTP_POST
  _server.on("/settings", HTTP_GET, std::bind(&WebServer::changeSettings, this));

  // 404
  _server.onNotFound(std::bind(&WebServer::handleNotFound, this));

  _server.begin();
}

void WebServer::initSSDP() {
  _server.on("/description.xml", HTTP_GET, [this]() {
    SSDP.schema(this->_server.client());
  });

  SSDP.setDeviceType("upnp:rootdevice");
  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(HTTP_PORT);
  SSDP.setName(ssdpName);
  SSDP.setSerialNumber("133713371337");
  SSDP.setURL("/");
  SSDP.setModelName("ESP");
  SSDP.setModelNumber("8266");
  SSDP.setModelURL("https://bymerc.xyz");
  SSDP.setManufacturer("Vasily Andreev");
  SSDP.setManufacturerURL("https://bymerc.xyz");
  SSDP.begin();
}
