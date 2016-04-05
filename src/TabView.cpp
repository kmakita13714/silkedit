﻿#include <memory>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QStylePainter>
#include <QTimer>

#include "TabView.h"
#include "TextEdit.h"
#include "KeymapManager.h"
#include "TabBar.h"
#include "Window.h"
#include "DraggingTabInfo.h"
#include "App.h"
#include "DocumentManager.h"
#include "Helper.h"
#include "core/Config.h"
#include "core/Theme.h"
#include "core/Util.h"
#include "core/scoped_guard.h"

using core::Document;
using core::Config;
using core::Theme;
using core::Util;
using core::ColorSettings;
using core::scoped_guard;

namespace {
constexpr const char* PATHS_PREFIX = "paths";
constexpr const char* TAB_TEXT_PREFIX = "tab_text";

QString getFileNameFrom(const QString& path) {
  QFileInfo info(path);
  return info.fileName().isEmpty() ? DocumentManager::DEFAULT_FILE_NAME : info.fileName();
}

bool isUntitledAndNotEmpty(Document* doc) {
  Q_ASSERT(doc);
  return doc->path().isEmpty() && !doc->isEmpty();
}

bool isNotUntitledAndModified(Document* doc) {
  Q_ASSERT(doc);
  return !doc->path().isEmpty() && doc->isModified();
}
}

TabView::TabView(QWidget* parent)
    : QTabWidget(parent), m_activeView(nullptr), m_tabBar(new TabBar(this)), m_tabDragging(false) {
  setTabBar(m_tabBar);
  setMovable(true);
  setDocumentMode(true);
  setTabsClosable(true);
  setTheme(Config::singleton().theme());
  // Note: setDocumentMode also calls setDrawBase
  tabBar()->setDrawBase(false);

  connect(m_tabBar, &TabBar::onDetachTabStarted, this, &TabView::detachTabStarted);
  connect(m_tabBar, &TabBar::onDetachTabEntered, this, &TabView::detachTabEntered);
  connect(m_tabBar, &TabBar::onDetachTabFinished, this, &TabView::detachTabFinished);
  connect(this, &QTabWidget::tabBarClicked, this, &TabView::focusTabContent);
  connect(this, &QTabWidget::currentChanged, this, &TabView::changeActiveView);
  connect(this, &QTabWidget::tabCloseRequested, this,
          static_cast<bool (TabView::*)(int)>(&TabView::closeTab));
  connect(&Config::singleton(), &Config::themeChanged, this, &TabView::setTheme);
}

TabView::~TabView() {
  qDebug("~TabView");
}

int TabView::addTab(QWidget* page, const QString& label) {
  return insertTab(-1, page, label);
}

int TabView::insertTab(int index, QWidget* widget, const QString& label) {
  if (!widget) {
    return -1;
  }

  widget->setParent(this);
  TextEdit* textEdit = qobject_cast<TextEdit*>(widget);
  if (textEdit) {
    connect(textEdit, &TextEdit::pathUpdated, this,
            [=](const QString& path) { setTabText(indexOf(textEdit), getFileNameFrom(path)); });
    connect(textEdit, &TextEdit::modificationChanged, this, &TabView::updateTabTextBasedOn);
    connect(textEdit, &TextEdit::fileDropped, this, &TabView::open);
  }
  int result = QTabWidget::insertTab(index, widget, label);

  if (count() == 1 && result >= 0) {
    m_activeView = widget;
  }

  widget->setFocus();

  return result;
}

int TabView::open(const QString& path) {
  qDebug() << "TabView::open(" << path << ")";
  int index = indexOfPath(path);
  if (index >= 0) {
    setCurrentIndex(index);
    return index;
  }

  auto newDoc = DocumentManager::singleton().create(path);
  if (!newDoc) {
    return -1;
  }

  if (count() == 1) {
    TextEdit* textEdit = qobject_cast<TextEdit*>(currentWidget());
    if (textEdit && !textEdit->document()->isModified() && textEdit->document()->path().isEmpty()) {
      qDebug() << "trying to replace an empty doc with a new one";
      textEdit->setDocument(newDoc);
      textEdit->setPath(path);
      return currentIndex();
    }
  }

  TextEdit* view = new TextEdit(this);
  view->setDocument(newDoc);
  auto newIndex = addTab(view, getFileNameFrom(path));

  setTabToolTip(newIndex, path);

  // restore modification state for an existing modified document
  if (newDoc->isModified()) {
    emit view->modificationChanged(true);
  }
  return newIndex;
}

bool TabView::closeTab(int index) {
  return closeTab(widget(index));
}

QList<QWidget*> TabView::widgets() const {
  QList<QWidget*> widgets;
  for (int i = 0; i < count(); i++) {
    widgets.append(widget(i));
  }

#ifdef QT_DEBUG
  Q_ASSERT(widgets.size() == count());
  for (int i = 0; i < widgets.size(); i++) {
    Q_ASSERT(widgets.at(i));
  }
#endif

  return widgets;
}

