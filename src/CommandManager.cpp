﻿#include <QDebug>

#include "CommandManager.h"
#include "HelperProxy.h"

QString CommandManager::cmdDescription(const QString& name) {
  if (m_commands.count(name) != 0) {
    return m_commands[name]->description();
  }

  return "";
}

void CommandManager::runCommand(const QString& name, const CommandArgument& args, int repeat) {
  // Copy command name and argument.
  std::string cmdName = name.toUtf8().constData();
  CommandArgument cmdArg = args;

  for (const CmdEventHandler& handler : m_cmdEventFilters) {
    std::tuple<bool, std::string, CommandArgument> resultTuple = handler(cmdName, cmdArg);
    if (std::get<0>(resultTuple)) {
      return;
    }
    cmdName = std::get<1>(resultTuple);
    cmdArg = std::get<2>(resultTuple);
  }

  QString qCmdName = QString::fromUtf8(cmdName.c_str());
  //  qDebug("qCmdName: %s", qPrintable(qCmdName));
  if (m_commands.find(qCmdName) != m_commands.end()) {
    m_commands[qCmdName]->run(cmdArg, repeat);
    HelperProxy::singleton().sendCommandEvent(qCmdName, cmdArg);
  } else {
    qDebug() << "Can't find a command: " << qCmdName;
  }
}

void CommandManager::add(std::unique_ptr<ICommand> cmd) {
  m_commands[cmd->name()] = std::move(cmd);
}

void CommandManager::remove(const QString& name) {
  m_commands.erase(name);
  emit commandRemoved(name);
}

void CommandManager::addEventFilter(CommandManager::CmdEventHandler handler) {
  m_cmdEventFilters.push_back(handler);
}
