﻿#include "crash_handler.h"
#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtCore/QCoreApplication>
#include <QString>
#include <QDebug>
#include <sstream>
#include "stringconv.h"

#if defined(Q_OS_LINUX)
#include "client/linux/handler/exception_handler.h"
#elif defined(Q_OS_WIN32)
#include "client/windows/handler/exception_handler.h"
#elif defined(Q_OS_MAC)
#include "client/mac/handler/exception_handler.h"
#endif

namespace Breakpad {
/************************************************************************/
/* CrashHandlerPrivate                                                  */
/************************************************************************/
class CrashHandlerPrivate {
 public:
  CrashHandlerPrivate() { pHandler = NULL; }

  ~CrashHandlerPrivate() { delete pHandler; }

  void InitCrashHandler(const QString& dumpPath);
  static google_breakpad::ExceptionHandler* pHandler;
  static bool bReportCrashesToSystem;
};

google_breakpad::ExceptionHandler* CrashHandlerPrivate::pHandler = NULL;
bool CrashHandlerPrivate::bReportCrashesToSystem = false;

/************************************************************************/
/* DumpCallback                                                         */
/************************************************************************/
#if defined(Q_OS_WIN32)
bool DumpCallback(const wchar_t* _dump_dir,
                  const wchar_t* _minidump_id,
                  void* context,
                  EXCEPTION_POINTERS* exinfo,
                  MDRawAssertionInfo* assertion,
                  bool success)
#elif defined(Q_OS_LINUX)
bool DumpCallback(const google_breakpad::MinidumpDescriptor& md, void* context, bool success)
#elif defined(Q_OS_MAC)
bool DumpCallback(const char* _dump_dir, const char* _minidump_id, void* context, bool success)
#endif
{
  Q_UNUSED(context);
#if defined(Q_OS_WIN32)
  Q_UNUSED(assertion);
  Q_UNUSED(exinfo);
#endif
  qDebug("BreakpadQt crash");

  /*
  NO STACK USE, NO HEAP USE THERE !!!
  Creating QString's, using qDebug, etc. - everything is crash-unfriendly.
  */

  QString command;
  QStringList arg;
#if defined(Q_OS_WIN)
  command = qApp->applicationDirPath() + "/crashreporter.exe";

  std::wstringstream dump_file;
  dump_file << _dump_dir << L"/" << _minidump_id << L".dmp";
  arg << string_util::stdWToQString(dump_file.str());

#elif defined(Q_OS_MAC)
  // open path.app
  command = qApp->applicationDirPath() + "/CrashReporter";

  std::stringstream dump_file;
  dump_file << _dump_dir << "/" << _minidump_id << ".dmp";
  arg << QString::fromStdString(dump_file.str());
#endif
  qDebug() << "Execute CrashReporter :" << command << arg;
  QProcess::startDetached(command, arg);

  return CrashHandlerPrivate::bReportCrashesToSystem ? success : true;
}

void CrashHandlerPrivate::InitCrashHandler(const QString& dumpPath) {
  if (pHandler != NULL)
    return;

#if defined(Q_OS_WIN32)
  std::wstring pathAsStr = (const wchar_t*)dumpPath.utf16();
  pHandler = new google_breakpad::ExceptionHandler(pathAsStr,
                                                   /*FilterCallback*/ 0, DumpCallback,
                                                   /*context*/
                                                   0, true);
#elif defined(Q_OS_LINUX)
  std::string pathAsStr = dumpPath.toStdString();
  google_breakpad::MinidumpDescriptor md(pathAsStr);
  pHandler = new google_breakpad::ExceptionHandler(md,
                                                   /*FilterCallback*/ 0, DumpCallback,
                                                   /*context*/ 0, true, -1);
#elif defined(Q_OS_MAC)
  std::string pathAsStr = dumpPath.toStdString();
  pHandler = new google_breakpad::ExceptionHandler(pathAsStr,
                                                   /*FilterCallback*/ 0, DumpCallback,
                                                   /*context*/
                                                   0, true, NULL);
#endif
}

/************************************************************************/
/* CrashHandler                                                         */
/************************************************************************/
CrashHandler* CrashHandler::instance() {
  static CrashHandler globalHandler;
  return &globalHandler;
}

CrashHandler::CrashHandler() {
  d = new CrashHandlerPrivate();
}

CrashHandler::~CrashHandler() {
  delete d;
}

void CrashHandler::setReportCrashesToSystem(bool report) {
  d->bReportCrashesToSystem = report;
}

bool CrashHandler::writeMinidump() {
  bool res = d->pHandler->WriteMinidump();
  if (res) {
    qDebug("BreakpadQt: writeMinidump() success.");
  } else {
    qWarning("BreakpadQt: writeMinidump() failed.");
  }
  return res;
}

void CrashHandler::Init(const QString& reportPath) {
  d->InitCrashHandler(reportPath);
}
}