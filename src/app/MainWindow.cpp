#include "app/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <algorithm>

#include "compositor/compose.h"
#include "core/Document.h"
#include "core/SelectionMask.h"
#include "history/LayerOpCommand.h"
#include "history/PaintCommand.h"
#include "history/SelectionCommand.h"
#include "history/UndoStack.h"
#include "io/PngIO.h"
#include "layers/LayerMask.h"
#include "layers/PixelLayer.h"
#include "tools/BrushTool.h"
#include "tools/MarqueeTool.h"
#include "ui/CanvasView.h"
#include "ui/LayersPanel.h"
#include "ui/ToolsPanel.h"

namespace tuxels {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("Tuxels");
  resize(1400, 900);

  brushTool_ = std::make_unique<BrushTool>();
  marqueeTool_ = std::make_unique<MarqueeTool>();
  undoStack_ = std::make_unique<UndoStack>(/*maxDepth=*/64);

  canvas_ = new CanvasView(this);
  canvas_->setTool(brushTool_.get());
  connect(canvas_, &CanvasView::layerPainted, this, &MainWindow::onLayerPainted);
  setCentralWidget(canvas_);

  buildDocks();
  buildMenus();
  statusBar()->showMessage(tr("Ready"));

  setDocument(std::make_unique<Document>(1024, 768));
  populateSampleDocument();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenus() {
  auto* mb = menuBar();

  auto* fileMenu = mb->addMenu(tr("&File"));
  auto* newAct = fileMenu->addAction(tr("&New…"));
  newAct->setShortcut(QKeySequence::New);
  connect(newAct, &QAction::triggered, this, &MainWindow::onFileNew);

  auto* openAct = fileMenu->addAction(tr("&Open…"));
  openAct->setShortcut(QKeySequence::Open);
  connect(openAct, &QAction::triggered, this, &MainWindow::onFileOpen);

  fileMenu->addSeparator();
  auto* exportAct = fileMenu->addAction(tr("Export &As PNG…"));
  exportAct->setShortcut(QKeySequence(tr("Ctrl+Shift+E")));
  connect(exportAct, &QAction::triggered, this, &MainWindow::onFileExport);

  fileMenu->addSeparator();
  auto* quitAct = fileMenu->addAction(tr("&Quit"));
  quitAct->setShortcut(QKeySequence::Quit);
  connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);

  auto* editMenu = mb->addMenu(tr("&Edit"));
  auto* undoAct = editMenu->addAction(tr("&Undo"));
  undoAct->setShortcut(QKeySequence::Undo);
  connect(undoAct, &QAction::triggered, this, &MainWindow::onEditUndo);
  auto* redoAct = editMenu->addAction(tr("&Redo"));
  redoAct->setShortcut(QKeySequence::Redo);
  connect(redoAct, &QAction::triggered, this, &MainWindow::onEditRedo);

  mb->addMenu(tr("&Image"));

  auto* layerMenu = mb->addMenu(tr("&Layer"));
  auto* addLayerAct = layerMenu->addAction(tr("&New Layer"));
  addLayerAct->setShortcut(QKeySequence(tr("Ctrl+Shift+N")));
  connect(addLayerAct, &QAction::triggered, this, &MainWindow::onLayerAdd);

  auto* delLayerAct = layerMenu->addAction(tr("&Delete Layer"));
  connect(delLayerAct, &QAction::triggered, this, &MainWindow::onLayerDelete);

  layerMenu->addSeparator();
  auto* addMaskAct = layerMenu->addAction(tr("Add Layer &Mask"));
  connect(addMaskAct, &QAction::triggered, this, &MainWindow::onAddLayerMask);
  auto* delMaskAct = layerMenu->addAction(tr("D&elete Layer Mask"));
  connect(delMaskAct, &QAction::triggered, this, &MainWindow::onDeleteLayerMask);

