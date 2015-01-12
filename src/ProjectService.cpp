#include "ProjectService.h"
#include "ProjectTreeView.h"
#include "MainWindow.h"
#include "API.h"
#include "TabWidget.h"

bool ProjectService::open(const QString& dirName) {
  if (!QDir(dirName).exists()) {
    qWarning("%s doesn't exist", qPrintable(dirName));
    return false;
  }

  MainWindow* window = MainWindow::createWithNewFile();
  if (window) {
    window->show();
    return window->openDir(dirName);
  } else {
    qWarning("active window or its project view is null");
    return false;
  }
}
