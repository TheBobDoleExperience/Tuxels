#include "ui/PropertiesDock.h"

#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/PropertiesPaneCurves.h"
#include "ui/PropertiesPaneLevels.h"

namespace tuxels {

PropertiesDock::PropertiesDock(QWidget* parent)
    : QDockWidget(tr("Properties"), parent) {
  setObjectName("PropertiesDock");
  setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

  stack_ = new QStackedWidget(this);

  // Page 0 — empty state.
  emptyPage_ = new QWidget(stack_);
  auto* emptyLayout = new QVBoxLayout(emptyPage_);
  emptyLayout->setContentsMargins(16, 16, 16, 16);
  auto* emptyLabel = new QLabel(
      tr("Select an adjustment layer to edit its properties."), emptyPage_);
  emptyLabel->setWordWrap(true);
  emptyLabel->setStyleSheet("color: rgba(180, 180, 180, 200);");
  emptyLayout->addWidget(emptyLabel);
  emptyLayout->addStretch(1);
  stack_->addWidget(emptyPage_);

  // Page 1 — Levels pane.
  levelsPane_ = new PropertiesPaneLevels(stack_);
  connect(levelsPane_, &PropertiesPaneLevels::previewChanged, this,
          &PropertiesDock::previewChanged);
  connect(levelsPane_, &PropertiesPaneLevels::commitRequested, this,
          &PropertiesDock::levelsCommitRequested);
  stack_->addWidget(levelsPane_);

  // Page 2 — Curves pane (M4-S3).
  curvesPane_ = new PropertiesPaneCurves(stack_);
  connect(curvesPane_, &PropertiesPaneCurves::previewChanged, this,
          &PropertiesDock::previewChanged);
  connect(curvesPane_, &PropertiesPaneCurves::commitRequested, this,
          &PropertiesDock::curvesCommitRequested);
  stack_->addWidget(curvesPane_);

  // Future panes (S4 H-S / S4 B&C) get added here.

  setWidget(stack_);
  bindNothing();
}

void PropertiesDock::bindLevels(LevelsAdjustment* layer, Histogram4x256 hist) {
  levelsPane_->bind(layer, hist);
  stack_->setCurrentWidget(levelsPane_);
}

void PropertiesDock::bindCurves(CurvesAdjustment* layer, Histogram4x256 hist) {
  curvesPane_->bind(layer, hist);
  stack_->setCurrentWidget(curvesPane_);
}

void PropertiesDock::bindNothing() {
  levelsPane_->unbind();
  curvesPane_->unbind();
  stack_->setCurrentWidget(emptyPage_);
}

}  // namespace tuxels
