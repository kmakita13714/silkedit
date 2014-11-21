#pragma once

#include <QObject>

#include "ICommand.h"
#include "TextEditView.h"

class OpenFileCommand : public QObject, public ICommand {
  Q_OBJECT
 public:
  static const QString name;

  OpenFileCommand(TextEditView* textEditView);
  ~OpenFileCommand() = default;
  DEFAULT_COPY_AND_MOVE(OpenFileCommand)

 private:
  void doRun(const CommandArgument& args, int repeat = 1) override;
  void hoge();

  TextEditView* m_textEditView;
};