#include <QDebug>
#include <QTextCursor>

#include "MoveCursorCommand.h"
#include "vi.h"
#include "stlSpecialization.h"

namespace {

int toMoveOperation(const QString& str) {
  if (str.toLower() == "up") {
    return QTextCursor::Up;
  } else if (str.toLower() == "down") {
    return QTextCursor::Down;
  } else if (str.toLower() == "left") {
    return QTextCursor::Left;
  } else if (str.toLower() == "right") {
    return QTextCursor::Right;
  } else if (str.toLower() == "start_of_block") {
    return QTextCursor::StartOfBlock;
  } else if (str.toLower() == "first_non_blank_char") {
    return ViMoveOperation::FirstNonBlankChar;
  } else if (str.toLower() == "last_char") {
    return ViMoveOperation::LastChar;
  } else if (str.toLower() == "next_line") {
    return ViMoveOperation::NextLine;
  } else if (str.toLower() == "prev_line") {
    return ViMoveOperation::PrevLine;
  } else {
    return QTextCursor::NoMove;
  }
}
}

MoveCursorCommand::MoveCursorCommand(TextEditView* textEditView)
    : ICommand("move_cursor"), m_textEditView(textEditView) {
}

void MoveCursorCommand::doRun(const CommandArgument& args, int repeat) {
  if (auto operationStr = args.find<QString>("operation")) {
    int operation = toMoveOperation(*operationStr);
    m_textEditView->moveCursor(operation, repeat);
  }
}