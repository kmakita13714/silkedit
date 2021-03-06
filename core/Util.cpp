#include <cmath>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QMetaMethod>

#include "Util.h"
#include "Wrapper.h"
#include "JSValue.h"

#ifdef Q_OS_WIN
#include <io.h>
#include <Windows.h>
#endif

namespace {

void replace(QString& str, const QString& regex, const QString& after) {
  QRegularExpression re(regex, QRegularExpression::CaseInsensitiveOption);
  QRegularExpressionMatchIterator iter = re.globalMatch(str);
  while (iter.hasNext()) {
    QRegularExpressionMatch match = iter.next();
    str = str.replace(match.capturedStart(), match.capturedLength(), after);
  }
}

bool qObjectPointerTypeCheck(QVariant var, const QByteArray& typeName) {
  if (var.isNull())
    return true;

  // skip "const "
  auto removeFirstCount = typeName.startsWith("const ") ? 6 : 0;
  auto classname = typeName.mid(removeFirstCount, typeName.size() - removeFirstCount - 1);

  return var.canConvert<QObject*>() && var.value<QObject*>()->inherits(classname);
}

bool enumTypeCheck(QVariant var, const QByteArray& typeName) {
  if (!var.canConvert<int>()) {
    return false;
  }

  int typeId = QMetaType::type(typeName);
  if (typeId != QMetaType::UnknownType) {
    const QMetaObject* metaObj = QMetaType(typeId).metaObject();
    const QByteArray& enumName = core::Util::stripNamespace(typeName);
    return metaObj->indexOfEnumerator(enumName) >= 0;
  }

  return false;
}
}

