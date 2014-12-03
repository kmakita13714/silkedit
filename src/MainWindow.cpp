#include <QFileDialog>
#include <QMainWindow>
#include <QApplication>
#include <QDebug>
#include <QBoxLayout>

#include "API.h"
#include "MainWindow.h"
#include "STabWidget.h"

MainWindow::MainWindow(QWidget* parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags), m_activeTabWidget(nullptr), m_layout(new QBoxLayout(QBoxLayout::LeftToRight)) {
  qDebug("creating MainWindow");

  setWindowTitle(QObject::tr("SilkEdit"));

  auto tabWidget = createTabWidget();
  m_layout->addWidget(tabWidget);
  m_layout->setContentsMargins(0,0,0,0);
  QWidget *window = new QWidget(this);
  // window becomes parent of this layout by setLayout
  window->setLayout(m_layout);
  setCentralWidget(window);
  m_activeTabWidget = tabWidget;
}

STabWidget* MainWindow::createTabWidget()
{
  auto tabWidget = new STabWidget(this);
  QObject::connect(tabWidget, &STabWidget::allTabRemoved, [this, tabWidget]() {
    qDebug() << "allTabRemoved";
    m_tabWidgets.removeOne(tabWidget);

    if (m_tabWidgets.size() == 0) {
      if (tabWidget->tabDragging()) {
        hide();
      } else {
        close();
      }
    }
  });

  m_tabWidgets.append(tabWidget);

  return tabWidget;
}

MainWindow* MainWindow::create(QWidget* parent, Qt::WindowFlags flags) {
  MainWindow* window = new MainWindow(parent, flags);
  window->resize(1280, 720);
  s_windows.append(window);
  return window;
}

MainWindow::~MainWindow() {
  qDebug("~MainWindow");
}

void MainWindow::show() {
  QMainWindow::show();
  QApplication::setActiveWindow(this);
}

void MainWindow::close() {
  if (s_windows.removeOne(this)) {
    deleteLater();
  }
}

void MainWindow::addTabWidgetHorizontally(QWidget* widget, const QString& label)
{
  auto tabWidget = createTabWidget();
  if (m_layout->direction() == QBoxLayout::LeftToRight) {
    tabWidget->addTab(widget, label);
    m_layout->addWidget(tabWidget);
  }

  m_activeTabWidget = tabWidget;
}

QList<MainWindow*> MainWindow::s_windows;
