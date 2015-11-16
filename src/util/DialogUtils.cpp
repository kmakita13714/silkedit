﻿#include <QApplication>
#include <QFileDialog>

#include "DialogUtils.h"
#include "core/Util.h"

using core::Util;

QStringList DialogUtils::showDialog(const QString& caption, DialogUtils::MODE mode) {
  switch (mode) {
    case MODE::FileAndDirectory:
      return showDialogImpl(caption, QFileDialog::AnyFile);
    case MODE::Files:
      return showDialogImpl(caption, QFileDialog::ExistingFiles);
    case MODE::Directory:
      return showDialogImpl(caption, QFileDialog::Directory, QFileDialog::ShowDirsOnly);
    default:
      qWarning("invalid mode: %d", static_cast<int>(mode));
      return QStringList();
  }
}

QStringList DialogUtils::showDialogImpl(const QString& caption,
                                        QFileDialog::FileMode fileMode,
                                        QFileDialog::Options options) {
  // On Windows, native dialog sets QApplication::activeWindow() to NULL. We need to store and
  // restore it after closing the dialog.
  // https://bugreports.qt.io/browse/QTBUG-38414
  QWidget* activeWindow = QApplication::activeWindow();
  QFileDialog dialog(nullptr, caption);
  dialog.setFileMode(fileMode);
  dialog.setOptions(options);
  // NOTE: event loop is blocked by native dialog
  if (dialog.exec()) {
    QApplication::setActiveWindow(activeWindow);
    QStringList paths;
    foreach (const QString& path, dialog.selectedFiles()) {
      paths.append(QDir::toNativeSeparators(path));
    }
    return paths;
  }
  return QStringList();
}