void TabView::addNewTab() {
  if (auto doc = Document::createBlank()) {
    TextEdit* view = new TextEdit(this);
    std::shared_ptr<Document> newDoc(doc);
    view->setDocument(std::move(newDoc));
    addTab(view, DocumentManager::DEFAULT_FILE_NAME);

    // calling setModified(true) immediately doesn't emit modificationChanged
    QTimer::singleShot(0, this, [=] {
      Q_ASSERT(doc);
      doc->setModified(true);
    });
  }
}

QWidget* TabView::widget(int index) const {
  return QTabWidget::widget(index);
}

bool TabView::closeAllTabs() {
  if (count() == 0) {
    emit allTabRemoved();
  } else {
    std::list<QWidget*> widgets;
    for (int i = 0; i < count(); i++) {
      widgets.push_back(widget(i));
    }

    for (auto w : widgets) {
      bool isSuccess = closeTab(w);
      if (!isSuccess)
        return false;
    }
  }

  return true;
}

int TabView::indexOfPath(const QString& path) {
  //  qDebug("TabView::indexOfPath(%s)", qPrintable(path));
  for (int i = 0; i < count(); i++) {
    TextEdit* v = qobject_cast<TextEdit*>(widget(i));
    if (!v) {
      continue;
    }

    QString path2 = v->path();
    if (!path2.isEmpty() && path == path2) {
      return i;
    }
  }

  return -1;
}

void TabView::detachTabStarted(int index, const QPoint&) {
  qDebug("DetachTabStarted");
  m_tabDragging = true;
  DraggingTabInfo::setWidget(widget(index));
  DraggingTabInfo::setTabText(tabText(index));
  removeTab(index);
  Q_ASSERT(DraggingTabInfo::widget());
}

void TabView::detachTabEntered(const QPoint& enterPoint) {
  qDebug("DetachTabEntered");
  qDebug() << "tabBar()->mapToGlobal(QPoint(0, 0)):" << tabBar()->mapToGlobal(QPoint(0, 0));
  QPoint relativeEnterPos = enterPoint - tabBar()->mapToGlobal(QPoint(0, 0));
  int index = tabBar()->tabAt(relativeEnterPos);
  int newIndex = insertTab(index, DraggingTabInfo::widget(), DraggingTabInfo::tabText());
  DraggingTabInfo::setWidget(nullptr);
  m_tabDragging = false;
  tabRemoved(-1);
  QPoint tabCenterPos = tabBar()->tabRect(newIndex).center();

  qDebug() << "tabCenterPos:" << tabCenterPos << "enterPoint:" << enterPoint
           << "relativeEnterPos:" << relativeEnterPos;
  m_tabBar->startMovingTab(tabCenterPos);
}

void TabView::tabInserted(int index) {
  setCurrentIndex(index);
  QTabWidget::tabInserted(index);
}

void TabView::tabRemoved(int) {
  if (count() == 0 && !m_tabDragging) {
    emit allTabRemoved();
  }
}

void TabView::mouseReleaseEvent(QMouseEvent* event) {
  qDebug("mouseReleaseEvent in TabView");
  QTabWidget::mouseReleaseEvent(event);
}

void TabView::changeActiveView(int index) {
  // This lambda is called after m_tabbar is deleted when shutdown.
  if (index < 0)
    return;

  qDebug("currentChanged. index: %i, tab count: %i", index, count());
  if (auto w = widget(index)) {
    setActiveView(w);
  } else {
    qDebug("active view is null");
    setActiveView(nullptr);
  }
}

void TabView::setActiveView(QWidget* activeView) {
  if (m_activeView != activeView) {
    QWidget* oldView = m_activeView;
    m_activeView = activeView;
    emit activeViewChanged(oldView, activeView);
  }
}

void TabView::setTheme(const Theme* theme) {
  qDebug("TabView theme is changed");
  if (!theme) {
    qWarning("theme is null");
    return;
  }

  if (theme->tabViewSettings != nullptr) {
    QString style;
    ColorSettings* tabViewSettings = theme->tabViewSettings.get();

    style = QString("background-color: %1;")
                .arg(Util::qcolorForStyleSheet(tabViewSettings->value("background")));
    this->setStyleSheet(style);
  }
}

void TabView::removeTabAndWidget(int index) {
  if (auto w = widget(index)) {
    if (w == m_activeView) {
      m_activeView = nullptr;
    }
    w->deleteLater();
  }
  removeTab(index);
}

