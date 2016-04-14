﻿#include <unicode/brkiter.h>
#include <unicode/uchriter.h>
#include <QTextBlock>
#include <QDebug>

#include "TextCursor.h"
#include "IcuUtil.h"
#include "scoped_guard.h"
#include "Config.h"

namespace core {

// Move a cursor considering ICU word boundary.
// Original movePosition skips Japanese text entirely...
// http://userguide.icu-project.org/boundaryanalysis
bool TextCursor::customMovePosition(QTextCursor& cursor,
                                    QTextCursor::MoveOperation op,
                                    QTextCursor::MoveMode mode,
                                    int n) {
  switch (op) {
    case MoveOperation::NextWord: {
      QTextCursor newCursor(cursor);
      newCursor.clearSelection();
      newCursor.movePosition(op, QTextCursor::MoveMode::KeepAnchor, n);
      const auto& text = newCursor.selectedText();
      UErrorCode status = U_ZERO_ERROR;
      auto boundary = BreakIterator::createWordInstance(
          IcuUtil::icuLocale(Config::singleton().locale()), status);
      scoped_guard guard([=] { delete boundary; });

      boundary->setText(IcuUtil::toIcuString(text));
      boundary->first();
      int32_t pos = 0;
      for (int i = 0; i < n; i++) {
        pos = boundary->next();
        if (pos == BreakIterator::DONE) {
          pos = 0;
          break;
        }
      }
      cursor.setPosition(cursor.position() + pos, mode);
      return true;
    }
    case MoveOperation::PreviousWord: {
      QTextCursor newCursor(cursor);
      newCursor.clearSelection();
      newCursor.movePosition(op, QTextCursor::MoveMode::KeepAnchor, n);
      const auto& text = newCursor.selectedText();
      UErrorCode status = U_ZERO_ERROR;
      auto boundary = BreakIterator::createWordInstance(
          IcuUtil::icuLocale(Config::singleton().locale()), status);
      scoped_guard guard([=] { delete boundary; });

      boundary->setText(IcuUtil::toIcuString(text));
      boundary->last();
      int32_t pos = 0;
      for (int i = 0; i < n; i++) {
        pos = boundary->previous();
        if (pos == BreakIterator::DONE) {
          pos = 0;
          break;
        }
      }
      cursor.setPosition(newCursor.position() + pos, mode);
      return true;
    }
    default:
      return cursor.movePosition(op, mode, n);
  }
}

QTextBlock TextCursor::block() const {
  return m_wrapped.value<QTextCursor>().block();
}

bool TextCursor::movePosition(TextCursor::MoveOperation operation,
                              TextCursor::MoveMode mode,
                              int n) {
  // QVariant::value<QTextCursor>() returns a copy of m_wrapped, so we need to reassign it after
  // movePosition
  auto cursor = m_wrapped.value<QTextCursor>();
  auto result = customMovePosition(cursor, static_cast<QTextCursor::MoveOperation>(operation),
                                   static_cast<QTextCursor::MoveMode>(mode), n);
  m_wrapped = QVariant::fromValue(cursor);
  return result;
}

int TextCursor::position() const {
  return m_wrapped.value<QTextCursor>().position();
}

void TextCursor::setPosition(int pos, TextCursor::MoveMode m) {
  auto cursor = m_wrapped.value<QTextCursor>();
  cursor.setPosition(pos, static_cast<QTextCursor::MoveMode>(m));
  m_wrapped = QVariant::fromValue(cursor);
}

QString TextCursor::selectedText() const {
  return m_wrapped.value<QTextCursor>().selectedText();
}

void TextCursor::insertText(const QString& text) {
  auto cursor = m_wrapped.value<QTextCursor>();
  cursor.insertText(text);
  m_wrapped = QVariant::fromValue(cursor);
}

void TextCursor::removeSelectedText() {
  auto cursor = m_wrapped.value<QTextCursor>();
  cursor.removeSelectedText();
  m_wrapped = QVariant::fromValue(cursor);
}

void TextCursor::clearSelection() {
  auto cursor = m_wrapped.value<QTextCursor>();
  cursor.clearSelection();
  m_wrapped = QVariant::fromValue(cursor);
}

}  // namespace core
