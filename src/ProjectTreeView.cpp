#include <QApplication>
#include <QDebug>
#include <QMenu>
#include <QContextMenuEvent>

#include "ProjectTreeView.h"
#include "DocumentService.h"

ProjectTreeView::ProjectTreeView(QWidget* parent) : QTreeView(parent) {
  setHeaderHidden(true);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setAttribute(Qt::WA_MacShowFocusRect, false);
  connect(this, SIGNAL(activated(QModelIndex)), this, SLOT(open(QModelIndex)));
}

bool ProjectTreeView::open(const QString& dirPath) {
  QDir targetDir(dirPath);
  if (targetDir.exists()) {
    QAbstractItemModel* prevModel = model();
    if (prevModel) {
      prevModel->deleteLater();
    }

    MyFileSystemModel* model = new MyFileSystemModel(this);
    model->setRootPath(dirPath);

    FilterModel* const filter = new FilterModel(this, dirPath);
    filter->setSourceModel(model);

    setModel(filter);
    if (targetDir.isRoot()) {
      setRootIndex(model->index(dirPath));
    } else {
      QDir parentDir(dirPath);
      parentDir.cdUp();
      QModelIndex rootIndex = filter->mapFromSource(model->index(parentDir.absolutePath()));
      setRootIndex(rootIndex);
    }

    expandAll();
    return true;
  } else {
    qWarning("%s doesn't exist", qPrintable(dirPath));
    return false;
  }
}

void ProjectTreeView::contextMenuEvent(QContextMenuEvent* event) {
  QMenu menu(this);
  menu.addAction(tr("Rename"), this, SLOT(rename()));
  menu.addAction(tr("Delete"), this, SLOT(remove()));
  menu.exec(event->globalPos());
}

bool ProjectTreeView::edit(const QModelIndex& index,
                           QAbstractItemView::EditTrigger trigger,
                           QEvent* event) {
  // disable renaming by double click
  if (trigger == QAbstractItemView::DoubleClicked)
    return false;
  return QTreeView::edit(index, trigger, event);
}

void ProjectTreeView::open(QModelIndex index) {
  if (!index.isValid()) {
    qWarning("index is invalid");
    return;
  }

  if (FilterModel* filter = qobject_cast<FilterModel*>(model())) {
    if (MyFileSystemModel* fsModel = qobject_cast<MyFileSystemModel*>(filter->sourceModel())) {
      QString filePath = fsModel->filePath(filter->mapToSource(index));
      DocumentService::open(filePath);
    }
  }
}

void ProjectTreeView::rename() {
  QTreeView::edit(currentIndex());
}

void ProjectTreeView::remove() {
  QModelIndexList indices = selectedIndexes();
  foreach (const QModelIndex& filterIndex, indices) {
    if (FilterModel* filter = qobject_cast<FilterModel*>(model())) {
      if (MyFileSystemModel* fsModel = qobject_cast<MyFileSystemModel*>(filter->sourceModel())) {
        QModelIndex index = filter->mapToSource(filterIndex);
        QString filePath = fsModel->filePath(index);
        QFileInfo info(filePath);
        if (info.isFile()) {
          fsModel->remove(index);
        } else if (info.isDir()) {
          fsModel->rmdir(index);
        } else {
          qWarning("%s is neither file nor directory", qPrintable(filePath));
        }
      }
    }
  }
}

MyFileSystemModel::MyFileSystemModel(QObject* parent) : QFileSystemModel(parent) {
  setReadOnly(false);
  removeColumns(1, 3);
}

int MyFileSystemModel::columnCount(const QModelIndex&) const {
  return 1;
}

QVariant MyFileSystemModel::data(const QModelIndex& index, int role) const {
  if (index.column() == 0) {
    return QFileSystemModel::data(index, role);
  } else {
    return QVariant();
  }
}

ProjectTreeView::~ProjectTreeView() {
  qDebug("~ProjectTreeView");
}
