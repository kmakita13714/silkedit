#include <QWidget>

#include "SilkApp.h"
#include "TextEditView.h"
#include "MainWindow.h"
#include "STabWidget.h"

namespace {
template <typename T>
T findParent(QWidget* widget) {
  if (!widget) return nullptr;

  T desiredWidget = qobject_cast<T>(widget->parentWidget());
  if (desiredWidget) return desiredWidget;
  return findParent<T>(widget->parentWidget());
}
}

SilkApp::SilkApp(int &argc, char **argv): QApplication(argc, argv)
{
  // Track active STabWidget
  QObject::connect(this, &QApplication::focusChanged, [](QWidget* , QWidget* now){
    qDebug("focusChanged");
    if (TextEditView* editView = qobject_cast<TextEditView*>(now)) {
      if (STabWidget* tabWidget = findParent<STabWidget*>(editView)) {
        if (MainWindow* window = qobject_cast<MainWindow*>(tabWidget->window())) {
          qDebug("window->setActiveTabWidget");
          window->setActiveTabWidget(tabWidget);
        } else {
          qDebug("top window is not MainWindow");
        }
      } else {
        qDebug("can't find STabWidget in ancestor");
      }
    } else {
      qDebug("now is not TextEditView");
    }
  });
}
