#include <memory>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>

#include "STabWidget.h"
#include "TextEditView.h"
#include "KeymapService.h"
#include "STabBar.h"
#include "MainWindow.h"
#include "DraggingTabInfo.h"
#include "SilkApp.h"
#include "DocumentService.h"

namespace {
QString getFileNameFrom(const QString& path) {
  QFileInfo info(path);
  return info.fileName();
}
}

STabWidget::STabWidget(QWidget* parent)
    : QTabWidget(parent), m_activeEditView(nullptr), m_tabBar(new STabBar(this)), m_tabDragging(false) {
  connect(m_tabBar,
          SIGNAL(onDetachTabStarted(int, const QPoint&)),
          this,
          SLOT(detachTabStarted(int, const QPoint&)));

  connect(m_tabBar,
          SIGNAL(onDetachTabEntered(const QPoint&)),
          this,
          SLOT(detachTabEntered(const QPoint&)));

  connect(m_tabBar,
          SIGNAL(onDetachTabFinished(const QPoint&)),
          this,
          SLOT(detachTabFinished(const QPoint&)));

  setTabBar(m_tabBar);
  setMovable(true);
  setDocumentMode(true);
  setTabsClosable(true);

  QObject::connect(this, &QTabWidget::currentChanged, [this](int index) {
    // This lambda is called after m_tabbar is deleted when shutdown.
    if (index < 0)
      return;

    qDebug("currentChanged. index: %i, tab count: %i", index, count());
    if (auto w = widget(index)) {
      setActiveEditView(qobject_cast<TextEditView*>(w));
    } else {
      qDebug("active edit view is null");
      setActiveEditView(nullptr);
    }
  });

  QObject::connect(this, &QTabWidget::tabCloseRequested, [this](int index) {
    qDebug("tab widget (index %i) is deleted", index);
    removeTabAndWidget(index);
  });
}

STabWidget::~STabWidget() {
  qDebug("~STabWidget");
  disconnect(m_tabBar,
             SIGNAL(onDetachTabStarted(int, const QPoint&)),
             this,
             SLOT(detachTabStarted(int, const QPoint&)));

  disconnect(m_tabBar,
             SIGNAL(onDetachTabEntered(const QPoint&)),
             this,
             SLOT(detachTabEntered(const QPoint&)));

  disconnect(m_tabBar,
             SIGNAL(onDetachTabFinished(const QPoint&)),
             this,
             SLOT(detachTabFinished(const QPoint&)));
}

int STabWidget::addTab(QWidget* page, const QString& label) {
  return insertTab(-1, page, label);
}

int STabWidget::insertTab(int index, QWidget* w, const QString& label) {
  w->setParent(this);
  TextEditView* editView = qobject_cast<TextEditView*>(w);
  if (editView) {
    QObject::connect(editView, &TextEditView::pathUpdated, [this, editView](const QString& path) {
      setTabText(indexOf(editView), getFileNameFrom(path));
    });
    QObject::connect(
        editView, &TextEditView::saved, [editView]() { editView->document()->setModified(false); });
    connect(editView, SIGNAL(modificationChanged(bool)), this, SLOT(updateTabTextBasedOn(bool)));
  } else {
    qDebug("inserted widget is not TextEditView");
  }
  return QTabWidget::insertTab(index, w, label);
}

int STabWidget::open(const QString& path) {
  qDebug("STabWidget::open(%s)", qPrintable(path));
  int index = indexOfPath(path);
  if (index >= 0) {
    setCurrentIndex(index);
    return index;
  }

  std::shared_ptr<Document> newDoc(Document::create(path));
  if (!newDoc) {
    return -1;
  }
  newDoc->setModified(false);

  if (count() == 1) {
    TextEditView* editView = qobject_cast<TextEditView*>(currentWidget());
    if (editView && !editView->document()->isModified() && editView->document()->isEmpty()) {
      qDebug("trying to replace am empty doc with a new one");
      editView->setDocument(std::move(newDoc));
      editView->setPath(path);
      return currentIndex();
    }
  }

  TextEditView* view = new TextEditView(this);
  view->setDocument(std::move(newDoc));
  return addTab(view, getFileNameFrom(path));
}

void STabWidget::addNew() {
  TextEditView* view = new TextEditView(this);
  std::shared_ptr<Document> newDoc(Document::createBlank());
  view->setDocument(std::move(newDoc));
  addTab(view, DocumentService::DEFAULT_FILE_NAME);
}