  auto* selectMenu = mb->addMenu(tr("&Select"));
  auto* selectAllAct = selectMenu->addAction(tr("&All"));
  selectAllAct->setShortcut(QKeySequence::SelectAll);
  connect(selectAllAct, &QAction::triggered, this, &MainWindow::onSelectAll);
  auto* deselectAct = selectMenu->addAction(tr("&Deselect"));
  deselectAct->setShortcut(QKeySequence(tr("Ctrl+D")));
  connect(deselectAct, &QAction::triggered, this, &MainWindow::onDeselect);
  auto* inverseAct = selectMenu->addAction(tr("&Inverse"));
  inverseAct->setShortcut(QKeySequence(tr("Ctrl+Shift+I")));
  connect(inverseAct, &QAction::triggered, this, &MainWindow::onSelectInverse);

  mb->addMenu(tr("&Filter"));
  mb->addMenu(tr("&View"));
  mb->addMenu(tr("&Window"));
  mb->addMenu(tr("&Help"));

  // Brush-size keyboard shortcuts live on the main window so they work
  // regardless of which panel has focus.
  auto* sizeUp = new QAction(this);
  sizeUp->setShortcut(QKeySequence(tr("]")));
  connect(sizeUp, &QAction::triggered, this, &MainWindow::onBrushSizeIncrease);
  addAction(sizeUp);

  auto* sizeDown = new QAction(this);
  sizeDown->setShortcut(QKeySequence(tr("[")));
  connect(sizeDown, &QAction::triggered, this, &MainWindow::onBrushSizeDecrease);
  addAction(sizeDown);

  auto* swap = new QAction(this);
  swap->setShortcut(QKeySequence(tr("X")));
  connect(swap, &QAction::triggered, this,
          [this]() { if (toolsPanel_) toolsPanel_->swapColors(); });
  addAction(swap);

  auto* reset = new QAction(this);
  reset->setShortcut(QKeySequence(tr("D")));
  connect(reset, &QAction::triggered, this,
          [this]() { if (toolsPanel_) toolsPanel_->resetColors(); });
  addAction(reset);

  // Tool-picker keyboard shortcuts (Photoshop letters).
  auto* pickBrush = new QAction(this);
  pickBrush->setShortcut(QKeySequence(tr("B")));
  connect(pickBrush, &QAction::triggered, this,
          [this]() { setActiveTool(ToolId::Brush); });
  addAction(pickBrush);

  auto* pickMarquee = new QAction(this);
  pickMarquee->setShortcut(QKeySequence(tr("M")));
  connect(pickMarquee, &QAction::triggered, this,
          [this]() { setActiveTool(ToolId::Marquee); });
  addAction(pickMarquee);
}

void MainWindow::buildDocks() {
  toolsPanel_ = new ToolsPanel(this);
  addDockWidget(Qt::LeftDockWidgetArea, toolsPanel_);
  toolsPanel_->setBrushTool(brushTool_.get());
  toolsPanel_->setActiveTool(activeToolId_);
  connect(toolsPanel_, &ToolsPanel::toolPicked, this,
          &MainWindow::onToolPicked);
  connect(toolsPanel_, &ToolsPanel::marqueeModeChanged, this,
          [this](SelectionMode m) {
            if (marqueeTool_) marqueeTool_->setMode(m);
          });
  if (marqueeTool_) toolsPanel_->setMarqueeMode(marqueeTool_->mode());

  layersPanel_ = new LayersPanel(this);
  addDockWidget(Qt::RightDockWidgetArea, layersPanel_);

  connect(layersPanel_, &LayersPanel::addLayerRequested, this,
          &MainWindow::onLayerAdd);
  connect(layersPanel_, &LayersPanel::deleteActiveLayerRequested, this,
          &MainWindow::onLayerDelete);
  connect(layersPanel_, &LayersPanel::moveActiveLayerUpRequested, this,
          &MainWindow::onLayerMoveUp);
  connect(layersPanel_, &LayersPanel::moveActiveLayerDownRequested, this,
          &MainWindow::onLayerMoveDown);
  connect(layersPanel_, &LayersPanel::activeLayerChanged, this,
          &MainWindow::onActiveLayerChanged);
  connect(layersPanel_, &LayersPanel::layerMutated, this,
          &MainWindow::onLayerPanelMutated);
  connect(layersPanel_, &LayersPanel::visibilityChangeRequested, this,
          &MainWindow::onLayerVisibilityChange);
  connect(layersPanel_, &LayersPanel::blendChangeRequested, this,
          &MainWindow::onLayerBlendChange);
  connect(layersPanel_, &LayersPanel::opacityEditCommitted, this,
          &MainWindow::onLayerOpacityCommit);
  connect(layersPanel_, &LayersPanel::paintTargetChangeRequested, this,
          &MainWindow::onLayerPaintTargetChange);
  connect(layersPanel_, &LayersPanel::maskEnabledToggleRequested, this,
          &MainWindow::onLayerMaskEnabledToggle);
  connect(layersPanel_, &LayersPanel::deleteMaskRequested, this,
          &MainWindow::onLayerDeleteMaskRequest);
}

