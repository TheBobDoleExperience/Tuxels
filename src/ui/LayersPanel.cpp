#include "ui/LayersPanel.h"

#include <QAction>
#include <QApplication>
#include <QDrag>
#include <QDropEvent>
#include <QIcon>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QStyle>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <cstring>

#include "core/Document.h"
#include "layers/GroupLayer.h"
#include "layers/LayerBase.h"
#include "ui/LayerRowWidget.h"

namespace tuxels {

namespace {

// Custom MIME type carrying the dragged layer's id (uint64). Internal to
// the panel — we never accept drops from outside the LayersPanel itself.
constexpr const char* kLayerIdMime = "application/x-tuxels-layerid";

// QListWidget subclass that:
//  - initiates drags carrying the dragged layer's id (so the row widgets
//    eating mouse events can't break drag-start),
//  - intercepts drops, computes the (target row, drop zone) pair from the
//    cursor, and forwards to LayersPanel::emitLayerDrop.
//
// We *don't* call the base's drop implementation: QListWidget's
// InternalMove would mutate the list-widget items but our true model is
// the document's LayerTree, so we route the move through a LayerOpCommand
// in MainWindow.
class LayerListWidget : public QListWidget {
 public:
  LayerListWidget(LayersPanel* panel, QWidget* parent = nullptr)
      : QListWidget(parent), panel_(panel) {
    // M6-S2: Extended selection enables Shift-click range + Ctrl-click
    // toggle. Single click still replaces the selection with the clicked
    // row (matches PS).
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setUniformItemSizes(false);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
  }

 protected:
  void mousePressEvent(QMouseEvent* event) override {
    QListWidget::mousePressEvent(event);
    if (event->button() == Qt::LeftButton) {
      pressPos_ = event->position().toPoint();
      pressItem_ = itemAt(pressPos_);
    }
  }

  void mouseMoveEvent(QMouseEvent* event) override {
    // Item widgets (the LayerRowWidget) eat mouse events that would
    // normally reach the QListWidget, so QListWidget's built-in drag
    // detection never fires. We initiate the drag manually from a press
    // tracked in mousePressEvent; the press itself is forwarded into the
    // children, but the item-rect hit-test still works for us via
    // `itemAt()` in press because mouseMove arrives on the viewport even
    // when item widgets handle their own presses (Qt re-routes via
    // bubbling for moves with held buttons).
    if ((event->buttons() & Qt::LeftButton) && pressItem_ != nullptr) {
      const int dist = (event->position().toPoint() - pressPos_).manhattanLength();
      if (dist >= QApplication::startDragDistance()) {
        startDragForItem(pressItem_);
        pressItem_ = nullptr;
        return;
      }
    }
    QListWidget::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent* event) override {
    pressItem_ = nullptr;
    QListWidget::mouseReleaseEvent(event);
  }

  void dragEnterEvent(QDragEnterEvent* event) override {
    if (event->mimeData()->hasFormat(kLayerIdMime) && event->source() == this) {
      event->acceptProposedAction();
    } else {
      event->ignore();
    }
  }

  void dragMoveEvent(QDragMoveEvent* event) override {
    if (event->mimeData()->hasFormat(kLayerIdMime) && event->source() == this) {
      event->acceptProposedAction();
      // M7-S2: track the drop zone under the cursor so paintEvent can
      // render our custom indicator (Qt's default doesn't distinguish
      // "drop INTO group" from "drop above/below row").
      QListWidgetItem* prevItem = hoverItem_;
      const DropZone prevZone = hoverZone_;
      hoverItem_ = nullptr;
      const QPoint pos = event->position().toPoint();
      if (auto* item = itemAt(pos)) {
        if (auto* row = qobject_cast<LayerRowWidget*>(itemWidget(item))) {
          (void)row;
          hoverItem_ = item;
          hoverZone_ = zoneAt(item, pos);
        }
      }
      if (hoverItem_ != prevItem || hoverZone_ != prevZone) {
        viewport()->update();
      }
    } else {
      event->ignore();
    }
  }

  void dragLeaveEvent(QDragLeaveEvent* event) override {
    hoverItem_ = nullptr;
    viewport()->update();
    QListWidget::dragLeaveEvent(event);
  }

