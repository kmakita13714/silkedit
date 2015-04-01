#pragma once

#include "ICommand.h"

class UndoCommand : public ICommand {
 public:
  static QString name;

  UndoCommand();
  ~UndoCommand() = default;
  DEFAULT_COPY_AND_MOVE(UndoCommand)

 private:
  void doRun(const CommandArgument& args, int repeat = 1) override;
};