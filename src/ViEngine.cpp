#include <memory>
#include <QStatusBar>

#include "MainWindow.h"
#include "ViEngine.h"
#include "TextEditView.h"
#include "LayoutView.h"
#include "CommandService.h"
#include "ContextService.h"
#include "KeymapService.h"
#include "ModeContext.h"
#include "commands/ChangeModeCommand.h"

ViEngine::ViEngine(LayoutView* layoutView, MainWindow* mainWindow, QObject* parent)
    : QObject(parent),
      m_mode(Mode::CMD),
      m_layoutView(layoutView),
      m_mainWindow(mainWindow),
      m_repeatCount(0),
      m_cmdLineEdit(new QLineEdit()),
      m_isEnabled(false) {
}

void ViEngine::processExCommand(const QString&) {
  setMode(Mode::CMD);
}

void ViEngine::enable() {
  std::unique_ptr<ChangeModeCommand> changeModeCmd(new ChangeModeCommand(this));
  CommandService::singleton().add(std::move(changeModeCmd));

  ContextService::singleton().add(
      ModeContext::name,
      std::move(std::unique_ptr<ModeContextCreator>(new ModeContextCreator(this))));

  KeyHandler::singleton().registerKeyEventFilter(this);
  if (auto view = m_layoutView->activeEditView()) {
    view->setThinCursor(false);
  }

  m_mainWindow->statusBar()->addWidget(m_cmdLineEdit.get(), 1);
  m_cmdLineEdit->installEventFilter(this);

  connect(m_cmdLineEdit.get(), SIGNAL(returnPressed()), this, SLOT(cmdLineReturnPressed()));
  connect(m_cmdLineEdit.get(),
          SIGNAL(cursorPositionChanged(int, int)),
          this,
          SLOT(cmdLineCursorPositionChanged(int, int)));
  connect(m_cmdLineEdit.get(),
          SIGNAL(textChanged(QString)),
          this,
          SLOT(cmdLineTextChanged(const QString&)));

  m_mode = Mode::CMD;
  onModeChanged(m_mode);
  m_repeatCount = 0;

  KeymapService::singleton().load();

  m_isEnabled = true;

  emit enabled();
}

void ViEngine::disable() {
  CommandService::singleton().remove(ChangeModeCommand::name);
  ContextService::singleton().remove(ModeContext::name);

  KeyHandler::singleton().registerKeyEventFilter(this);
  if (auto view = m_layoutView->activeEditView()) {
    view->setThinCursor(true);
  }

  m_mainWindow->statusBar()->removeWidget(m_cmdLineEdit.get());
  m_mainWindow->statusBar()->clearMessage();
  m_cmdLineEdit->removeEventFilter(this);

  disconnect(m_cmdLineEdit.get(), SIGNAL(returnPressed()), this, SLOT(cmdLineReturnPressed()));
  disconnect(m_cmdLineEdit.get(),
             SIGNAL(cursorPositionChanged(int, int)),
             this,
             SLOT(cmdLineCursorPositionChanged(int, int)));
  disconnect(m_cmdLineEdit.get(),
             SIGNAL(textChanged(QString)),
             this,
             SLOT(cmdLineTextChanged(const QString&)));

  KeymapService::singleton().load();

  m_isEnabled = false;
  emit disabled();
}

bool ViEngine::keyEventFilter(QKeyEvent* event) {
  if (event->type() == QEvent::KeyPress && mode() == Mode::CMD) {
    cmdModeKeyPressEvent(static_cast<QKeyEvent*>(event));
    return true;
  }

  if (event->type() == QEvent::KeyPress && mode() == Mode::CMDLINE) {
    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->key() == Qt::Key_Escape) {
      setMode(Mode::CMD);
      return true;
    }
  }

  return false;
}

void ViEngine::setMode(Mode mode) {
  if (mode != m_mode) {
    if (m_mode == Mode::INSERT && m_layoutView->activeEditView()) {
      m_layoutView->activeEditView()->moveCursor(QTextCursor::Left);
    }
    m_mode = mode;
    onModeChanged(mode);

    emit modeChanged(mode);
  }
}

void ViEngine::onModeChanged(Mode mode) {
  QString text;
  switch (mode) {
    case Mode::CMD:
      text = "CMD";
      break;
    case Mode::INSERT:
      text = "INSERT";
      break;
    case Mode::CMDLINE:
      m_cmdLineEdit->setText(":");
      m_cmdLineEdit->show();
      m_cmdLineEdit->setFocus(Qt::OtherFocusReason);
      return;
  }

  m_cmdLineEdit->hide();
  m_mainWindow->statusBar()->showMessage(text);

  updateCursor();
}

void ViEngine::updateCursor() {
  if (mode() == Mode::CMD) {
    if (auto view = m_layoutView->activeEditView()) {
      view->setThinCursor(false);
    }
  } else {
    if (auto view = m_layoutView->activeEditView()) {
      view->setThinCursor(true);
    }
  }
}

void ViEngine::cmdLineReturnPressed() {
  QString text = m_cmdLineEdit->text();
  if (!text.isEmpty() && text[0] == ':') {
    processExCommand(text.mid(1));
  }
}

void ViEngine::cmdLineCursorPositionChanged(int, int newPos) {
  if (newPos == 0) {
    m_cmdLineEdit->setCursorPosition(1);
  }
}

void ViEngine::cmdLineTextChanged(const QString& text) {
  if (text.isEmpty() || text[0] != ':') {
    setMode(Mode::CMD);
  }
}

void ViEngine::cmdModeKeyPressEvent(QKeyEvent* event) {
  QString text = event->text();
  if (text.isEmpty()) {
    return;
  }

  ushort ch = text[0].unicode();
  if ((ch == '0' && m_repeatCount != 0) || (ch >= '1' && ch <= '9')) {
    m_repeatCount = m_repeatCount * 10 + (ch - '0');
    return;
  }

  if (m_repeatCount > 0) {
    KeymapService::singleton().dispatch(event, m_repeatCount);
    m_repeatCount = 0;
  } else {
    KeymapService::singleton().dispatch(event);
  }
}