void MainWindow::setDocument(std::unique_ptr<Document> doc) {
  doc_ = std::move(doc);
  if (undoStack_) undoStack_->clear();
  canvas_->setDocument(doc_.get());
  layersPanel_->setDocument(doc_.get());
}

void MainWindow::refreshAfterUndoRedo(Rect dirtyRect) {
  if (!doc_) return;
  if (doc_->activeLayerIndex() >= static_cast<int>(doc_->tree().size())) {
    doc_->setActiveLayerIndex(static_cast<int>(doc_->tree().size()) - 1);
  }
  layersPanel_->refresh();
  if (dirtyRect.isEmpty()) {
    canvas_->requestRecomposite();
  } else {
    canvas_->requestRecomposite(dirtyRect);
  }
  // Selection pointer may have been swapped by an undone/redone
  // SelectionCommand — paintEvent's identity check will rebuild the cache,
  // but kicking it here keeps the overlay in lock-step with the recomposite.
  if (canvas_) canvas_->refreshSelectionOverlay();
}

void MainWindow::populateSampleDocument() {
  if (!doc_) return;
  // Background layer (white).
  auto* bg = doc_->addBlankPixelLayer("Background");
  bg->image.fill(Rgba32F::white());
  // A red rectangle in the middle.
  auto* red = doc_->addBlankPixelLayer("Red Square");
  for (int y = 200; y < 500; ++y) {
    for (int x = 300; x < 700; ++x) {
      red->image.setPixel(x, y, Rgba32F(0.9f, 0.1f, 0.1f, 1.f));
    }
  }
  // A green circle-ish region (simple disc).
  auto* green = doc_->addBlankPixelLayer("Green Disc");
  const int cx = 700, cy = 400, rad = 180;
  for (int y = cy - rad; y <= cy + rad; ++y) {
    for (int x = cx - rad; x <= cx + rad; ++x) {
      const int dx = x - cx, dy = y - cy;
      if (dx * dx + dy * dy <= rad * rad) {
        green->image.setPixel(x, y, Rgba32F(0.1f, 0.8f, 0.2f, 1.f));
      }
    }
  }
  green->blend = BlendMode::Multiply;

  layersPanel_->refresh();
  canvas_->requestRecomposite();
}

void MainWindow::onFileNew() {
  bool ok = false;
  const int w = QInputDialog::getInt(this, tr("New Document"), tr("Width:"),
                                     1024, 1, 30000, 1, &ok);
  if (!ok) return;
  const int h = QInputDialog::getInt(this, tr("New Document"), tr("Height:"),
                                     768, 1, 30000, 1, &ok);
  if (!ok) return;
  setDocument(std::make_unique<Document>(w, h));
  doc_->addBlankPixelLayer("Background");
  layersPanel_->refresh();
  canvas_->requestRecomposite();
  statusBar()->showMessage(tr("New %1×%2 document").arg(w).arg(h), 3000);
}

