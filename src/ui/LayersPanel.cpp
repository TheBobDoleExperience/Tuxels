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
#include "layers/GroupLayer.h"
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

namespace {

// Build a top-down display-order list of layers, honoring per-group
// expansion. Children of an expanded group come immediately after the
// group's own row (visually nested below); collapsed groups skip their
// children entirely. Each entry carries the depth so the row can indent
// itself.
//
// PS reads top-of-stack first; the model is bottom-to-top. We walk children
// in *reverse* so the "topmost" child appears just below its group header
// in the UI, matching PS.
struct DisplayEntry {
  LayerBase* layer;
  int depth;
};

void buildDisplayList(const std::vector<std::unique_ptr<LayerBase>>& children,
                      int depth, std::vector<DisplayEntry>& out) {
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    LayerBase* l = it->get();
    if (!l) continue;
    out.push_back({l, depth});
    if (l->kind() == LayerKind::Group) {
      const auto* g = static_cast<const GroupLayer*>(l);
      if (g->isExpanded) {
        buildDisplayList(g->children, depth + 1, out);
      }
    }
  }
}

}  // namespace

void LayersPanel::refresh() {
  refreshing_ = true;
  list_->clear();
  rows_.clear();
  if (!doc_) {
    refreshing_ = false;
    return;
  }

  // Walk the tree top-down (PS UI reading order); collect the layers that
  // are visible in the panel given current group-expansion state.
  std::vector<DisplayEntry> entries;
  buildDisplayList(doc_->tree().raw(), /*depth=*/0, entries);

  for (const auto& e : entries) {
    auto* row = new LayerRowWidget();
    row->bindToLayer(e.layer);
    row->setIndentDepth(e.depth);
    connect(row, &LayerRowWidget::layerMutated, this,
            &LayersPanel::onLayerRowMutated);
    connect(row, &LayerRowWidget::visibilityChangeRequested, this,
            &LayersPanel::visibilityChangeRequested);
    connect(row, &LayerRowWidget::blendChangeRequested, this,
            &LayersPanel::blendChangeRequested);
    connect(row, &LayerRowWidget::opacityEditCommitted, this,
            &LayersPanel::opacityEditCommitted);
    connect(row, &LayerRowWidget::paintTargetChangeRequested, this,
            &LayersPanel::paintTargetChangeRequested);
    connect(row, &LayerRowWidget::maskEnabledToggleRequested, this,
            &LayersPanel::maskEnabledToggleRequested);
    connect(row, &LayerRowWidget::deleteMaskRequested, this,
            &LayersPanel::deleteMaskRequested);
    connect(row, &LayerRowWidget::editAdjustmentRequested, this,
            &LayersPanel::editAdjustmentRequested);
    connect(row, &LayerRowWidget::toggleClipToBelowRequested, this,
            &LayersPanel::toggleClipToBelowRequested);
    connect(row, &LayerRowWidget::chevronToggled, this,
            &LayersPanel::onGroupChevronToggled);

    auto* item = new QListWidgetItem();
    item->setSizeHint(row->sizeHint());
    list_->addItem(item);
    list_->setItemWidget(item, row);
    rows_.push_back(row);
  }

  // Reflect active-layer selection by id (M5).
  const LayerId activeId = doc_->activeLayerId();
  if (activeId != 0) {
    for (int i = 0; i < static_cast<int>(rows_.size()); ++i) {
      if (rows_[static_cast<std::size_t>(i)]->layer() &&
          rows_[static_cast<std::size_t>(i)]->layer()->id == activeId) {
        list_->setCurrentRow(i);
        break;
      }
    }
  }
  for (int i = 0; i < static_cast<int>(rows_.size()); ++i) {
    rows_[static_cast<std::size_t>(i)]->setActive(i == list_->currentRow());
    rows_[static_cast<std::size_t>(i)]->setPaintTarget(doc_->paintTarget());
  }

  refreshing_ = false;
}

void LayersPanel::onGroupChevronToggled(GroupLayer* group) {
  if (!group) return;
  group->isExpanded = !group->isExpanded;
  refresh();
}

void LayersPanel::onCurrentRowChanged(int row) {
  if (refreshing_ || !doc_) return;
  if (row < 0 || row >= static_cast<int>(rows_.size())) {
    doc_->setActiveLayerId(0);
    emit activeLayerChanged();
    return;
  }
  LayerBase* layer = rows_[static_cast<std::size_t>(row)]->layer();
  doc_->setActiveLayerId(layer ? layer->id : 0);
  for (int i = 0; i < static_cast<int>(rows_.size()); ++i) {
    rows_[static_cast<std::size_t>(i)]->setActive(i == row);
  }
  emit activeLayerChanged();
}

void LayersPanel::onLayerRowMutated(LayerBase* /*layer*/) {
  emit layerMutated();
}

int LayersPanel::rowCountForTesting() const {
  return static_cast<int>(rows_.size());
}

LayerRowWidget* LayersPanel::rowAtForTesting(int index) const {
  if (index < 0 || index >= static_cast<int>(rows_.size())) return nullptr;
  return rows_[static_cast<std::size_t>(index)];
}

}  // namespace tuxels
