#pragma once

#include <QSplitter>

#include "macros.h"

class Splitter : public QSplitter {
  Q_OBJECT
  DISABLE_COPY(Splitter)

 public:
  Splitter(Qt::Orientation orientation, QWidget* parent = nullptr);
  ~Splitter() = default;
  DEFAULT_MOVE(Splitter)
};

class HSplitter : public Splitter {
  Q_OBJECT

 public:
  HSplitter(QWidget* parent) : Splitter(Qt::Orientation::Horizontal, parent) {}
  ~HSplitter() = default;
};
