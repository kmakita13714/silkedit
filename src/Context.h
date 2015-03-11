#pragma once

#include <unordered_map>
#include <QString>

#include "IContext.h"
#include "stlSpecialization.h"

class Context {
 public:
  static void init();
  static void add(const QString& key, IContext* context);
  static void remove(const QString& key);

  QString m_key;
  Operator m_op;
  QString m_value;

  Context(const QString& key, Operator op, const QString& value);
  bool isSatisfied();

 private:
  static std::unordered_map<QString, IContext*> s_contexts;
};