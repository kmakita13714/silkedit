#include <qDebug>

#include "OpenRecentItemManager.h"
#include "DocumentManager.h"
#include "SilkApp.h"
#include "TabView.h"
#include "CommandAction.h"
#include "commands/ReopenLastClosedFileCommand.h"

void OpenRecentItemManager::clear() {
  m_recentItems.clear();
  updateOpenRecentItems();
}

void OpenRecentItemManager::reopenLastClosedFile() {
  for (auto& path : m_recentItems) {
    auto tabView = SilkApp::activeTabView();
    if (tabView->indexOfPath(path) < 0) {
      tabView->open(path);
      return;
    }
  }
}

void OpenRecentItemManager::addOpenRecentItem(const QString& path) {
  if (path.isEmpty()) {
    qDebug("path is empty");
    return;
  }

  auto foundIter = std::find(m_recentItems.begin(), m_recentItems.end(), path);
  if (foundIter != m_recentItems.end()) {
    // Move found item to top
    m_recentItems.splice(m_recentItems.begin(), m_recentItems, foundIter);
    qDebug() << path << "is already in recent file list";
  } else {
    m_recentItems.push_front(path);
  }

  updateOpenRecentItems();
}

OpenRecentItemManager::OpenRecentItemManager() : m_openRecentMenu(new QMenu(tr("Open Recent"))) {
  m_reopenLastClosedFileAction = new CommandAction(QObject::tr("&Reopen Last Closed File"),
                                                   ReopenLastClosedFileCommand::name,
                                                   m_openRecentMenu.get());
  m_openRecentMenu->addAction(m_reopenLastClosedFileAction);
  m_openRecentMenu->addSeparator();

  for (int i = 0; i < MAX_RECENT_ITEMS; i++) {
    auto action = new OpenRecentAction(m_openRecentMenu.get());
    action->setVisible(false);
    m_recentItemActions[i] = action;
    m_openRecentMenu->addAction(action);
  }

  m_openRecentMenu->addSeparator();
  m_clearRecentItemListAction = new ClearRecentItemListAction(m_openRecentMenu.get());
  m_openRecentMenu->addAction(m_clearRecentItemListAction);

  updateOpenRecentItems();
}

void OpenRecentItemManager::updateOpenRecentItems() {
  // delete extra recent items
  while (m_recentItems.size() > MAX_RECENT_ITEMS) {
    m_recentItems.pop_back();
  }

  m_clearRecentItemListAction->setEnabled(m_recentItems.empty() ? false : true);
  m_reopenLastClosedFileAction->setEnabled(m_recentItems.empty() ? false : true);

  int index = 0;
  for (auto& item : m_recentItems) {
    m_recentItemActions[index]->setText(item);
    m_recentItemActions[index]->setData(item);
    m_recentItemActions[index]->setVisible(true);
    index++;
  }

  // Hide empty recent item actions
  for (int i = index; i < MAX_RECENT_ITEMS; i++) {
    m_recentItemActions[i]->setVisible(false);
  }
}

// OpenRecentAction

OpenRecentAction::OpenRecentAction(QObject* parent) : QAction(parent) {
  QObject::connect(this, &QAction::triggered, [this]() {
    if (data().isValid()) {
      DocumentManager::open(data().toString());
    }
  });
}

// ClearRecentItemListAction

ClearRecentItemListAction::ClearRecentItemListAction(QObject* parent)
    : QAction(tr("Clear List"), parent) {
  QObject::connect(
      this, &QAction::triggered, [this]() { OpenRecentItemManager::singleton().clear(); });
}