void MainWindow::onFileOpen() {
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Open Image"), QString(), tr("PNG Images (*.png)"));
  if (path.isEmpty()) return;

  QString err;
  auto img = loadPng(path, &err);
  if (!img) {
    QMessageBox::warning(this, tr("Open Failed"),
                         tr("Could not open %1:\n%2").arg(path, err));
    return;
  }

  auto doc = std::make_unique<Document>(img->width(), img->height());
  auto* layer = doc->addBlankPixelLayer(QFileInfo(path).fileName().toStdString());
  layer->image = std::move(*img);
  setDocument(std::move(doc));
  layersPanel_->refresh();
  canvas_->requestRecomposite();
  statusBar()->showMessage(tr("Opened %1").arg(path), 3000);
}

void MainWindow::onFileExport() {
  if (!doc_ || doc_->width() <= 0 || doc_->height() <= 0) {
    QMessageBox::warning(this, tr("Export"),
                         tr("No document to export."));
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, tr("Export PNG"), QString(), tr("PNG Images (*.png)"));
  if (path.isEmpty()) return;

  TuxImage out(doc_->width(), doc_->height());
  compose(doc_->tree(), out);

  QString err;
  if (!savePng(path, out, &err)) {
    QMessageBox::warning(this, tr("Export Failed"),
                         tr("Could not write %1:\n%2").arg(path, err));
    return;
  }
  statusBar()->showMessage(tr("Exported %1").arg(path), 3000);
}