void STabWidget::saveAllTabs() {
  for (int i = 0; i < count(); i++) {
    auto editView = qobject_cast<TextEditView*>(widget(i));
    if (editView) {
      editView->save();
    }
  }
}

void STabWidget::closeActiveTab() {
  closeTab(currentWidget());
}

bool STabWidget::closeAllTabs() {
  std::list<QWidget*> widgets;
  for (int i = 0; i < count(); i++) {
    widgets.push_back(widget(i));
  }

  for (auto w : widgets) {
    bool isSuccess = closeTab(w);
    if (!isSuccess)
      return false;
  }

  return true;
}

void STabWidget::closeOtherTabs() {
  std::list<QWidget*> widgets;
  for (int i = 0; i < count(); i++) {
    if (i != currentIndex()) {
      widgets.push_back(widget(i));
    }
  }

  for (auto w : widgets) {
    closeTab(w);
  }
}

int STabWidget::indexOfPath(const QString& path) {
  //  qDebug("STabWidget::indexOfPath(%s)", qPrintable(path));
  for (int i = 0; i < count(); i++) {
    TextEditView* v = qobject_cast<TextEditView*>(widget(i));
    QString path2 = v->path();
    if (v && !path2.isEmpty() && path == path2) {
      return i;
    }
  }

  return -1;
}

void STabWidget::detachTabStarted(int index, const QPoint&) {
  qDebug("DetachTabStarted");
  m_tabDragging = true;
  DraggingTabInfo::setWidget(widget(index));
  DraggingTabInfo::setTabText(tabText(index));
  removeTab(index);
  Q_ASSERT(DraggingTabInfo::widget());
}

void STabWidget::detachTabEntered(const QPoint& enterPoint) {
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

void STabWidget::tabInserted(int index) {
  setCurrentIndex(index);
  QTabWidget::tabInserted(index);
}

void STabWidget::tabRemoved(int) {
  if (count() == 0) {
    emit allTabRemoved();
  }
}

void STabWidget::mouseReleaseEvent(QMouseEvent* event) {
  qDebug("mouseReleaseEvent in STabWidget");
  QTabWidget::mouseReleaseEvent(event);
}

void STabWidget::setActiveEditView(TextEditView *editView)
{
  m_activeEditView = editView;
  emit activeTextEditViewChanged(editView);
}

void STabWidget::removeTabAndWidget(int index) {
  if (auto w = widget(index)) {
    w->deleteLater();
  }
  removeTab(index);
}

bool STabWidget::closeTab(QWidget* w) {
  TextEditView* editView = qobject_cast<TextEditView*>(w);
  if (editView && editView->document()->isModified()) {
    QMessageBox msgBox;
    msgBox.setText(tr("Do you want to save the changes made to the document %1?")
                       .arg(getFileNameFrom(editView->path())));
    msgBox.setInformativeText(tr("Your changes will be lost if you don’t save them."));
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);
    msgBox.setIconPixmap(SilkApp::windowIcon().pixmap(64, 64));
    int ret = msgBox.exec();
    switch (ret) {
      case QMessageBox::Save:
        editView->save();
        break;
      case QMessageBox::Discard:
        break;
      case QMessageBox::Cancel:
        return false;
      default:
        qWarning("ret is invalid");
        return false;
    }
  } else {
    qDebug("widget is not TextEditView or not modified");
  }

  removeTabAndWidget(indexOf(w));
  return true;
}

void STabWidget::updateTabTextBasedOn(bool changed) {
  qDebug() << "updateTabTextBasedOn. changed:" << changed;
  if (QWidget* w = qobject_cast<QWidget*>(QObject::sender())) {
    int index = indexOf(w);
    QString text = tabText(index);
    if (changed) {
      setTabText(index, text + "*");
    } else if (text.endsWith('*')) {
      text.chop(1);
      setTabText(index, text);
    }
  } else {
    qDebug("sender is null or not QWidget");
  }
}

void STabWidget::detachTabFinished(const QPoint& dropPoint) {
  qDebug() << "DetachTab."
           << "dropPoint:" << dropPoint;
  MainWindow* window = MainWindow::create();
  window->move(dropPoint);
  window->show();
  if (DraggingTabInfo::widget()) {
    window->activeTabWidget()->addTab(DraggingTabInfo::widget(), DraggingTabInfo::tabText());
    DraggingTabInfo::setWidget(nullptr);
    m_tabDragging = false;
    tabRemoved(-1);
  } else {
    qWarning("draggign widget is null");
  }
}
