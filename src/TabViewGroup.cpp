#include <QDebug>
#include <QVBoxLayout>

#include "TabViewGroup.h"
#include "TabView.h"
#include "Splitter.h"
#include "TextEditView.h"

namespace {
QSplitter* findItemFromSplitter(QSplitter* splitter, QWidget* item) {
  for (int i = 0; i < splitter->count(); i++) {
    QSplitter* subSplitter = qobject_cast<QSplitter*>(splitter->widget(i));
    if (subSplitter) {
      QSplitter* foundSplitter = findItemFromSplitter(subSplitter, item);
      if (foundSplitter)
        return foundSplitter;
    }
    QWidget* widget = splitter->widget(i);
    if (widget && widget == item)
      return splitter;
  }

  return nullptr;
}
}

TabViewGroup::TabViewGroup(QWidget* parent)
    : QWidget(parent), m_activeTabView(nullptr), m_rootSplitter(new HSplitter(this)) {
  auto tabView = createTabView();
  // Note: The ownership of tabView is transferred to the splitter, and it's the splitter's
  // responsibility to delete it.
  m_rootSplitter->addWidget(tabView);
  setActiveTab(tabView);
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_rootSplitter);
  setLayout(layout);
}

void TabViewGroup::setActiveTab(TabView* tabView) {
  if (m_activeTabView != tabView) {
    TabView* oldtabView = m_activeTabView;
    m_activeTabView = tabView;
    emit activeTabViewChanged(oldtabView, tabView);
  }
}

void TabViewGroup::saveAllTabs() {
  for (auto tabView : m_tabViews) {
    tabView->saveAllTabs();
  }
}

bool TabViewGroup::closeAllTabs() {
  while (!m_tabViews.empty()) {
    bool isSuccess = m_tabViews.front()->closeAllTabs();
    if (!isSuccess)
      return false;
  }

  return true;
}

void TabViewGroup::splitTabHorizontally() {
  splitTab(std::bind(
      &TabViewGroup::addTabViewHorizontally, this, std::placeholders::_1, std::placeholders::_2));
}

void TabViewGroup::splitTabVertically() {
  splitTab(std::bind(
      &TabViewGroup::addTabViewVertically, this, std::placeholders::_1, std::placeholders::_2));
}

TabView* TabViewGroup::createTabView() {
  auto tabView = new TabView();
  QObject::connect(tabView, &TabView::allTabRemoved, [this, tabView]() {
    qDebug() << "allTabRemoved";
    removeTabView(tabView);

    if (m_tabViews.size() == 0) {
      if (tabView->tabDragging()) {
        hide();
      } else {
        close();
      }
    }
  });

  m_tabViews.push_back(tabView);

  return tabView;
}

void TabViewGroup::removeTabView(TabView* widget) {
  m_tabViews.remove(widget);
  widget->hide();
  widget->deleteLater();
}

void TabViewGroup::addTabViewHorizontally(QWidget* widget, const QString& label) {
  addTabView(widget, label, Qt::Orientation::Horizontal, Qt::Orientation::Vertical);
}

void TabViewGroup::addTabViewVertically(QWidget* widget, const QString& label) {
  addTabView(widget, label, Qt::Orientation::Vertical, Qt::Orientation::Horizontal);
}

void TabViewGroup::addTabView(QWidget* widget,
                              const QString& label,
                              Qt::Orientation activeSplitterDirection,
                              Qt::Orientation newDirection) {
  auto tabView = createTabView();
  tabView->addTab(widget, label);

  QSplitter* splitterInActiveTabViewGroup = findItemFromSplitter(m_rootSplitter, m_activeTabView);
  if (splitterInActiveTabViewGroup->orientation() == activeSplitterDirection) {
    int index = splitterInActiveTabViewGroup->indexOf(m_activeTabView);
    Q_ASSERT(index >= 0);
    Splitter* splitter = new Splitter(newDirection);
    splitter->addWidget(m_activeTabView);
    splitter->addWidget(tabView);
    splitterInActiveTabViewGroup->insertWidget(index, splitter);
  } else {
    splitterInActiveTabViewGroup->addWidget(tabView);
  }
}

void TabViewGroup::splitTab(std::function<void(QWidget*, const QString&)> func) {
  if (m_activeTabView) {
    TextEditView* activeEditView = m_activeTabView->activeEditView();
    QString label = m_activeTabView->tabText(m_activeTabView->currentIndex());
    if (activeEditView) {
      TextEditView* anotherEditView = activeEditView->clone();
      func(anotherEditView, label);
    }
  }
}