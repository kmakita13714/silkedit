// Qt includes
#include <qapplication.h>
#include <qevent.h>
#include <qimage.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qpaintdevice.h>
#include <qtabbar.h>
#include <QTabWidget>
#include <qboxlayout.h>
#include <qmessagebox.h>
#include <qdrag.h>
#include <qmimedata.h>

#include "STabBar.h"
#include "STabWidget.h"

STabBar::STabBar(QWidget* parent) : QTabBar(parent) {
  setAcceptDrops(true);

  setElideMode(Qt::ElideRight);
  setSelectionBehaviorOnRemove(QTabBar::SelectLeftTab);

  setMovable(true);
  setDocumentMode(true);
  setTabsClosable(true);
}

//////////////////////////////////////////////////////////////////////////////
void STabBar::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    m_dragStartPos = event->pos();
  }
  m_dragDropedPos.setX(0);
  m_dragDropedPos.setY(0);
  m_dragMovedPos.setX(0);
  m_dragMovedPos.setY(0);

  m_dragInitiated = false;

  QTabBar::mousePressEvent(event);
}

//////////////////////////////////////////////////////////////////////////////
void STabBar::mouseMoveEvent(QMouseEvent* event) {
  // Distinguish a drag
  if (!m_dragStartPos.isNull() &&
      ((event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance())) {
    m_dragInitiated = true;
  }

  // The left button is pressed
  // And the move could also be a drag
  // And the mouse moved outside the tab bar
  if (((event->buttons() & Qt::LeftButton)) && m_dragInitiated &&
      (!geometry().contains(event->pos()))) {
    // Stop the move to be able to convert to a drag
    {
      QMouseEvent* finishMoveEvent = new QMouseEvent(
          QEvent::MouseMove, event->pos(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
      QTabBar::mouseMoveEvent(finishMoveEvent);
      delete finishMoveEvent;
      finishMoveEvent = NULL;
    }

    // Initiate Drag
    QDrag* drag = new QDrag(this);
    QMimeData* mimeData = new QMimeData;
    // a crude way to distinguish tab-reordering drops from other ones
    mimeData->setData("action", "application/tab-detach");
    drag->setMimeData(mimeData);

    // Create transparent screen dump
    QPixmap pixmap =
        QPixmap::grabWindow(dynamic_cast<STabWidget*>(parentWidget())->currentWidget()->winId())
            .scaled(640, 480, Qt::KeepAspectRatio);
    QPixmap targetPixmap(pixmap.size());
    QPainter painter(&targetPixmap);
    painter.setOpacity(0.5);
    painter.drawPixmap(0, 0, pixmap);
    painter.end();
    drag->setPixmap(targetPixmap);
    //    drag->setHotSpot (QPoint (20, 10));

    // Handle Detach and Move
    Qt::DropAction dragged = drag->exec(Qt::MoveAction | Qt::CopyAction);
    if (Qt::IgnoreAction == dragged) {
      event->accept();
      OnDetachTab(tabAt(m_dragStartPos), m_dragDropedPos);
    } else if (Qt::MoveAction == dragged) {
      if (!m_dragDropedPos.isNull()) {
        event->accept();
        OnMoveTab(tabAt(m_dragStartPos), tabAt(m_dragDropedPos));
      }
    }
    delete drag;
    drag = NULL;
  } else {
    QTabBar::mouseMoveEvent(event);
  }
}

//////////////////////////////////////////////////////////////////////////////
void STabBar::dragEnterEvent(QDragEnterEvent* event) {
  qDebug("dragEnterEvent");
  // Only accept if it's an tab-reordering request
  const QMimeData* m = event->mimeData();
  QStringList formats = m->formats();
  if (formats.contains("action") && (m->data("action") == "application/tab-detach")) {
    event->acceptProposedAction();
  }
  QTabBar::dragEnterEvent(event);
}

//////////////////////////////////////////////////////////////////////////////
void STabBar::dragMoveEvent(QDragMoveEvent* event) {
  // Only accept if it's an tab-reordering request
  const QMimeData* m = event->mimeData();
  QStringList formats = m->formats();
  if (formats.contains("action") && (m->data("action") == "application/tab-detach")) {
    m_dragMovedPos = event->pos();
    event->acceptProposedAction();
  }
  QTabBar::dragMoveEvent(event);
}

//////////////////////////////////////////////////////////////////////////////
void STabBar::dropEvent(QDropEvent* event) {
  // If a dragged Event is dropped within this widget it is not a drag but
  // a move.
  m_dragDropedPos = event->pos();
  QTabBar::dropEvent(event);
}