bool TabView::closeTab(QWidget* widget) {
  TextEdit* textEdit = qobject_cast<TextEdit*>(widget);
  // close an empty untitled document even if it's modified state because that's default
  if (textEdit && !App::instance()->isQuitting() &&
      (isUntitledAndNotEmpty(textEdit->document()) ||
       isNotUntitledAndModified(textEdit->document()))) {
    QMessageBox msgBox;
    msgBox.setText(tr("Do you want to save the changes made to the document %1?")
                       .arg(getFileNameFrom(textEdit->path())));
    msgBox.setInformativeText(tr("Your changes will be lost if you don’t save them."));
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);
    msgBox.setIconPixmap(QIcon(":/app_icon_064.png").pixmap(64, 64));
    int ret = msgBox.exec();
    switch (ret) {
      case QMessageBox::Save:
        textEdit->save(true);
        break;
      case QMessageBox::Discard:
        break;
      case QMessageBox::Cancel:
        return false;
      default:
        Q_ASSERT(false);
        qWarning("ret is invalid");
        return false;
    }
  }
  removeTabAndWidget(indexOf(widget));

  // Focus to the current widget after closing a tab
  if (QWidget* w = currentWidget()) {
    w->setFocus();
  }
  return true;
}

TabView::CloseTabIncludingDocResult TabView::closeTabIncludingDoc(core::Document* doc) {
  auto widgetList = widgets();
  auto iter = widgetList.begin();
  while (iter != widgetList.end()) {
    if (TextEdit* textEdit = qobject_cast<TextEdit*>(*iter)) {
      if (textEdit && textEdit->document() == doc) {
        int count = this->count();
        bool removed = closeTab(textEdit);

        // User cancels
        if (!removed) {
          return CloseTabIncludingDocResult::UserCanceled;
        }

        if (count == 1 && removed) {
          return CloseTabIncludingDocResult::AllTabsRemoved;
        }
        iter = widgetList.erase(iter);
      } else {
        ++iter;
      }
    }
  }

  return CloseTabIncludingDocResult::Finished;
}

void TabView::focusTabContent(int index) {
  if (QWidget* w = widget(index)) {
    w->setFocus();
  }
}

void TabView::updateTabTextBasedOn(bool changed) {
  if (TextEdit* textEdit = qobject_cast<TextEdit*>(QObject::sender())) {
    int index = indexOf(textEdit);
    QString text = tabText(index);
    if (changed) {
      setTabText(index, text + "*");
    } else if (text.endsWith('*')) {
      text.chop(1);
      setTabText(index, text);
    }
  } else {
    qDebug("sender is null or not TextEdit");
  }
}

void TabView::detachTabFinished(const QPoint& newWindowPos, bool isFloating) {
  qDebug() << "DetachTab."
           << "newWindowPos:" << newWindowPos << "isFloating:" << isFloating;

  if (isFloating) {
    Window* newWindow = new Window();
    newWindow->move(newWindowPos);
    newWindow->show();
    if (DraggingTabInfo::widget()) {
      newWindow->activeTabView()->addTab(DraggingTabInfo::widget(), DraggingTabInfo::tabText());
      DraggingTabInfo::setWidget(nullptr);
    } else {
      qWarning("dragging widget is null");
    }
  }

  m_tabDragging = false;
  // call tabRemoved to emit allTabRemoved if this had only one tab before drag (it's empty now)
  tabRemoved(-1);
}

void TabView::saveState(QSettings& settings) {
  settings.beginGroup(TabView::staticMetaObject.className());
  settings.beginWriteArray(PATHS_PREFIX);
  int arrayIndex = 0;
  for (int i = 0; i < count(); i++) {
    if (TextEdit* textEdit = qobject_cast<TextEdit*>(widget(i))) {
      settings.setArrayIndex(arrayIndex);
      textEdit->saveState(settings);
      settings.setValue(TAB_TEXT_PREFIX, tabText(i).toStdString().c_str());
      arrayIndex++;
    }
  }
  settings.endArray();
  settings.endGroup();
}

void TabView::loadState(QSettings& settings) {
  settings.beginGroup(TabView::staticMetaObject.className());

  // get array size.
  int size = settings.beginReadArray(PATHS_PREFIX);
  scoped_guard guard([&] {
    settings.endArray();
    settings.endGroup();
  });

  // restore tab information.
  for (int i = 0; i < size; i++) {
    settings.setArrayIndex(i);

    if (settings.childGroups().contains(TextEdit::staticMetaObject.className())) {
      auto textEdit = new TextEdit(this);
      if (textEdit) {
        textEdit->loadState(settings);
        auto newIndex = addTab(textEdit, getFileNameFrom(textEdit->path()));
        setTabToolTip(newIndex, textEdit->path());
        if (settings.contains(TAB_TEXT_PREFIX)) {
          auto tabTextVar = settings.value(TAB_TEXT_PREFIX);
          if (tabTextVar.canConvert<QString>()) {
            setTabText(i, tabTextVar.toString());
          }
        }
      }
    }
  }
}

QString TabView::tabTextWithoutModificationState(int index) const {
  const auto& text = QTabWidget::tabText(index);
  int lastIndexOfAsterisk = text.size() - 1;
  while (lastIndexOfAsterisk >= 0 && text[lastIndexOfAsterisk] == '*') {
    lastIndexOfAsterisk--;
  }

  return text.left(lastIndexOfAsterisk + 1);
}
