#pragma once

#include <unordered_map>
#include <QString>
#include <QVariant>

#include "stlSpecialization.h"
#include "macros.h"
#include "IContext.h"
#include "CommandArgument.h"

class CommandEvent {
  DISABLE_COPY(CommandEvent)
 public:
  explicit CommandEvent(const QString& name);
  CommandEvent(const QString& name, const CommandArgument& args);
  CommandEvent(const QString& name, std::shared_ptr<IContext> context);
  CommandEvent(const QString& name, const CommandArgument& args, std::shared_ptr<IContext> context);
  ~CommandEvent() = default;
  DEFAULT_MOVE(CommandEvent)

  bool execute(int repeat = 1);

 private:
  QString m_cmdName;
  CommandArgument m_args;
  std::shared_ptr<IContext> m_context;
};