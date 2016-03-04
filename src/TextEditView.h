﻿#pragma once

#include <functional>
#include <memory>
#include <boost/optional.hpp>
#include <QObject>
#include <QPlainTextEdit>
#include <QHash>
#include <QMutex>
#include <QStringListModel>
#include <QCompleter>

#include "core/macros.h"
#include "core/ICloneable.h"
#include "core/Document.h"
#include "core/BOM.h"

class QPaintEvent;
class QResizeEvent;
class QSize;
class QWidget;
class QElapsedTimer;
class LineNumberArea;
class TextEditViewPrivate;
namespace core {
struct Language;
class Encoding;
class Theme;
class BOM;
}

class TextEditView : public QPlainTextEdit, public core::ICloneable<TextEditView> {
  Q_OBJECT
  Q_PROPERTY(QString text READ toText WRITE setText USER true)
 public:
  explicit TextEditView(QWidget* parent);
  virtual ~TextEditView();

  void setDocument(std::shared_ptr<core::Document> document);
  core::Language* language();
  void setLanguage(const QString& scopeName);
  boost::optional<core::Encoding> encoding();
  boost::optional<QString> lineSeparator();
  void setLineSeparator(const QString& lineSeparator);
  boost::optional<core::BOM> BOM();
  void setBOM(const core::BOM& bom);

  void lineNumberAreaPaintEvent(QPaintEvent* event);
  int lineNumberAreaWidth();
  TextEditView* clone() override;
  void setPath(const QString& path);
  boost::optional<core::Region> find(const QString& text,
                                     int from,
                                     int begin = 0,
                                     int end = -1,
                                     core::Document::FindFlags flags = 0);
  void highlightSearchMatches(const QString& text,
                              int begin,
                              int end,
                              core::Document::FindFlags flags = 0);
  void clearSearchHighlight();
  void replaceSelection(const QString& text, bool preserveCase = false);
  void replaceAllSelection(const QString& findText,
                           const QString& replaceText,
                           int begin,
                           int end,
                           core::Document::FindFlags flags = 0,
                           bool preserveCase = false);
  void clearSelection();
  void save(bool beforeClose);

 public slots:
  QString scopeName();
  QString scopeTree();
  void undo();
  void redo();
  void cut();
  void copy();
  void paste();
  void selectAll();
  void indent();
  void outdent();
  void insertNewLine();
  void save();
  void saveAs();
  void deleteChar(int repeat = 1);
  bool isThinCursor();
  void setThinCursor(bool on);
  QString path();
  void setTextCursor(const QTextCursor& cursor);
  QTextCursor textCursor() const;
  core::Document* document();
  QRect cursorRect() const;
  QRect cursorRect(const QTextCursor& cursor) const;

 signals:
  void pathUpdated(const QString& path);
  void saved();
  void languageChanged(const QString& scope);
  // emitted when underlying document's encoding is changed.
  void encodingChanged(const core::Encoding& encoding);
  void lineSeparatorChanged(const QString& separator);
  void bomChanged(const core::BOM& bom);

  // private signals
  void destroying(const QString& path, QPrivateSignal);

 protected:
  void resizeEvent(QResizeEvent* event) override;
  void paintEvent(QPaintEvent* e) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void mousePressEvent(QMouseEvent* e) override;
  void setFontPointSize(int sz);
  void makeFontBigger(bool bigger);

 private:
  friend class TextEditViewPrivate;

  std::unique_ptr<TextEditViewPrivate> d_ptr;

  QString toText();
  void setText(const QString& text);
  void setViewportMargins(int left, int top, int right, int bottom);
  void setTheme(core::Theme* theme);

  inline TextEditViewPrivate* d_func() { return d_ptr.get(); }
  inline const TextEditViewPrivate* d_func() const { return d_ptr.get(); }

  Q_PRIVATE_SLOT(d_func(), void outdentCurrentLineIfNecessary())
  Q_PRIVATE_SLOT(d_func(), void updateLineNumberAreaWidth(int newBlockCount))
  Q_PRIVATE_SLOT(d_func(), void updateLineNumberArea(const QRect&, int))
  Q_PRIVATE_SLOT(d_func(), void highlightCurrentLine())
  Q_PRIVATE_SLOT(d_func(), void clearDirtyMarker())
  Q_PRIVATE_SLOT(d_func(), void toggleHighlightingCurrentLine(bool hasSelection))
  Q_PRIVATE_SLOT(d_func(), void setTabStopWidthFromSession())
  Q_PRIVATE_SLOT(d_func(), void setWordWrap(bool))
};

Q_DECLARE_METATYPE(TextEditView*)