namespace core {

// This function is same as sort.Search in golang
// Search uses binary search to find and return the smallest index i in [0, n) at which f(i) is
// true, assuming that on the range [0, n), f(i) == true implies f(i+1) == true. That is, Search
// requires that f is false for some (possibly empty) prefix of the input range [0, n) and then true
// for the (possibly empty) remainder; Search returns the first true index. If there is no such
// index, Search returns n. (Note that the "not found" return value is not -1 as in, for instance,
// strings.Index). Search calls f(i) only for i in the range [0, n).
int Util::binarySearch(int last, std::function<bool(int)> fn) {
  int low = 0;
  int high = last;
  while (low < high) {
    int mid = (low + high) / 2;
    //    qDebug("low: %d, high: %d, mid: %d", low, high, mid);
    if (fn(mid)) {
      if (mid == 0) {
        break;
      }
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }
  return low;
}

void Util::ensureDir(const QString& path) {
  QDir::root().mkpath(QFileInfo(path).dir().path());
}

bool Util::copy(const QString& source, const QString& dist) {
  ensureDir(dist);
  return QFile(source).copy(dist);
}

std::list<std::string> Util::toStdStringList(const QStringList& qStrList) {
  std::list<std::string> list;
  foreach (QString str, qStrList) {
    std::string path = str.toUtf8().constData();
    list.push_back(path);
  }

  return list;
}

QKeySequence Util::toSequence(const QString& aStr) {
  QString str = aStr;
#ifdef Q_OS_MAC
  replace(str, R"(ctrl|control)", "meta");
  replace(str, R"(cmd|command)", "ctrl");
  replace(str, R"(opt|option)", "alt");
#endif

  replace(str, "enter", "return");
  // When pressing shfit+tab, Qt recognizes it as Shift + Qt::Key_Backtab
  replace(str, R"(shift\s*\+\s*tab)", "shift+backtab");

  return QKeySequence(str);
}

QString Util::toString(const QKeySequence& keySeq) {
  QString str = keySeq.toString().toLower();

#ifdef Q_OS_MAC
  replace(str, "ctrl", "cmd");
  replace(str, "meta", "ctrl");
  replace(str, "alt", "opt");
#endif

  replace(str, "return", "enter");
  return str;
}

void Util::processWithPublicMethods(const QMetaObject* metaObj,
                                    std::function<void(const QMetaMethod&)> fn) {
  QSet<QByteArray> registeredMethods;
  for (int i = 0; i < metaObj->methodCount(); i++) {
    const QMetaMethod& method = metaObj->method(i);
    // MethodType::Method means Q_INVOKABLE method
    if (method.access() == QMetaMethod::Access::Public &&
        (method.methodType() == QMetaMethod::MethodType::Method ||
         method.methodType() == QMetaMethod::MethodType::Slot) &&
        !registeredMethods.contains(method.name())) {
      registeredMethods.insert(method.name());
      fn(method);
    }
  }
}

QByteArray Util::stripNamespace(const QByteArray& name) {
  return name.mid(name.lastIndexOf(":") + 1);
}

bool Util::matchTypes(QList<QByteArray> types, QVariantList args) {
  if (types.size() != args.size()) {
    return false;
  }

  for (int i = 0; i < types.size(); i++) {
    if (QMetaType::type(types[i]) != QMetaType::type(args[i].typeName()) &&
        !qObjectPointerTypeCheck(args[i], types[i]) && !wrappedTypeCheck(args[i], types[i]) &&
        !enumTypeCheck(args[i], types[i])) {
      return false;
    }
  }

  return true;
}

bool Util::convertArgs(ParameterTypes parameterTypes, QVariantList& args) {
  for (int j = 0; j < args.size(); j++) {
    int getWrappedIndex = -1;
    int typeId = QMetaType::type(args[j].typeName());
    if (args[j].canConvert<QObject*>()) {
      QByteArray pointerType = args[j].value<QObject*>()->metaObject()->className();
      pointerType.append('*');
      typeId = QMetaType::type(pointerType);
      if (typeId != QMetaType::UnknownType) {
        const QMetaObject* metaObj = QMetaType::metaObjectForType(typeId);
        if (metaObj) {
          getWrappedIndex = metaObj->indexOfMethod(WRAPPED_METHOD_SIGNATURE);
        }
      }
    }

    if (getWrappedIndex != -1) {
      // get wrapped type from wrapper class
      // e.g. get QUrl from Url
      const QMetaObject* metaObj = QMetaType::metaObjectForType(typeId);
      if (!metaObj) {
        qWarning() << "failed to get metaObject for" << typeId;
        return false;
      }
      const QMetaMethod& method = metaObj->method(getWrappedIndex);
      QVariant returnValue = QVariant(method.returnType(), nullptr);
      QGenericReturnArgument returnArg =
          QGenericReturnArgument(method.typeName(), returnValue.data());
      bool result = method.invoke(args[j].value<QObject*>(), returnArg);
      if (!result) {
        qWarning() << "faled to invoke" << method.name();
        return false;
      }
      QVariant wrappedVar = returnValue.value<QVariant>();
      args[j] = QVariant(QMetaType::type(wrappedVar.typeName()), wrappedVar.data());
    } else {
      // e.g. convert QLabel* to QWidget*
      args[j] = QVariant(QMetaType::type(parameterTypes[j]), args[j].data());
    }
  }

  return true;
}

// argv is contiguous and has nullptr as last element
char** core::Util::toArgv(const QStringList& argsStrings) {
  int totalSize = 0;
  for (const auto& arg : argsStrings) {
    totalSize += arg.toUtf8().size() + 1;
  }

  char** argv = (char**)malloc(sizeof(char*) * (argsStrings.size() + 1));  // +1 for last nullptr
  if (argv == nullptr) {
    qWarning() << "failed to allocate memory for argument";
    exit(1);
  }

  int i = 0;

  // Don't call malloc(0)
  // https://www.securecoding.cert.org/confluence/display/c/MEM04-C.+Beware+of+zero-length+allocations
  if (totalSize > 0) {
// clang-check thinks that s is never freed, but argv points to the memory to which s also points.
// Ignore the following warning because this is false positive.
// clang-check says "Potential leak of memory pointed to by 's'"
#ifndef __clang_analyzer__
    char* s = (char*)malloc(sizeof(char) * totalSize);
    for (i = 0; i < argsStrings.size(); i++) {
      int strSize = argsStrings[i].size() + 1;  // +1 for 0 terminated string
      // convert to UTF-8
      memcpy(s, argsStrings[i].toUtf8().data(), strSize);
      argv[i] = s;
      s += strSize;
    }
#endif
  }

  // the last element of argv is nullptr
  argv[i] = nullptr;

  return argv;
}

bool Util::wrappedTypeCheck(QVariant var, const QByteArray& typeName) {
  // canConvert doesn't work for QObject constructed by QMetaObject::newInstance
  //  if (var.canConvert<QObject*>()) {
  if (static_cast<QMetaType::Type>(var.type()) != QMetaType::QObjectStar &&
      !var.canConvert<QObject*>()) {
    return false;
  }

  auto wrapperQObject = var.value<QObject*>();
  Q_ASSERT(wrapperQObject);
  QByteArray pointerType = wrapperQObject->metaObject()->className();
  pointerType.append('*');
  int typeId = QMetaType::type(pointerType);

  const QMetaObject* metaObj = QMetaType::metaObjectForType(typeId);
  if (!metaObj) {
    return false;
  }

  int infoIndex = metaObj->indexOfClassInfo(WRAPPED);
  if (infoIndex == -1) {
    return false;
  }

  if (metaObj->classInfo(infoIndex).value() == typeName) {
    return true;
  } else {
    // Check INHERITS class info
    int inheritsIndex = metaObj->indexOfClassInfo(INHERITS);
    if (inheritsIndex >= 0 &&
        metaObj->classInfo(inheritsIndex).value() == typeName.left(typeName.size() - 1)) {
      return true;
    }

    // check wrapped QObject* inheritance
    Q_ASSERT(wrapperQObject->inherits(Wrapper::staticMetaObject.className()));
    auto wrapper = qobject_cast<Wrapper*>(wrapperQObject);
    Q_ASSERT(wrapper);
    QVariant wrapped = wrapper->getWrapped();
    if (wrapped.canConvert<QObject*>()) {
      auto qobj = wrapped.value<QObject*>();
      Q_ASSERT(qobj);
      return qobj->inherits(typeName.left(typeName.size() - 1));
    }
  }

  return false;
}

QVariant core::Util::toVariant(const char* str) {
  return toVariant(QString::fromUtf8(str));
}

QVariant core::Util::toVariant(const std::string& str) {
  return toVariant(QString::fromUtf8(str.c_str()));
}

// http://www.yaml.org/spec/1.2/spec.html#id2805071
QVariant core::Util::toVariant(const QString& str) {
  static QRegularExpression nullPattern("null|Null|NULL|~");
  static QRegularExpression boolPattern("true|True|TRUE|false|False|FALSE");
  static QRegularExpression base10IntPattern("[-+]?[0-9]+");
  static QRegularExpression base8IntPattern("0o[0-7]+");
  static QRegularExpression base16IntPattern("0x[0-9a-fA-F]+");
  static QRegularExpression floatPattern(R"([-+]?(\.[0-9]+|[0-9]+(\.[0-9]*)?)([eE][-+]?[0-9]+)?)");
  static QRegularExpression infinityPattern(R"([-+]?(\.inf|\.Inf|\.INF))");
  static QRegularExpression nanPattern(R"(\.nan|\.NaN|\.NAN)");

  QRegularExpressionMatch* match = new QRegularExpressionMatch();
  if (str.contains(nullPattern, match) && match->capturedLength(0) == str.size()) {
    return QVariant::fromValue(JSNull());
  } else if (str.contains(boolPattern, match) && match->capturedLength(0) == str.size()) {
    if (str.toLower() == "true") {
      return QVariant::fromValue(true);
    } else {
      return QVariant::fromValue(false);
    }
  } else if (str.contains(base10IntPattern, match) && match->capturedLength(0) == str.size()) {
    bool ok;
    int value = str.toInt(&ok, 10);
    if (ok) {
      return QVariant::fromValue(value);
    } else {
      qCritical() << "failed to convert" << str << "to int (base 10)";
      return QVariant();
    }
  } else if (str.contains(base8IntPattern, match) && match->capturedLength(0) == str.size()) {
    bool ok;
    int value = str.mid(2).toInt(&ok, 8);
    if (ok) {
      return QVariant::fromValue(value);
    } else {
      qCritical() << "failed to convert" << str << "to int (base 8)";
      return QVariant();
    }
  } else if (str.contains(base16IntPattern, match) && match->capturedLength(0) == str.size()) {
    bool ok;
    int value = str.toInt(&ok, 16);
    if (ok) {
      return QVariant::fromValue(value);
    } else {
      qCritical() << "failed to convert" << str << "to int (base 16)";
      return QVariant();
    }
  } else if (str.contains(floatPattern, match) && match->capturedLength(0) == str.size()) {
    bool ok;
    double value = str.toDouble(&ok);
    if (ok) {
      return QVariant::fromValue(value);
    } else {
      qCritical() << "failed to convert" << str << "to double";
      return QVariant();
    }
  } else if (str.contains(infinityPattern, match) && match->capturedLength(0) == str.size()) {
    return str.startsWith('-') ? QVariant::fromValue(-1 * INFINITY) : QVariant::fromValue(INFINITY);
  } else if (str.contains(nanPattern, match) && match->capturedLength(0) == str.size()) {
    return QVariant::fromValue(NAN);
  } else {
    return QVariant::fromValue(str);
  }
}

#ifdef Q_OS_WIN
// copied from chromium launch_win.cc and added stdin case
void Util::RouteStdioToConsole(bool create_console_if_not_found) {
  // Don't change anything if stdout or stderr already point to a
  // valid stream.
  //
  // If we are running under Buildbot or under Cygwin's default
  // terminal (mintty), stderr and stderr will be pipe handles.  In
  // that case, we don't want to open CONOUT$, because its output
  // likely does not go anywhere.
  //
  // We don't use GetStdHandle() to check stdout/stderr here because
  // it can return dangling IDs of handles that were never inherited
  // by this process.  These IDs could have been reused by the time
  // this function is called.  The CRT checks the validity of
  // stdout/stderr on startup (before the handle IDs can be reused).
  // _fileno(stdout) will return -2 (_NO_CONSOLE_FILENO) if stdout was
  // invalid.
  if (_fileno(stdout) >= 0 || _fileno(stderr) >= 0) {
    // _fileno was broken for SUBSYSTEM:WINDOWS from VS2010 to VS2012/2013.
    // http://crbug.com/358267. Confirm that the underlying HANDLE is valid
    // before aborting.
    // This causes NaCl tests to hang on XP for reasons unclear, perhaps due
    // to not being able to inherit handles. Since it's only for debugging,
    // and redirecting still works, punt for now.
    intptr_t stdout_handle = _get_osfhandle(_fileno(stdout));
    intptr_t stderr_handle = _get_osfhandle(_fileno(stderr));
    if (stdout_handle >= 0 || stderr_handle >= 0)
      return;
  }
  if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
    unsigned int result = GetLastError();
    // Was probably already attached.
    if (result == ERROR_ACCESS_DENIED)
      return;
    // Don't bother creating a new console for each child process if the
    // parent process is invalid (eg: crashed).
    if (result == ERROR_GEN_FAILURE)
      return;
    if (create_console_if_not_found) {
      // Make a new console if attaching to parent fails with any other error.
      // It should be ERROR_INVALID_HANDLE at this point, which means the
      // browser was likely not started from a console.
      AllocConsole();
    } else {
      return;
    }
  }
  // NOTE: --version outputs nothing with setting setvbuf, so we commented it out
  // Arbitrary byte count to use when buffering output lines.  More
  // means potential waste, less means more risk of interleaved
  // log-lines in output.
  //  enum { kOutputBufferSize = 64 * 1024 };
  if (freopen("CONOUT$", "w", stdout)) {
    //    setvbuf(stdout, nullptr, _IOLBF, kOutputBufferSize);
    // Overwrite FD 1 for the benefit of any code that uses this FD
    // directly.  This is safe because the CRT allocates FDs 0, 1 and
    // 2 at startup even if they don't have valid underlying Windows
    // handles.  This means we won't be overwriting an FD created by
    // _open() after startup.
    _dup2(_fileno(stdout), 1);
  }
  if (freopen("CONOUT$", "w", stderr)) {
    //    setvbuf(stderr, nullptr, _IOLBF, kOutputBufferSize);
    _dup2(_fileno(stderr), 2);
  }

  // NOTE: we added stdin case
  if (freopen("CONIN$", "r", stdin)) {
    //    setvbuf(stdin, nullptr, _IOLBF, kOutputBufferSize);
    _dup2(_fileno(stdin), 0);
  }
  // Fix all cout, wcout, cin, wcin, cerr, wcerr, clog and wclog.
  std::ios::sync_with_stdio();
}
#endif

QString Util::qcolorForStyleSheet(const QColor& color) {
  QString rgbaString;
  int alpha = static_cast<int>(color.alphaF() / 1.0 * 100);
  if (alpha == 0) {
    rgbaString = QString("rgb(%1, %2, %3)").arg(color.red()).arg(color.green()).arg(color.blue());
  } else {
    rgbaString = QString("rgba(%1, %2, %3, %4%)")
                     .arg(color.red())
                     .arg(color.green())
                     .arg(color.blue())
                     .arg(alpha);
  }
  return rgbaString;
}

QString Util::readResource(const QString& resource) {
  QFile file(resource);
  auto result = file.open(QFile::ReadOnly | QIODevice::Text);
  Q_ASSERT(result);
  return QLatin1String(file.readAll());
}

}  // namespace core
