#pragma once

#include <string>
#include <functional>
#include <memory>
#include <list>
#include <QMainWindow>

#include "macros.h"
#include "UniqueObject.h"

class TabView;
class QBoxLayout;
class StatusBar;
class Splitter;
class ProjectTreeView;
class TabViewGroup;
class FindReplaceView;
class TextEditView;

class Window : public QMainWindow, public UniqueObject<Window> {
  Q_OBJECT
  DISABLE_COPY(Window)

 public:
  static Window* create(QWidget* parent = nullptr, Qt::WindowFlags flags = nullptr);
  static Window* createWithNewFile(QWidget* parent = nullptr, Qt::WindowFlags flags = nullptr);
  static QList<Window*> windows() { return s_windows; }
  static void loadMenu(const std::string& ymlPath);

  ~Window();
  DEFAULT_MOVE(Window)

  // accessor
  TabViewGroup* tabViewGroup() { return m_tabViewGroup; }
  TabView* activeTabView();

  void show();
  void closeEvent(QCloseEvent* event) override;
  bool openDir(const QString& dirPath);
  void openFindAndReplacePanel();
  void hideFindReplacePanel();

signals:
  void activeEditViewChanged(TextEditView* oldEditView, TextEditView* newEditView);

 protected:
  friend struct UniqueObject<Window>;

  static void request(Window* window, const std::string& method, msgpack::rpc::msgid_t msgId, const msgpack::object& obj);
  static void notify(Window* window, const std::string& method, const msgpack::object& obj);

 private:
  static QList<Window*> s_windows;

  explicit Window(QWidget* parent = nullptr, Qt::WindowFlags flags = nullptr);

  Splitter* m_rootSplitter;
  TabViewGroup* m_tabViewGroup;
  StatusBar* m_statusBar;
  ProjectTreeView* m_projectView;
  FindReplaceView* m_findReplaceView;

 private slots:
  void updateConnection(TabView* oldTab, TabView* newTab);
  void updateConnection(TextEditView* oldEditView, TextEditView* newEditView);
  void emitActiveEditViewChanged(TabView* oldTabView, TabView* newTabView);
};