void MainWindow::onLayerAdd() {
  if (!doc_) return;
  const int n = static_cast<int>(doc_->tree().size()) + 1;
  const std::string name = "Layer " + std::to_string(n);
  const int w = doc_->width();
  const int h = doc_->height();
  const LayerId id = doc_->nextLayerId();
  const int prevActive = doc_->activeLayerIndex();

  // Stash the layer ptr so that redo can rebuild its identity. We hold a
  // shared_ptr owned by the command; when it's "attached" to the tree we
  // transfer ownership in, and when detached (undone) we take it back.
  auto stash = std::make_shared<std::unique_ptr<LayerBase>>();

  auto doIt = [this, stash, name, w, h, id, prevActive]() mutable {
    std::unique_ptr<LayerBase> layer;
    if (*stash) {
      layer = std::move(*stash);
    } else {
      auto px = std::make_unique<PixelLayer>(w, h);
      px->id = id;
      px->name = name;
      layer = std::move(px);
    }
    const std::size_t insertIdx = doc_->tree().size();
    doc_->tree().add(std::move(layer));
    doc_->setActiveLayerIndex(static_cast<int>(insertIdx));
    refreshAfterUndoRedo();
    (void)prevActive;
  };
  auto undoIt = [this, stash, prevActive]() mutable {
    const std::size_t lastIdx = doc_->tree().size() - 1;
    *stash = doc_->tree().removeAt(lastIdx);
    doc_->setActiveLayerIndex(prevActive);
    refreshAfterUndoRedo();
  };

  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>("Add Layer",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
}

void MainWindow::onLayerDelete() {
  if (!doc_) return;
  const int i = doc_->activeLayerIndex();
  if (i < 0 || static_cast<std::size_t>(i) >= doc_->tree().size()) return;
  const std::size_t idx = static_cast<std::size_t>(i);
  const int prevActive = i;
  auto stash = std::make_shared<std::unique_ptr<LayerBase>>();

  auto doIt = [this, stash, idx]() mutable {
    *stash = doc_->tree().removeAt(idx);
    const int newActive = std::min(static_cast<int>(doc_->tree().size()) - 1,
                                   static_cast<int>(idx));
    doc_->setActiveLayerIndex(newActive);
    refreshAfterUndoRedo();
  };
  auto undoIt = [this, stash, idx, prevActive]() mutable {
    if (!*stash) return;
    doc_->tree().insertAt(idx, std::move(*stash));
    doc_->setActiveLayerIndex(prevActive);
    refreshAfterUndoRedo();
  };

  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>("Delete Layer",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
}

void MainWindow::onLayerMoveUp() {
  if (!doc_) return;
  const int i = doc_->activeLayerIndex();
  if (i < 0 || i + 1 >= static_cast<int>(doc_->tree().size())) return;
  const std::size_t from = static_cast<std::size_t>(i);
  const std::size_t to = from + 1;

  auto doIt = [this, from, to]() {
    doc_->tree().move(from, to);
    doc_->setActiveLayerIndex(static_cast<int>(to));
    refreshAfterUndoRedo();
  };
  auto undoIt = [this, from, to]() {
    doc_->tree().move(to, from);
    doc_->setActiveLayerIndex(static_cast<int>(from));
    refreshAfterUndoRedo();
  };

  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>("Move Layer Up",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
}

void MainWindow::onLayerMoveDown() {
  if (!doc_) return;
  const int i = doc_->activeLayerIndex();
  if (i <= 0) return;
  const std::size_t from = static_cast<std::size_t>(i);
  const std::size_t to = from - 1;

  auto doIt = [this, from, to]() {
    doc_->tree().move(from, to);
    doc_->setActiveLayerIndex(static_cast<int>(to));
    refreshAfterUndoRedo();
  };
  auto undoIt = [this, from, to]() {
    doc_->tree().move(to, from);
    doc_->setActiveLayerIndex(static_cast<int>(from));
    refreshAfterUndoRedo();
  };

  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>("Move Layer Down",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
}

void MainWindow::onLayerPanelMutated() {
  if (canvas_) canvas_->requestRecomposite();
}

void MainWindow::onActiveLayerChanged() {
  // Placeholder for future painting-target updates.
}

void MainWindow::onLayerPainted() {
  if (!doc_) return;
  // Brush stroke commit (only populated when the brush was the active tool
  // for the just-released gesture).
  if (brushTool_) {
    auto info = brushTool_->takeLastStroke();
    if (info.layer && info.target) {
      auto cmd = std::make_unique<PaintCommand>(
          info.target, std::move(info.recorded.before),
          std::move(info.recorded.after), "Paint Stroke");
      undoStack_->push(std::move(cmd));
    }
  }
  // Marquee commit. Push a SelectionCommand and refresh the ants overlay.
  if (marqueeTool_) {
    if (auto commit = marqueeTool_->takeCommit()) {
      doc_->setSelection(commit->after ? commit->after->clone() : nullptr);
      undoStack_->push(std::make_unique<SelectionCommand>(
          doc_.get(), std::move(commit->before), std::move(commit->after),
          commit->label));
      if (canvas_) canvas_->refreshSelectionOverlay();
    }
  }
  if (layersPanel_) layersPanel_->refresh();
}

void MainWindow::setActiveTool(ToolId id) {
  if (id == activeToolId_) return;
  activeToolId_ = id;
  switch (id) {
    case ToolId::Brush:
      if (canvas_) canvas_->setTool(brushTool_.get());
      break;
    case ToolId::Marquee:
      if (canvas_) canvas_->setTool(marqueeTool_.get());
      break;
  }
  if (toolsPanel_) toolsPanel_->setActiveTool(id);
  if (canvas_) canvas_->refreshBrushCursor();
}

void MainWindow::onToolPicked(ToolId id) { setActiveTool(id); }

void MainWindow::onEditUndo() {
  if (!undoStack_) return;
  const auto r = undoStack_->undo();
  if (!r.touched) return;
  refreshAfterUndoRedo(r.dirtyRect);
}

void MainWindow::onEditRedo() {
  if (!undoStack_) return;
  const auto r = undoStack_->redo();
  if (!r.touched) return;
  refreshAfterUndoRedo(r.dirtyRect);
}

void MainWindow::onLayerVisibilityChange(LayerBase* layer, bool oldVal, bool newVal) {
  if (!layer || !undoStack_) return;
  auto doIt = [this, layer, newVal]() {
    layer->visible = newVal;
    refreshAfterUndoRedo();
  };
  auto undoIt = [this, layer, oldVal]() {
    layer->visible = oldVal;
    refreshAfterUndoRedo();
  };
  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>("Toggle Visibility",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
}

void MainWindow::onLayerBlendChange(LayerBase* layer, BlendMode oldMode,
                                    BlendMode newMode) {
  if (!layer || !undoStack_) return;
  auto doIt = [this, layer, newMode]() {
    layer->blend = newMode;
    refreshAfterUndoRedo();
  };
  auto undoIt = [this, layer, oldMode]() {
    layer->blend = oldMode;
    refreshAfterUndoRedo();
  };
  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>("Change Blend Mode",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
}

void MainWindow::onLayerOpacityCommit(LayerBase* layer, float oldVal, float newVal) {
  if (!layer || !undoStack_) return;
  auto doIt = [this, layer, newVal]() {
    layer->opacity = newVal;
    refreshAfterUndoRedo();
  };
  auto undoIt = [this, layer, oldVal]() {
    layer->opacity = oldVal;
    refreshAfterUndoRedo();
  };
  // The live-preview in LayerRowWidget::onOpacitySliderMoved has already
  // applied `newVal` — don't re-apply in doIt() here; just register the
  // reversible pair. Calling doIt() would be a no-op since newVal is the
  // current value, but we skip it to avoid a redundant refresh.
  undoStack_->push(std::make_unique<LayerOpCommand>("Change Opacity",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
}

void MainWindow::onAddLayerMask() {
  if (!doc_) return;
  auto* px = dynamic_cast<PixelLayer*>(doc_->activeLayer());
  if (!px) {
    statusBar()->showMessage(tr("Masks require a pixel layer."), 3000);
    return;
  }
  if (px->mask) {
    statusBar()->showMessage(tr("Layer already has a mask."), 3000);
    return;
  }
  const int w = doc_->width();
  const int h = doc_->height();
  auto stash = std::make_shared<std::unique_ptr<LayerMask>>();

  auto doIt = [this, px, stash, w, h]() mutable {
    if (*stash) {
      px->mask = std::move(*stash);
    } else {
      auto m = std::make_unique<LayerMask>(w, h);
      m->image.fill(Rgba32F(1.f, 1.f, 1.f, 1.f));  // fully reveal
      px->mask = std::move(m);
    }
    doc_->setPaintTarget(PaintTarget::Mask);
    refreshAfterUndoRedo();
  };
  auto undoIt = [this, px, stash]() mutable {
    *stash = std::move(px->mask);
    doc_->setPaintTarget(PaintTarget::Layer);
    refreshAfterUndoRedo();
  };

  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>("Add Layer Mask",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
}

void MainWindow::onDeleteLayerMask() {
  if (!doc_) return;
  auto* px = dynamic_cast<PixelLayer*>(doc_->activeLayer());
  if (!px || !px->mask) return;
  onLayerDeleteMaskRequest(px);
}

void MainWindow::onLayerPaintTargetChange(LayerBase* layer, PaintTarget target) {
  if (!doc_ || !layer) return;
  const auto n = doc_->tree().size();
  for (std::size_t i = 0; i < n; ++i) {
    if (doc_->tree().at(i) == layer) {
      doc_->setActiveLayerIndex(static_cast<int>(i));
      break;
    }
  }
  doc_->setPaintTarget(target);
  if (layersPanel_) layersPanel_->refresh();
}

void MainWindow::onLayerMaskEnabledToggle(LayerBase* layer, bool oldVal,
                                          bool newVal) {
  if (!layer || !layer->mask || !undoStack_) return;
  auto doIt = [this, layer, newVal]() {
    if (layer->mask) layer->mask->enabled = newVal;
    refreshAfterUndoRedo();
  };
  auto undoIt = [this, layer, oldVal]() {
    if (layer->mask) layer->mask->enabled = oldVal;
    refreshAfterUndoRedo();
  };
  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>("Toggle Mask Enabled",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
}

void MainWindow::onLayerDeleteMaskRequest(LayerBase* layer) {
  if (!layer || !layer->mask || !undoStack_) return;
  auto stash = std::make_shared<std::unique_ptr<LayerMask>>();

  auto doIt = [this, layer, stash]() mutable {
    *stash = std::move(layer->mask);
    if (doc_) doc_->setPaintTarget(PaintTarget::Layer);
    refreshAfterUndoRedo();
  };
  auto undoIt = [this, layer, stash]() mutable {
    layer->mask = std::move(*stash);
    refreshAfterUndoRedo();
  };

  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>("Delete Layer Mask",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
}

void MainWindow::onBrushSizeIncrease() {
  if (!brushTool_) return;
  const int d = brushTool_->brush().diameter();
  const int step = std::max(1, d / 10);
  brushTool_->brush().setDiameter(d + step);
  if (toolsPanel_) toolsPanel_->refreshFromBrush();
  if (canvas_) canvas_->refreshBrushCursor();
  statusBar()->showMessage(
      tr("Brush size: %1").arg(brushTool_->brush().diameter()), 1500);
}

void MainWindow::onBrushSizeDecrease() {
  if (!brushTool_) return;
  const int d = brushTool_->brush().diameter();
  const int step = std::max(1, d / 10);
  brushTool_->brush().setDiameter(d - step);
  if (toolsPanel_) toolsPanel_->refreshFromBrush();
  if (canvas_) canvas_->refreshBrushCursor();
  statusBar()->showMessage(
      tr("Brush size: %1").arg(brushTool_->brush().diameter()), 1500);
}

void MainWindow::onSelectAll() {
  if (!doc_ || doc_->width() <= 0 || doc_->height() <= 0) return;
  auto before = doc_->selection() ? doc_->selection()->clone() : nullptr;
  doc_->setSelection(SelectionMask::makeAll(doc_->width(), doc_->height()));
  auto after = doc_->selection()->clone();
  undoStack_->push(std::make_unique<SelectionCommand>(
      doc_.get(), std::move(before), std::move(after), "Select All"));
  if (canvas_) canvas_->refreshSelectionOverlay();
  statusBar()->showMessage(tr("Selection: all"), 1500);
}

void MainWindow::onDeselect() {
  if (!doc_ || !doc_->selection()) return;
  auto before = doc_->selection()->clone();
  doc_->setSelection(nullptr);
  undoStack_->push(std::make_unique<SelectionCommand>(
      doc_.get(), std::move(before), nullptr, "Deselect"));
  if (canvas_) canvas_->refreshSelectionOverlay();
  statusBar()->showMessage(tr("Deselected"), 1500);
}

void MainWindow::onSelectInverse() {
  // Photoshop greys out Inverse when nothing is selected; mirror that.
  if (!doc_ || !doc_->selection()) return;
  auto before = doc_->selection()->clone();
  auto inverted = doc_->selection()->clone();
  inverted->invert();
  doc_->setSelection(std::move(inverted));
  auto after = doc_->selection()->clone();
  undoStack_->push(std::make_unique<SelectionCommand>(
      doc_.get(), std::move(before), std::move(after), "Inverse Selection"));
  if (canvas_) canvas_->refreshSelectionOverlay();
  statusBar()->showMessage(tr("Selection inverted"), 1500);
}

}  // namespace tuxels
