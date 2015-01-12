#pragma once

#include <QStringList>

#include "macros.h"

class Constants {
  DISABLE_COPY_AND_MOVE(Constants)

 public:
  static QStringList configPaths();
  static QStringList keymapPaths();
  static QStringList packagePaths();
  static QString standardConfigPath();
  static QString standardKeymapPath();

 private:
  Constants() = delete;
  ~Constants() = delete;

  static QStringList dataDirectoryPaths();
};