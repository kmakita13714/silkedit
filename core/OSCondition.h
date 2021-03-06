#pragma once

#include <QVariant>

#include "Condition.h"
#include "macros.h"

namespace core {

class OSCondition : public Condition {
  DISABLE_COPY(OSCondition)

 public:
  static const QString name;

  OSCondition() = default;
  ~OSCondition() = default;
  DEFAULT_MOVE(OSCondition)

  bool isStatic() override { return true; }

 private:
  QVariant value() override;
};

class OnMacCondition : public Condition {
  DISABLE_COPY(OnMacCondition)

 public:
  static const QString name;

  OnMacCondition() = default;
  ~OnMacCondition() = default;
  DEFAULT_MOVE(OnMacCondition)

  bool isStatic() override { return true; }

 private:
  QVariant value() override;
};

class OnWindowsCondition : public Condition {
  DISABLE_COPY(OnWindowsCondition)

 public:
  static const QString name;

  OnWindowsCondition() = default;
  ~OnWindowsCondition() = default;
  DEFAULT_MOVE(OnWindowsCondition)

  bool isStatic() override { return true; }

 private:
  QVariant value() override;
};

}  // namespace core
