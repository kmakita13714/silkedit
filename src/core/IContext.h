#pragma once

#include "core/macros.h"

class QString;

namespace core {

enum class Operator {
  EQUALS,
  NOT_EQUALS,
};

class IContext {
  DISABLE_COPY_AND_MOVE(IContext)
 public:
  static QString operatorString(Operator op);

  virtual ~IContext() = default;

  virtual bool isSatisfied(Operator op, const QString& operand);

 protected:
  IContext() = default;

 private:
  virtual QString key() = 0;
};

}  // namespace core
