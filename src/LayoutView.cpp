#include <memory>
#include <QTabWidget>
#include <QTabBar>
#include <QPlainTextDocumentLayout>
#include <QFileInfo>

#include "LayoutView.h"
#include "TextEditView.h"
#include "FileDocument.h"
#include "STabWidget.h"
#include "KeymapService.h"

LayoutView::LayoutView()
    : m_tabbar(new STabWidget(this)), m_layout(new QHBoxLayout), m_activeEditView(nullptr) {
  m_tabbar->setAcceptDrops(true);

  m_tabbar->setElideMode(Qt::ElideRight);
  //    m_tabbar->setSelectionBehaviorOnRemove (QTabBar::SelectLeftTab);

  m_tabbar->setMovable(true);
  m_tabbar->setDocumentMode(true);
  m_tabbar->setTabsClosable(true);

  m_layout->addWidget(m_tabbar.get());
  m_layout->setSpacing(0);
  m_layout->setMargin(0);
  // LayoutView takes ownership of this layout by calling setLayout
  setLayout(m_layout);
  setContentsMargins(0, 0, 0, 0);

  QObject::connect(m_tabbar.get(), &QTabWidget::currentChanged, [this](int index) {
    // This lambda is called after m_tabbar is deleted when shutdown.
    if (!m_tabbar || index < 0)
      return;

    qDebug("currentChanged. index: %i, tab count: %i", index, m_tabbar->count());
    if (auto w = m_tabbar->widget(index)) {
      m_activeEditView = qobject_cast<TextEditView*>(w);
    } else {
      qDebug("active edit view is null");
      m_activeEditView = nullptr;
    }
  });

  addNewDocument();
}

void LayoutView::addDocument(const QString& filename, QTextDocument* doc) {
  TextEditView* view;
  QString label("untitled");

  if (!filename.isEmpty()) {
    QFileInfo info(filename);
    label = info.fileName();
    view = new TextEditView(filename);
  } else {
    view = new TextEditView();
  }
  Q_ASSERT(view);
  if (doc) {
    view->setDocument(doc);
  }
  view->installEventFilter(&KeyHandler::singleton());
  m_tabbar->addTextEditView(view, label);
}

void LayoutView::addNewDocument() {
  addDocument("", nullptr);
}