#include "ui/LayersPanel.h"

#include <QAction>
#include <QIcon>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStyle>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include "core/Document.h"
#include "layers/LayerBase.h"
#include "ui/LayerRowWidget.h"

namespace tuxels {

LayersPanel::LayersPanel(QWidget* parent)
    : QDockWidget(tr("Layers"), parent) {
  setObjectName("LayersPanel");

  auto* container = new QWidget(this);
  auto* vbox = new QVBoxLayout(container);
  vbox->setContentsMargins(0, 0, 0, 0);
  vbox->setSpacing(0);

  toolbar_ = new QToolBar(container);
  toolbar_->setIconSize(QSize(16, 16));
  auto* addAct = toolbar_->addAction(
      style()->standardIcon(QStyle::SP_FileDialogNewFolder), tr("Add layer"));
  auto* delAct = toolbar_->addAction(
      style()->standardIcon(QStyle::SP_TrashIcon), tr("Delete layer"));
  auto* upAct = toolbar_->addAction(
      style()->standardIcon(QStyle::SP_ArrowUp), tr("Move up"));
  auto* downAct = toolbar_->addAction(
      style()->standardIcon(QStyle::SP_ArrowDown), tr("Move down"));

  connect(addAct, &QAction::triggered, this, &LayersPanel::addLayerRequested);
  connect(delAct, &QAction::triggered, this,
          &LayersPanel::deleteActiveLayerRequested);
  connect(upAct, &QAction::triggered, this,
          &LayersPanel::moveActiveLayerUpRequested);
  connect(downAct, &QAction::triggered, this,
          &LayersPanel::moveActiveLayerDownRequested);

  vbox->addWidget(toolbar_);

  list_ = new QListWidget(container);
  list_->setSelectionMode(QAbstractItemView::SingleSelection);
  list_->setUniformItemSizes(false);
  connect(list_, &QListWidget::currentRowChanged, this,
          &LayersPanel::onCurrentRowChanged);
  vbox->addWidget(list_, /*stretch=*/1);

  setWidget(container);
}

void LayersPanel::setDocument(Document* doc) {
  doc_ = doc;
  refresh();
}

void LayersPanel::refresh() {
  refreshing_ = true;
  list_->clear();
  rows_.clear();
  if (!doc_) {
    refreshing_ = false;
    return;
  }

  // Display top-of-stack first so the UI reads top-down as in Photoshop.
  const int n = static_cast<int>(doc_->tree().size());
  for (int uiIdx = 0; uiIdx < n; ++uiIdx) {
    const int modelIdx = n - 1 - uiIdx;
    LayerBase* layer = doc_->tree().at(static_cast<std::size_t>(modelIdx));
    auto* row = new LayerRowWidget();
    row->bindToLayer(layer);
    connect(row, &LayerRowWidget::layerMutated, this,
            &LayersPanel::onLayerRowMutated);

    auto* item = new QListWidgetItem();
    item->setSizeHint(row->sizeHint());
    item->setData(Qt::UserRole, modelIdx);
    list_->addItem(item);
    list_->setItemWidget(item, row);
    rows_.push_back(row);
  }

  // Reflect active-layer selection.
  if (doc_->activeLayerIndex() >= 0) {
    const int uiRow = n - 1 - doc_->activeLayerIndex();
    if (uiRow >= 0 && uiRow < n) list_->setCurrentRow(uiRow);
  }
  for (int i = 0; i < static_cast<int>(rows_.size()); ++i) {
    rows_[static_cast<std::size_t>(i)]->setActive(i == list_->currentRow());
  }

  refreshing_ = false;
}

void LayersPanel::onCurrentRowChanged(int row) {
  if (refreshing_ || !doc_) return;
  if (row < 0) {
    doc_->setActiveLayerIndex(-1);
    emit activeLayerChanged();
    return;
  }
  const int n = static_cast<int>(doc_->tree().size());
  const int modelIdx = n - 1 - row;
  doc_->setActiveLayerIndex(modelIdx);
  for (int i = 0; i < static_cast<int>(rows_.size()); ++i) {
    rows_[static_cast<std::size_t>(i)]->setActive(i == row);
  }
  emit activeLayerChanged();
}

void LayersPanel::onLayerRowMutated(LayerBase* /*layer*/) {
  emit layerMutated();
}

}  // namespace tuxels