  void dropEvent(QDropEvent* event) override {
    // Clear hover state regardless of outcome so the indicator vanishes.
    hoverItem_ = nullptr;
    viewport()->update();

    if (!panel_ || event->source() != this ||
        !event->mimeData()->hasFormat(kLayerIdMime)) {
      event->ignore();
      return;
    }
    const QByteArray bytes = event->mimeData()->data(kLayerIdMime);
    if (bytes.size() != static_cast<int>(sizeof(LayerId))) {
      event->ignore();
      return;
    }
    LayerId movedId = 0;
    std::memcpy(&movedId, bytes.constData(), sizeof(LayerId));
    if (movedId == 0) {
      event->ignore();
      return;
    }

    const QPoint pos = event->position().toPoint();
    QListWidgetItem* targetItem = itemAt(pos);
    LayerBase* targetLayer = nullptr;
    DropZone zone = DropZone::Below;  // default for "outside any row"
    if (targetItem) {
      auto* targetRow =
          qobject_cast<LayerRowWidget*>(itemWidget(targetItem));
      if (targetRow) {
        targetLayer = targetRow->layer();
        zone = zoneAt(targetItem, pos);
      }
    }
    panel_->emitLayerDrop(movedId, targetLayer, zone);
    event->acceptProposedAction();
    // Intentionally do NOT call QListWidget::dropEvent — the panel
    // refresh after the LayerOpCommand will rebuild the rows from the
    // tree.
  }

  void paintEvent(QPaintEvent* event) override {
    QListWidget::paintEvent(event);
    // M7-S2: overlay our custom drop indicator on top of the items.
    if (!hoverItem_) return;
    QPainter p(viewport());
    const QRect r = visualItemRect(hoverItem_);
    if (hoverZone_ == DropZone::On) {
      auto* row = qobject_cast<LayerRowWidget*>(itemWidget(hoverItem_));
      const bool targetIsGroup =
          row && row->layer() && row->layer()->kind() == LayerKind::Group;
      if (targetIsGroup) {
        // Drop INTO group: tinted fill + thick border to make the action
        // unambiguous.
        p.fillRect(r, QColor(60, 120, 200, 70));
        QPen pen(QColor(60, 120, 200, 230), 2);
        p.setPen(pen);
        p.drawRect(r.adjusted(1, 1, -1, -1));
      } else {
        // On non-group: same visual as Below (the dispatch will resolve
        // it that way).
        QPen pen(QColor(60, 200, 120, 230), 2);
        p.setPen(pen);
        p.drawLine(r.left(), r.bottom() - 1, r.right(), r.bottom() - 1);
      }
    } else if (hoverZone_ == DropZone::Above) {
      QPen pen(QColor(60, 200, 120, 230), 2);
      p.setPen(pen);
      p.drawLine(r.left(), r.top(), r.right(), r.top());
    } else {  // Below
      QPen pen(QColor(60, 200, 120, 230), 2);
      p.setPen(pen);
      p.drawLine(r.left(), r.bottom() - 1, r.right(), r.bottom() - 1);
    }
  }

 private:
  // Single hit-test source-of-truth for drag-move (hover) and drop.
  DropZone zoneAt(QListWidgetItem* item, const QPoint& pos) const {
    const QRect r = visualItemRect(item);
    const int y = pos.y() - r.top();
    const int h = r.height() > 0 ? r.height() : 1;
    if (y < h / 4) return DropZone::Above;
    if (y > 3 * h / 4) return DropZone::Below;
    return DropZone::On;
  }

  void startDragForItem(QListWidgetItem* item) {
    auto* row = qobject_cast<LayerRowWidget*>(itemWidget(item));
    if (!row || !row->layer()) return;
    const LayerId id = row->layer()->id;
    auto* mime = new QMimeData();
    QByteArray bytes(reinterpret_cast<const char*>(&id), sizeof(LayerId));
    mime->setData(kLayerIdMime, bytes);
    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    // Reasonable default cursor; no custom pixmap for now.
    drag->exec(Qt::MoveAction);
  }

  LayersPanel* panel_;
  QPoint pressPos_;
  QListWidgetItem* pressItem_ = nullptr;
  // Drag-hover state for the M7-S2 custom indicator.
  QListWidgetItem* hoverItem_ = nullptr;
  DropZone hoverZone_ = DropZone::Below;
};

}  // namespace

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

  list_ = new LayerListWidget(this, container);
  connect(list_, &QListWidget::currentRowChanged, this,
          &LayersPanel::onCurrentRowChanged);
  // M6-S2: every selection change (Shift-range / Ctrl-toggle / single
  // click) pushes the row-set into Document::selectedLayerIds. The
  // currentRow signal still drives activeLayerId.
  connect(list_, &QListWidget::itemSelectionChanged, this,
          &LayersPanel::onSelectionChanged);
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
    connect(row, &LayerRowWidget::nameChangeRequested, this,
            &LayersPanel::nameChangeRequested);
    connect(row, &LayerRowWidget::duplicateLayerRequested, this,
            &LayersPanel::duplicateLayerRequested);
    connect(row, &LayerRowWidget::deleteLayerRequested, this,
            &LayersPanel::deleteLayerRequested);
    connect(row, &LayerRowWidget::groupLayerRequested, this,
            &LayersPanel::groupLayerRequested);
    connect(row, &LayerRowWidget::addLayerMaskRequested, this,
            &LayersPanel::addLayerMaskRequested);
    connect(row, &LayerRowWidget::renameLayerRequested, this,
            &LayersPanel::renameLayerRequested);

