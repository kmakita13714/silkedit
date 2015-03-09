#include <memory>
#include <QFile>
#include <QTextStream>
#include <QTextDocument>
#include <QTextBlock>
#include <QFileDialog>
#include <QDebug>

#include "DocumentService.h"
#include "MainWindow.h"
#include "SilkApp.h"
#include "TabView.h"
#include "Document.h"

const QString DocumentService::DEFAULT_FILE_NAME = "untitled";

bool DocumentService::open(const QString& filename) {
  if (TabView* tabView = SilkApp::activeTabView(true)) {
    return tabView->open(filename) >= 0;
  } else {
    qWarning("active tab view is null");
    return false;
  }
}

bool DocumentService::save(Document* doc) {
  if (!doc) {
    qWarning("doc is null");
    return false;
  }

  if (doc->path().isEmpty()) {
    QString newFilePath = saveAs(doc);
    return !newFilePath.isEmpty();
  }

  QFile outFile(doc->path());
  if (outFile.open(QIODevice::WriteOnly)) {
    QTextStream out(&outFile);
    for (int i = 0; i < doc->blockCount(); i++) {
      if (i < doc->blockCount() - 1) {
        out << doc->findBlockByNumber(i).text() << endl;
      } else {
        // don't output a new line character in the last block.
        out << doc->findBlockByNumber(i).text();
      }
    }
    return true;
  }

  return false;
}

QString DocumentService::saveAs(Document* doc) {
  QString filePath = QFileDialog::getSaveFileName(nullptr, QObject::tr("Save As"), doc->path());
  if (!filePath.isEmpty()) {
    doc->setPath(filePath);
    save(doc);
  }

  return filePath;
}
