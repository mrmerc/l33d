#ifndef L33D_APP_OPTIONS_H
#define L33D_APP_OPTIONS_H

#include <shared/L33DCommon.h>
#include <shared/constants.h>

class AppOptions {
public:
  AppOptions(void);
  std::vector<appOption> getOptions();
  void setOption(String key, String value);

private:
  std::vector<appOption> _appOptions;
};

#endif