    auto* item = new QListWidgetItem();
    item->setSizeHint(row->sizeHint());
    // M6-S1: each row participates in drag-and-drop. The drag is initiated
    // by the LayerListWidget's mouse handlers (item widgets eat the
    // events otherwise); these flags let the QListWidget model accept the
    // drop intent.
    item->setFlags(item->flags() | Qt::ItemIsDragEnabled |
                   Qt::ItemIsDropEnabled);
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
  // M6-S2: re-apply the multi-selection set onto the QListWidget so
  // selections survive reorders + group expansion. Active id is already
  // included in the selection by convention; if the set is empty we
  // fall back to the current row's selection.
  const auto& selIds = doc_->selectedLayerIds();
  if (!selIds.empty()) {
    list_->clearSelection();
    for (int i = 0; i < static_cast<int>(rows_.size()); ++i) {
      LayerBase* l = rows_[static_cast<std::size_t>(i)]->layer();
      if (!l) continue;
      for (LayerId sid : selIds) {
        if (l->id == sid) {
          list_->item(i)->setSelected(true);
          break;
        }
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

void LayersPanel::onSelectionChanged() {
  if (refreshing_ || !doc_) return;
  std::vector<LayerId> ids;
  ids.reserve(rows_.size());
  for (int i = 0; i < static_cast<int>(rows_.size()); ++i) {
    auto* item = list_->item(i);
    if (item && item->isSelected()) {
      LayerBase* l = rows_[static_cast<std::size_t>(i)]->layer();
      if (l) ids.push_back(l->id);
    }
  }
  doc_->setSelectedLayerIds(std::move(ids));
}

void LayersPanel::onLayerRowMutated(LayerBase* /*layer*/) {
  emit layerMutated();
}

void LayersPanel::emitLayerDrop(LayerId movedId, LayerBase* target,
                                 DropZone zone) {
  if (!doc_ || movedId == 0) return;

  // Locate the dragged layer in the tree.
  auto srcLoc = doc_->tree().locate(movedId);
  if (!srcLoc) return;
  GroupLayer* fromParent = srcLoc->parent;
  const std::size_t fromIdx = srcLoc->index;
  const LayerId fromParentId = fromParent ? fromParent->id : 0;

  // Compute destination (parent, finalIndex) from the zone + target.
  GroupLayer* toParent = nullptr;
  std::size_t toIdx = 0;

  if (target == nullptr) {
    // Dropped on empty viewport — append at root top (panel top).
    toParent = nullptr;
    toIdx = doc_->tree().size();  // tree.move clamps to size if needed
  } else if (zone == DropZone::On && target->kind() == LayerKind::Group) {
    // Drop INTO group → land at end of children (panel-wise this is the
    // top of the group's nested block, immediately under the group's
    // header row).
    auto* g = static_cast<GroupLayer*>(target);
    toParent = g;
    toIdx = g->children.size();
  } else {
    auto loc = doc_->tree().locate(target->id);
    if (!loc) return;
    toParent = loc->parent;
    const std::size_t Kx = loc->index;
    if (zone == DropZone::Above) {
      toIdx = Kx;  // final tree idx = K_X (panel reverses tree order)
    } else {
      // Below in panel = lower tree idx by 1; clamp at 0.
      toIdx = (Kx == 0) ? 0 : (Kx - 1);
    }
  }

  const LayerId toParentId = toParent ? toParent->id : 0;

  // Cycle check: a group cannot be dropped into itself or any descendant.
  if (toParentId != 0) {
    LayerId cur = toParentId;
    while (cur != 0) {
      if (cur == movedId) return;  // would create a cycle
      auto curLoc = doc_->tree().locate(cur);
      if (!curLoc) break;
      cur = curLoc->parent ? curLoc->parent->id : 0;
    }
  }

  // No-op: same parent + same effective slot.
  if (fromParentId == toParentId && fromIdx == toIdx) return;
  // No-op: same parent + adjacent slot that resolves to the same location
  // after the erase. tree.move's post-erase frame collapses (K, K) and
  // (K, K+1) to no-ops anyway, but bail early to avoid pushing a dead
  // undo entry.
  if (fromParentId == toParentId && fromIdx + 1 == toIdx) return;

  emit layerDroppedRequested(movedId, toParentId, toIdx);
}

void LayersPanel::beginRenameForLayer(LayerId id) {
  for (auto* row : rows_) {
    if (row && row->layer() && row->layer()->id == id) {
      row->beginRename();
      return;
    }
  }
}

int LayersPanel::rowCountForTesting() const {
  return static_cast<int>(rows_.size());
}

LayerRowWidget* LayersPanel::rowAtForTesting(int index) const {
  if (index < 0 || index >= static_cast<int>(rows_.size())) return nullptr;
  return rows_[static_cast<std::size_t>(index)];
}

}  // namespace tuxels
