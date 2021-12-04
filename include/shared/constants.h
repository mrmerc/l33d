#ifndef L33D_SHARED_CONSTANTS_H
#define L33D_SHARED_CONSTANTS_H

const int HTTP_PORT = 80;

enum HttpResponse {
  SUCCESS = 200,
  NOT_FOUND = 404,
  SERVER_ERROR = 500,
};

const char *const ssdpName = "l33d-ssdp";

const int OPTIONS_LENGTH = 3;

#endif
