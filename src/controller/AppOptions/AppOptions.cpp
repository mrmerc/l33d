#include "controller/AppOptions/AppOptions.h"
#include <shared/L33DCommon.h>
#include <shared/constants.h>

AppOptions::AppOptions(){};

std::vector<appOption> AppOptions::getOptions() {
  return _appOptions;
}

void AppOptions::setOption(String key, String value) {
  for (int i = 0; i < _appOptions.size(); i++) {
    if (_appOptions[i].key == key) {
      _appOptions[i].value = value;
      return;
    }
  }

  appOption option;
  option.key = key;
  option.value = value;

  return _appOptions.push_back(option);
}
