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
#include <set>

#include "compositor/compose.h"
#include "core/Document.h"
#include "core/SelectionMask.h"
#include "core/Histogram.h"
#include "history/CropCommand.h"
#include "history/LayerOpCommand.h"
#include "history/LayerParamsCommand.h"
#include "history/MoveLayerCommand.h"
#include "history/PaintCommand.h"
#include "history/SelectionCommand.h"
#include "history/TransformCommand.h"
#include "history/UndoStack.h"
#include "io/PngIO.h"
#include "io/TxlIO.h"
#include "layers/BrightnessContrast.h"
#include "layers/CurvesAdjustment.h"
#include "layers/GroupLayer.h"
#include "layers/HueSaturation.h"
#include "layers/LayerMask.h"
#include "layers/LevelsAdjustment.h"
#include "layers/PixelLayer.h"
#include "tools/BrushTool.h"
#include "tools/BucketTool.h"
#include "tools/CropTool.h"
#include "tools/LassoTool.h"
#include "tools/MagicWandTool.h"
#include "tools/MarqueeTool.h"
#include "tools/MoveTool.h"
#include "tools/PolyLassoTool.h"
#include "tools/SelectByColorTool.h"
#include "tools/TransformTool.h"
#include "ui/CanvasView.h"
#include "ui/LayersPanel.h"
#include "ui/PropertiesDock.h"
#include "ui/PropertiesPaneGroup.h"
#include "ui/ToolsPanel.h"

namespace tuxels {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("Tuxels");
  resize(1400, 900);

  brushTool_ = std::make_unique<BrushTool>();
  marqueeTool_ = std::make_unique<MarqueeTool>();
  bucketTool_ = std::make_unique<BucketTool>();
  wandTool_ = std::make_unique<MagicWandTool>();
  cropTool_ = std::make_unique<CropTool>();
  moveTool_ = std::make_unique<MoveTool>();
  transformTool_ = std::make_unique<TransformTool>();
  lassoTool_ = std::make_unique<LassoTool>();
  polyLassoTool_ = std::make_unique<PolyLassoTool>();
  selectByColorTool_ = std::make_unique<SelectByColorTool>();
  undoStack_ = std::make_unique<UndoStack>(/*maxDepth=*/64);

  canvas_ = new CanvasView(this);
  canvas_->setTool(brushTool_.get());
  canvas_->setTransformTool(transformTool_.get());
  canvas_->setToolCursor(CanvasView::cursorForTool(ToolId::Brush));
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

  auto* placeAct = fileMenu->addAction(tr("&Place…"));
  placeAct->setShortcut(QKeySequence(tr("Ctrl+Shift+P")));
  connect(placeAct, &QAction::triggered, this, &MainWindow::onFilePlace);

  auto* saveAsAct = fileMenu->addAction(tr("&Save As…"));
  saveAsAct->setShortcut(QKeySequence::SaveAs);
  connect(saveAsAct, &QAction::triggered, this, &MainWindow::onFileSaveAs);

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
  editMenu->addSeparator();
  auto* freeXformAct = editMenu->addAction(tr("Free &Transform"));
  freeXformAct->setShortcut(QKeySequence(tr("Ctrl+T")));
  connect(freeXformAct, &QAction::triggered, this,
          &MainWindow::onEditFreeTransform);

  mb->addMenu(tr("&Image"));

  auto* layerMenu = mb->addMenu(tr("&Layer"));
  auto* addLayerAct = layerMenu->addAction(tr("&New Layer"));
  addLayerAct->setShortcut(QKeySequence(tr("Ctrl+Shift+N")));
  connect(addLayerAct, &QAction::triggered, this, &MainWindow::onLayerAdd);

  auto* newGroupAct = layerMenu->addAction(tr("New &Group"));
  connect(newGroupAct, &QAction::triggered, this,
          &MainWindow::onLayerNewGroup);

  auto* delLayerAct = layerMenu->addAction(tr("&Delete Layer"));
  connect(delLayerAct, &QAction::triggered, this, &MainWindow::onLayerDelete);

  layerMenu->addSeparator();
  // Group / Ungroup (M5-S4). Enabled state tracks the active layer:
  // Group Layer requires any active layer; Ungroup Layer requires the
  // active layer to be a Group. Updated in `updateGroupActionStates()`,
  // called from `onActiveLayerChanged()` and the menu's `aboutToShow`.
  groupLayerAction_ = layerMenu->addAction(tr("&Group Layer"));
  groupLayerAction_->setShortcut(QKeySequence(tr("Ctrl+G")));
  connect(groupLayerAction_, &QAction::triggered, this,
          &MainWindow::onLayerGroupActive);
  ungroupLayerAction_ = layerMenu->addAction(tr("&Ungroup Layer"));
  ungroupLayerAction_->setShortcut(QKeySequence(tr("Ctrl+Shift+G")));
  connect(ungroupLayerAction_, &QAction::triggered, this,
          &MainWindow::onLayerUngroupActive);
  connect(layerMenu, &QMenu::aboutToShow, this,
          &MainWindow::updateGroupActionStates);
  // Initialize disabled — refreshed once a doc is loaded.
  groupLayerAction_->setEnabled(false);
  ungroupLayerAction_->setEnabled(false);

  layerMenu->addSeparator();
  auto* addMaskAct = layerMenu->addAction(tr("Add Layer &Mask"));
  connect(addMaskAct, &QAction::triggered, this, &MainWindow::onAddLayerMask);
  auto* delMaskAct = layerMenu->addAction(tr("D&elete Layer Mask"));
  connect(delMaskAct, &QAction::triggered, this, &MainWindow::onDeleteLayerMask);

  auto* clipAct = layerMenu->addAction(tr("Create Clipping Mas&k"));
  clipAct->setShortcut(QKeySequence(tr("Ctrl+Alt+G")));
  connect(clipAct, &QAction::triggered, this,
          &MainWindow::onToggleClipToBelow);

  layerMenu->addSeparator();
  auto* adjMenu = layerMenu->addMenu(tr("New &Adjustment Layer"));
  auto* addLevelsAct = adjMenu->addAction(tr("&Levels…"));
  addLevelsAct->setShortcut(QKeySequence(tr("Ctrl+L")));
  connect(addLevelsAct, &QAction::triggered, this,
          &MainWindow::onLayerAddLevels);
  auto* addCurvesAct = adjMenu->addAction(tr("&Curves…"));
  addCurvesAct->setShortcut(QKeySequence(tr("Ctrl+M")));
  connect(addCurvesAct, &QAction::triggered, this,
          &MainWindow::onLayerAddCurves);
  auto* addHueSatAct = adjMenu->addAction(tr("&Hue/Saturation…"));
  addHueSatAct->setShortcut(QKeySequence(tr("Ctrl+U")));
  connect(addHueSatAct, &QAction::triggered, this,
          &MainWindow::onLayerAddHueSaturation);
  auto* addBCAct = adjMenu->addAction(tr("&Brightness/Contrast…"));
  connect(addBCAct, &QAction::triggered, this,
          &MainWindow::onLayerAddBrightnessContrast);

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

  auto* pickBucket = new QAction(this);
  pickBucket->setShortcut(QKeySequence(tr("G")));
  connect(pickBucket, &QAction::triggered, this,
          [this]() { setActiveTool(ToolId::Bucket); });
  addAction(pickBucket);

  auto* pickWand = new QAction(this);
  pickWand->setShortcut(QKeySequence(tr("W")));
  connect(pickWand, &QAction::triggered, this,
          [this]() { setActiveTool(ToolId::MagicWand); });
  addAction(pickWand);

  // Shift+W cycles within the color-selection group (Magic Wand ↔ Select
  // By Color), matching Photoshop's grouped-tool behaviour.
  auto* cycleWandGroup = new QAction(this);
  cycleWandGroup->setShortcut(QKeySequence(tr("Shift+W")));
  connect(cycleWandGroup, &QAction::triggered, this, [this]() {
    setActiveTool(activeToolId_ == ToolId::MagicWand ? ToolId::SelectByColor
                                                     : ToolId::MagicWand);
  });
  addAction(cycleWandGroup);

  auto* pickCrop = new QAction(this);
  pickCrop->setShortcut(QKeySequence(tr("C")));
  connect(pickCrop, &QAction::triggered, this,
          [this]() { setActiveTool(ToolId::Crop); });
  addAction(pickCrop);

  auto* pickMove = new QAction(this);
  pickMove->setShortcut(QKeySequence(tr("V")));
  connect(pickMove, &QAction::triggered, this,
          [this]() { setActiveTool(ToolId::Move); });
  addAction(pickMove);

  auto* pickLasso = new QAction(this);
  pickLasso->setShortcut(QKeySequence(tr("L")));
  connect(pickLasso, &QAction::triggered, this,
          [this]() { setActiveTool(ToolId::Lasso); });
  addAction(pickLasso);

  // Enter / Escape — transform accept / cancel. Guarded inside the handler
  // so they're no-ops unless the TransformTool is active; normal typing is
  // unaffected because QInputDialogs/modals steal the key before this
  // window-scope action sees it.
  auto* xformAccept = new QAction(this);
  xformAccept->setShortcuts({QKeySequence(Qt::Key_Return),
                             QKeySequence(Qt::Key_Enter)});
  connect(xformAccept, &QAction::triggered, this,
          &MainWindow::onTransformAccept);
  addAction(xformAccept);

  auto* xformCancel = new QAction(this);
  xformCancel->setShortcut(QKeySequence(Qt::Key_Escape));
  connect(xformCancel, &QAction::triggered, this,
          &MainWindow::onTransformCancel);
  addAction(xformCancel);
}

void MainWindow::buildDocks() {
  toolsPanel_ = new ToolsPanel(this);
  addDockWidget(Qt::LeftDockWidgetArea, toolsPanel_);
  toolsPanel_->setBrushTool(brushTool_.get());
  toolsPanel_->setBucketTool(bucketTool_.get());
  toolsPanel_->setMagicWandTool(wandTool_.get());
  toolsPanel_->setSelectByColorTool(selectByColorTool_.get());
  toolsPanel_->setLassoTools(lassoTool_.get(), polyLassoTool_.get());
  toolsPanel_->setActiveTool(activeToolId_);
  connect(toolsPanel_, &ToolsPanel::toolPicked, this,
          &MainWindow::onToolPicked);
  connect(toolsPanel_, &ToolsPanel::marqueeModeChanged, this,
          [this](SelectionMode m) {
            if (marqueeTool_) marqueeTool_->setMode(m);
          });
  connect(toolsPanel_, &ToolsPanel::wandModeChanged, this,
          [this](SelectionMode m) {
            // Wand and Select-By-Color share the options row; push the
            // mode into both so switching between them preserves it.
            if (wandTool_) wandTool_->setMode(m);
            if (selectByColorTool_) selectByColorTool_->setMode(m);
          });
  connect(toolsPanel_, &ToolsPanel::lassoModeChanged, this,
          [this](SelectionMode m) {
            // Both lasso tools share the persistent mode so swapping
            // between Lasso and Polygonal Lasso doesn't flip it.
            if (lassoTool_) lassoTool_->setMode(m);
            if (polyLassoTool_) polyLassoTool_->setMode(m);
          });
  if (marqueeTool_) toolsPanel_->setMarqueeMode(marqueeTool_->mode());
  if (wandTool_) toolsPanel_->setWandMode(wandTool_->mode());
  if (lassoTool_) toolsPanel_->setLassoMode(lassoTool_->mode());

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
  connect(layersPanel_, &LayersPanel::editAdjustmentRequested, this,
          &MainWindow::onEditAdjustmentRequested);
  connect(layersPanel_, &LayersPanel::toggleClipToBelowRequested, this,
          &MainWindow::onLayerToggleClipToBelow);

  // Properties dock — non-modal editor for adjustment-layer params (M4-S2).
  // Tab-stacked under LayersPanel by default; users can drag-detach.
  propertiesDock_ = new PropertiesDock(this);
  addDockWidget(Qt::RightDockWidgetArea, propertiesDock_);
  tabifyDockWidget(layersPanel_, propertiesDock_);
  layersPanel_->raise();  // Layers shows on top by default; user clicks the
                          // Properties tab (or an adjustment-layer thumb)
                          // to bring it forward.
  connect(propertiesDock_, &PropertiesDock::previewChanged, this,
          [this]() { if (canvas_) canvas_->requestRecomposite(); });
  connect(propertiesDock_, &PropertiesDock::levelsCommitRequested, this,
          [this](LevelsAdjustment* layer,
                 std::array<LevelsParams, 4> before,
                 std::array<LevelsParams, 4> after) {
            if (!layer) return;
            using ParamArr = std::array<LevelsParams, 4>;
            auto setter = [](LevelsAdjustment* l, const ParamArr& p) {
              l->setAllParams(p);
            };
            undoStack_->push(
                std::make_unique<LayerParamsCommand<LevelsAdjustment, ParamArr>>(
                    layer, before, after, setter, "Edit Levels"));
            if (canvas_) canvas_->requestRecomposite();
          });
  connect(propertiesDock_, &PropertiesDock::curvesCommitRequested, this,
          [this](CurvesAdjustment* layer,
                 CurvesAdjustment::PointsArray before,
                 CurvesAdjustment::PointsArray after) {
            if (!layer) return;
            using PtArr = CurvesAdjustment::PointsArray;
            auto setter = [](CurvesAdjustment* l, const PtArr& p) {
              l->setAllPoints(p);
            };
            undoStack_->push(
                std::make_unique<LayerParamsCommand<CurvesAdjustment, PtArr>>(
                    layer, before, after, setter, "Edit Curves"));
            if (canvas_) canvas_->requestRecomposite();
          });
  connect(propertiesDock_, &PropertiesDock::hueSatCommitRequested, this,
          [this](HueSaturation* layer, HueSaturationParams before,
                 HueSaturationParams after) {
            if (!layer) return;
            auto setter = [](HueSaturation* l, const HueSaturationParams& p) {
              l->setParams(p);
            };
            undoStack_->push(
                std::make_unique<
                    LayerParamsCommand<HueSaturation, HueSaturationParams>>(
                    layer, before, after, setter, "Edit Hue/Saturation"));
            if (canvas_) canvas_->requestRecomposite();
          });
  connect(propertiesDock_, &PropertiesDock::brightnessContrastCommitRequested,
          this,
          [this](BrightnessContrast* layer,
                 BrightnessContrastParams before,
                 BrightnessContrastParams after) {
            if (!layer) return;
            auto setter = [](BrightnessContrast* l,
                             const BrightnessContrastParams& p) {
              l->setParams(p);
            };
            undoStack_->push(
                std::make_unique<LayerParamsCommand<
                    BrightnessContrast, BrightnessContrastParams>>(
                    layer, before, after, setter,
                    "Edit Brightness/Contrast"));
            if (canvas_) canvas_->requestRecomposite();
          });
  // M6-S0: group properties commit. Setter applies all four fields
  // atomically. The pane has already mutated the layer to `after` state
  // before this fires; pushing the command preserves the diff for undo.
  connect(propertiesDock_, &PropertiesDock::groupCommitRequested, this,
          [this](GroupLayer* layer, GroupProperties before,
                 GroupProperties after) {
            if (!layer) return;
            auto setter = [](GroupLayer* g, const GroupProperties& p) {
              g->name = p.name;
              g->blend = p.blend;
              g->opacity = p.opacity;
              g->clipToBelow = p.clipToBelow;
            };
            undoStack_->push(
                std::make_unique<LayerParamsCommand<GroupLayer, GroupProperties>>(
                    layer, before, after, setter, "Edit Group Properties"));
            // Layer-row chevron + name + indent depend on these fields,
            // and a blend transition between Pass-Through and isolated
            // re-shuffles compose. Refresh the panel + canvas.
            if (layersPanel_) layersPanel_->refresh();
            if (canvas_) canvas_->requestRecomposite();
          });
}

void MainWindow::setDocument(std::unique_ptr<Document> doc) {
  doc_ = std::move(doc);
  if (undoStack_) undoStack_->clear();
  canvas_->setDocument(doc_.get());
  layersPanel_->setDocument(doc_.get());
}

void MainWindow::refreshAfterUndoRedo(Rect dirtyRect) {
  if (!doc_) return;
  // Active id may point at a layer that's been removed; clear to none and
  // let the panel pick a sensible default on the next user click.
  if (doc_->activeLayerId() != 0 &&
      doc_->tree().findById(doc_->activeLayerId()) == nullptr) {
    if (!doc_->tree().empty()) {
      doc_->setActiveLayerId(doc_->tree().at(doc_->tree().size() - 1)->id);
    } else {
      doc_->setActiveLayerId(0);
    }
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
      this, tr("Open"), QString(),
      tr("Tuxels or PNG (*.txl *.png);;Tuxels Document (*.txl);;PNG Images (*.png)"));
  if (path.isEmpty()) return;

  const QString suffix = QFileInfo(path).suffix().toLower();
  if (suffix == "txl") {
    std::string err;
    auto loaded = loadTxl(path.toStdString(), &err);
    if (!loaded) {
      QMessageBox::warning(this, tr("Open Failed"),
                           tr("Could not open %1:\n%2")
                               .arg(path, QString::fromStdString(err)));
      return;
    }
    setDocument(std::move(*loaded));
    layersPanel_->refresh();
    canvas_->requestRecomposite();
    canvas_->refreshSelectionOverlay();
    statusBar()->showMessage(tr("Opened %1").arg(path), 3000);
    return;
  }

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

void MainWindow::onFilePlace() {
  if (!doc_ || doc_->width() <= 0 || doc_->height() <= 0) {
    QMessageBox::warning(this, tr("Place"),
                         tr("Open or create a document first."));
    return;
  }
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Place Image"), QString(), tr("PNG Images (*.png)"));
  if (path.isEmpty()) return;

  QString err;
  auto img = loadPng(path, &err);
  if (!img) {
    QMessageBox::warning(this, tr("Place Failed"),
                         tr("Could not open %1:\n%2").arg(path, err));
    return;
  }

  // Center the placed image inside the doc. Oversized images keep offscreen
  // pixels thanks to the M2-S0 origin plumbing — they become visible once
  // the layer is moved, or the doc is resized/uncropped.
  const int pw = img->width();
  const int ph = img->height();
  const int ox = (doc_->width() - pw) / 2;
  const int oy = (doc_->height() - ph) / 2;
  const std::string name = QFileInfo(path).fileName().toStdString();
  const LayerId id = doc_->nextLayerId();
  const LayerId prevActiveId = doc_->activeLayerId();

  // Shared stash so redo reinstalls the exact same layer instance that undo
  // detached — mirrors `onLayerAdd`.
  auto stash = std::make_shared<std::unique_ptr<LayerBase>>();
  auto cached = std::make_shared<TuxImage>(std::move(*img));

  auto doIt = [this, stash, cached, name, id, ox, oy]() mutable {
    std::unique_ptr<LayerBase> layer;
    if (*stash) {
      layer = std::move(*stash);
    } else {
      auto px = std::make_unique<PixelLayer>();
      px->id = id;
      px->name = name;
      px->originX = ox;
      px->originY = oy;
      // First execution consumes the decoded image; subsequent redos reuse
      // the detached layer, so `cached` only matters on the first apply.
      px->image = std::move(*cached);
      layer = std::move(px);
    }
    doc_->tree().add(std::move(layer));
    doc_->setActiveLayerId(id);
    refreshAfterUndoRedo();
  };
  auto undoIt = [this, stash, prevActiveId]() mutable {
    const std::size_t lastIdx = doc_->tree().size() - 1;
    *stash = doc_->tree().removeAt(lastIdx);
    doc_->setActiveLayerId(prevActiveId);
    refreshAfterUndoRedo();
  };

  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>("Place Image",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
  statusBar()->showMessage(
      tr("Placed %1 (%2×%3) at (%4, %5)").arg(path).arg(pw).arg(ph).arg(ox).arg(oy),
      3000);
}

void MainWindow::onFileSaveAs() {
  if (!doc_ || doc_->width() <= 0 || doc_->height() <= 0) {
    QMessageBox::warning(this, tr("Save"), tr("No document to save."));
    return;
  }
  QString path = QFileDialog::getSaveFileName(
      this, tr("Save As"), QString(), tr("Tuxels Document (*.txl)"));
  if (path.isEmpty()) return;
  if (!path.endsWith(".txl", Qt::CaseInsensitive)) path += ".txl";

  std::string err;
  if (!saveTxl(path.toStdString(), *doc_, &err)) {
    QMessageBox::warning(this, tr("Save Failed"),
                         tr("Could not write %1:\n%2")
                             .arg(path, QString::fromStdString(err)));
    return;
  }
  statusBar()->showMessage(tr("Saved %1").arg(path), 3000);
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

void MainWindow::onLayerNewGroup() {
  if (!doc_) return;
  // Name the new group "Group N" where N counts existing groups in the
  // tree (recursively). Matches PS's "Group 1, Group 2..." pattern.
  int existingGroups = 0;
  doc_->tree().forEach([&existingGroups](const LayerBase* l) {
    if (l && l->kind() == LayerKind::Group) ++existingGroups;
  });
  const std::string name = "Group " + std::to_string(existingGroups + 1);
  const LayerId id = doc_->nextLayerId();
  const LayerId prevActiveId = doc_->activeLayerId();

  // Insert at active layer's parent + (active's index + 1) so the group
  // appears immediately above the active layer (matches PS). When no
  // active or active no longer exists, append at root top.
  LayerId targetParentId = 0;
  std::size_t targetIdx = doc_->tree().size();
  if (prevActiveId != 0) {
    if (auto loc = doc_->tree().locate(prevActiveId)) {
      targetParentId = loc->parent ? loc->parent->id : 0;
      targetIdx = loc->index + 1;
    }
  }

  auto stash = std::make_shared<std::unique_ptr<LayerBase>>();

  auto resolveParent = [this](LayerId pid) -> GroupLayer* {
    if (pid == 0) return nullptr;
    return dynamic_cast<GroupLayer*>(doc_->tree().findById(pid));
  };

  auto doIt = [this, stash, name, id, targetParentId, targetIdx,
               resolveParent]() mutable {
    std::unique_ptr<LayerBase> layer;
    if (*stash) {
      layer = std::move(*stash);
    } else {
      auto g = std::make_unique<GroupLayer>();
      g->id = id;
      g->name = name;
      // Default: PassThrough blend (set by GroupLayer ctor), expanded,
      // no mask, opacity 1.
      layer = std::move(g);
    }
    doc_->tree().insertAtPath(resolveParent(targetParentId), targetIdx,
                               std::move(layer));
    doc_->setActiveLayerId(id);
    refreshAfterUndoRedo();
  };
  auto undoIt = [this, stash, prevActiveId, targetParentId, targetIdx,
                 resolveParent]() mutable {
    *stash = doc_->tree().removeFromPath(resolveParent(targetParentId),
                                          targetIdx);
    doc_->setActiveLayerId(prevActiveId);
    refreshAfterUndoRedo();
  };

  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>("New Group",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
}

void MainWindow::onLayerGroupActive() {
  if (!doc_) return;
  const LayerId activeId = doc_->activeLayerId();
  if (activeId == 0) return;
  auto loc = doc_->tree().locate(activeId);
  if (!loc) return;

  // Capture by parent id so closures survive reorders.
  const LayerId parentId = loc->parent ? loc->parent->id : 0;
  const std::size_t idx = loc->index;

  // New group's name + id allocated up front.
  int existingGroups = 0;
  doc_->tree().forEach([&existingGroups](const LayerBase* l) {
    if (l && l->kind() == LayerKind::Group) ++existingGroups;
  });
  const std::string groupName = "Group " + std::to_string(existingGroups + 1);
  const LayerId groupId = doc_->nextLayerId();

  // Stash the group instance for redo identity (preserves the group's id
  // + any properties the user might tweak before further undo/redo).
  auto stashGroup = std::make_shared<std::unique_ptr<LayerBase>>();

  auto resolveParent = [this](LayerId pid) -> GroupLayer* {
    if (pid == 0) return nullptr;
    return dynamic_cast<GroupLayer*>(doc_->tree().findById(pid));
  };

  auto doIt = [this, stashGroup, groupName, groupId, activeId, parentId,
               idx, resolveParent]() mutable {
    GroupLayer* parent = resolveParent(parentId);
    auto layer = doc_->tree().removeFromPath(parent, idx);
    if (!layer) return;

    std::unique_ptr<LayerBase> group;
    if (*stashGroup) {
      group = std::move(*stashGroup);
    } else {
      auto g = std::make_unique<GroupLayer>();
      g->id = groupId;
      g->name = groupName;
      group = std::move(g);
    }
    auto* gPtr = static_cast<GroupLayer*>(group.get());
    gPtr->children.insert(gPtr->children.begin(), std::move(layer));
    doc_->tree().insertAtPath(parent, idx, std::move(group));
    doc_->setActiveLayerId(groupId);
    refreshAfterUndoRedo();
    (void)activeId;  // referenced in undoIt
  };
  auto undoIt = [this, stashGroup, groupId, activeId, parentId, idx,
                 resolveParent]() mutable {
    auto loc2 = doc_->tree().locate(groupId);
    if (!loc2) return;
    auto group = doc_->tree().removeFromPath(loc2->parent, loc2->index);
    if (!group) return;
    auto* gPtr = static_cast<GroupLayer*>(group.get());
    if (gPtr->children.empty()) return;  // defensive — should never happen
    auto layer = std::move(gPtr->children.front());
    gPtr->children.erase(gPtr->children.begin());
    doc_->tree().insertAtPath(resolveParent(parentId), idx, std::move(layer));
    *stashGroup = std::move(group);
    doc_->setActiveLayerId(activeId);
    refreshAfterUndoRedo();
  };

  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>("Group Layer",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
}

void MainWindow::onLayerUngroupActive() {
  if (!doc_) return;
  LayerBase* active = doc_->activeLayer();
  if (!active || active->kind() != LayerKind::Group) return;
  auto* g = static_cast<GroupLayer*>(active);
  const LayerId groupId = g->id;
  auto loc = doc_->tree().locate(groupId);
  if (!loc) return;

  const LayerId parentId = loc->parent ? loc->parent->id : 0;
  const std::size_t groupIdx = loc->index;
  // Snapshot child ids (in stored order, child[0] = bottom of group) so
  // undo can re-collect them by id regardless of any subsequent reorders.
  std::vector<LayerId> childIds;
  childIds.reserve(g->children.size());
  for (const auto& c : g->children) childIds.push_back(c->id);

  auto stashGroup = std::make_shared<std::unique_ptr<LayerBase>>();

  auto resolveParent = [this](LayerId pid) -> GroupLayer* {
    if (pid == 0) return nullptr;
    return dynamic_cast<GroupLayer*>(doc_->tree().findById(pid));
  };

  auto doIt = [this, stashGroup, groupId, parentId, groupIdx, childIds,
               resolveParent]() mutable {
    auto loc2 = doc_->tree().locate(groupId);
    if (!loc2) return;
    auto group = doc_->tree().removeFromPath(loc2->parent, loc2->index);
    if (!group) return;
    auto* gPtr = static_cast<GroupLayer*>(group.get());

    // Promote children: child[i] lands at parent[groupIdx + i] so bottom-
    // up order matches the group's internal order. We iterate in order
    // and call insertAtPath which inserts BEFORE the index, so adjacent
    // inserts stack correctly.
    GroupLayer* parent = resolveParent(parentId);
    for (std::size_t i = 0; i < gPtr->children.size(); ++i) {
      doc_->tree().insertAtPath(parent, groupIdx + i,
                                 std::move(gPtr->children[i]));
    }
    gPtr->children.clear();
    *stashGroup = std::move(group);

    // Active = bottom-most ex-child. For an empty group, fall back to
    // whatever now occupies parent[groupIdx] (the layer that used to be
    // immediately above the group), or the parent itself, or none.
    if (!childIds.empty()) {
      doc_->setActiveLayerId(childIds.front());
    } else {
      LayerId fallback = 0;
      if (parent) {
        if (groupIdx < parent->children.size()) {
          fallback = parent->children[groupIdx]->id;
        } else if (!parent->children.empty()) {
          fallback = parent->children.back()->id;
        } else {
          fallback = parent->id;
        }
      } else {
        if (groupIdx < doc_->tree().size()) {
          fallback = doc_->tree().at(groupIdx)->id;
        } else if (!doc_->tree().empty()) {
          fallback = doc_->tree().at(doc_->tree().size() - 1)->id;
        }
      }
      doc_->setActiveLayerId(fallback);
    }
    refreshAfterUndoRedo();
  };
  auto undoIt = [this, stashGroup, groupId, parentId, groupIdx, childIds,
                 resolveParent]() mutable {
    if (!*stashGroup) return;
    // Pull each promoted child out by id and re-collect in original order.
    auto* gPtr = static_cast<GroupLayer*>(stashGroup->get());
    for (LayerId id : childIds) {
      auto loc2 = doc_->tree().locate(id);
      if (!loc2) continue;
      auto child = doc_->tree().removeFromPath(loc2->parent, loc2->index);
      if (child) gPtr->children.push_back(std::move(child));
    }
    auto group = std::move(*stashGroup);
    doc_->tree().insertAtPath(resolveParent(parentId), groupIdx,
                               std::move(group));
    doc_->setActiveLayerId(groupId);
    refreshAfterUndoRedo();
  };

  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>("Ungroup Layer",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
}

void MainWindow::updateGroupActionStates() {
  if (!groupLayerAction_ || !ungroupLayerAction_) return;
  const LayerBase* active = doc_ ? doc_->activeLayer() : nullptr;
  groupLayerAction_->setEnabled(active != nullptr);
  ungroupLayerAction_->setEnabled(active != nullptr &&
                                   active->kind() == LayerKind::Group);
}

MainWindow::AdjustmentInsertSlot MainWindow::computeAdjustmentInsertSlot(
    LayerId activeId) const {
  AdjustmentInsertSlot slot;
  if (!doc_) return slot;
  LayerBase* active = activeId ? doc_->tree().findById(activeId) : nullptr;
  if (!active) {
    slot.index = doc_->tree().size();
    return slot;
  }
  if (active->kind() == LayerKind::Group) {
    // Inside the group at the top of its children — adjustments affect
    // the group's existing contents.
    auto* g = static_cast<GroupLayer*>(active);
    slot.parentId = g->id;
    slot.index = g->children.size();
    return slot;
  }
  // Regular layer: same parent, above active.
  auto loc = doc_->tree().locate(activeId);
  if (!loc) {
    slot.index = doc_->tree().size();
    return slot;
  }
  slot.parentId = loc->parent ? loc->parent->id : 0;
  slot.index = loc->index + 1;
  return slot;
}

void MainWindow::onLayerAdd() {
  if (!doc_) return;
  const int n = static_cast<int>(doc_->tree().size()) + 1;
  const std::string name = "Layer " + std::to_string(n);
  const int w = doc_->width();
  const int h = doc_->height();
  const LayerId id = doc_->nextLayerId();
  const LayerId prevActiveId = doc_->activeLayerId();

  // Stash the layer ptr so that redo can rebuild its identity. We hold a
  // shared_ptr owned by the command; when it's "attached" to the tree we
  // transfer ownership in, and when detached (undone) we take it back.
  auto stash = std::make_shared<std::unique_ptr<LayerBase>>();

  auto doIt = [this, stash, name, w, h, id]() mutable {
    std::unique_ptr<LayerBase> layer;
    if (*stash) {
      layer = std::move(*stash);
    } else {
      auto px = std::make_unique<PixelLayer>(w, h);
      px->id = id;
      px->name = name;
      layer = std::move(px);
    }
    doc_->tree().add(std::move(layer));
    doc_->setActiveLayerId(id);
    refreshAfterUndoRedo();
  };
  auto undoIt = [this, stash, prevActiveId]() mutable {
    const std::size_t lastIdx = doc_->tree().size() - 1;
    *stash = doc_->tree().removeAt(lastIdx);
    doc_->setActiveLayerId(prevActiveId);
    refreshAfterUndoRedo();
  };

  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>("Add Layer",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
}

void MainWindow::onLayerDelete() {
  if (!doc_) return;
  const LayerId activeId = doc_->activeLayerId();
  if (activeId == 0) return;
  auto loc = doc_->tree().locate(activeId);
  if (!loc) return;

  // Capture by parent id so closures survive intervening reorders /
  // group changes. The unique_ptr ownership chain transitively destroys
  // any children when a group is the deleted layer; undo simply
  // reinstalls the stashed unique_ptr (group + children intact).
  const LayerId parentId = loc->parent ? loc->parent->id : 0;
  const std::size_t idx = loc->index;
  auto stash = std::make_shared<std::unique_ptr<LayerBase>>();

  auto resolveParent = [this](LayerId pid) -> GroupLayer* {
    if (pid == 0) return nullptr;
    return dynamic_cast<GroupLayer*>(doc_->tree().findById(pid));
  };

  auto doIt = [this, stash, parentId, idx, resolveParent]() mutable {
    GroupLayer* parent = resolveParent(parentId);
    *stash = doc_->tree().removeFromPath(parent, idx);
    if (!*stash) return;
    // Pick the layer that now occupies the deleted slot in the parent,
    // or fall back to the parent's last layer / the parent itself / none.
    LayerId newActive = 0;
    if (parent) {
      if (idx < parent->children.size()) {
        newActive = parent->children[idx]->id;
      } else if (!parent->children.empty()) {
        newActive = parent->children.back()->id;
      } else {
        newActive = parent->id;
      }
    } else {
      if (idx < doc_->tree().size()) {
        newActive = doc_->tree().at(idx)->id;
      } else if (!doc_->tree().empty()) {
        newActive = doc_->tree().at(doc_->tree().size() - 1)->id;
      }
    }
    doc_->setActiveLayerId(newActive);
    refreshAfterUndoRedo();
  };
  auto undoIt = [this, stash, parentId, idx, activeId, resolveParent]() mutable {
    if (!*stash) return;
    doc_->tree().insertAtPath(resolveParent(parentId), idx,
                               std::move(*stash));
    doc_->setActiveLayerId(activeId);
    refreshAfterUndoRedo();
  };

  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>("Delete Layer",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
}

void MainWindow::onLayerMoveUp() {
  if (!doc_) return;
  const LayerId activeId = doc_->activeLayerId();
  if (activeId == 0) return;
  auto loc = doc_->tree().locate(activeId);
  if (!loc) return;
  GroupLayer* parent = loc->parent;
  const std::size_t siblingCount = parent ? parent->children.size()
                                           : doc_->tree().size();
  if (loc->index + 1 >= siblingCount) {
    statusBar()->showMessage(
        parent ? tr("Already at top of group")
               : tr("Already at top of stack"),
        1500);
    return;
  }
  const std::size_t from = loc->index;
  const std::size_t to = from + 1;
  const LayerId parentId = parent ? parent->id : 0;

  auto resolveParent = [this](LayerId pid) -> GroupLayer* {
    if (pid == 0) return nullptr;
    return dynamic_cast<GroupLayer*>(doc_->tree().findById(pid));
  };

  auto doIt = [this, parentId, from, to, activeId, resolveParent]() {
    GroupLayer* p = resolveParent(parentId);
    doc_->tree().move(p, from, p, to);
    doc_->setActiveLayerId(activeId);
    refreshAfterUndoRedo();
  };
  auto undoIt = [this, parentId, from, to, activeId, resolveParent]() {
    GroupLayer* p = resolveParent(parentId);
    doc_->tree().move(p, to, p, from);
    doc_->setActiveLayerId(activeId);
    refreshAfterUndoRedo();
  };

  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>("Move Layer Up",
                                                    std::move(doIt),
                                                    std::move(undoIt)));
}

void MainWindow::onLayerMoveDown() {
  if (!doc_) return;
  const LayerId activeId = doc_->activeLayerId();
  if (activeId == 0) return;
  auto loc = doc_->tree().locate(activeId);
  if (!loc) return;
  GroupLayer* parent = loc->parent;
  if (loc->index == 0) {
    statusBar()->showMessage(
        parent ? tr("Already at bottom of group")
               : tr("Already at bottom of stack"),
        1500);
    return;
  }
  const std::size_t from = loc->index;
  const std::size_t to = from - 1;
  const LayerId parentId = parent ? parent->id : 0;

  auto resolveParent = [this](LayerId pid) -> GroupLayer* {
    if (pid == 0) return nullptr;
    return dynamic_cast<GroupLayer*>(doc_->tree().findById(pid));
  };

  auto doIt = [this, parentId, from, to, activeId, resolveParent]() {
    GroupLayer* p = resolveParent(parentId);
    doc_->tree().move(p, from, p, to);
    doc_->setActiveLayerId(activeId);
    refreshAfterUndoRedo();
  };
  auto undoIt = [this, parentId, from, to, activeId, resolveParent]() {
    GroupLayer* p = resolveParent(parentId);
    doc_->tree().move(p, to, p, from);
    doc_->setActiveLayerId(activeId);
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
  // M4-S2: when the active layer is an adjustment with a Properties pane,
  // load it into the dock automatically. Other kinds (or non-adjustments)
  // leave the dock at its empty state — explicit fx-thumb click on a
  // dialog-only adjustment kind still opens the modal.
  bindActiveAdjustmentToDock();
  // M5-S4: keep the Group / Ungroup menu actions in sync so their
  // keyboard shortcuts honor the current state without waiting for the
  // menu's `aboutToShow`.
  updateGroupActionStates();
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
  // Bucket fill commit. A press-only tool, same tile-COW undo pathway.
  if (bucketTool_) {
    auto fill = bucketTool_->takeLastFill();
    if (fill.layer && fill.target) {
      auto cmd = std::make_unique<PaintCommand>(
          fill.target, std::move(fill.recorded.before),
          std::move(fill.recorded.after), "Paint Bucket");
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
  // Magic wand commit — same selection-command plumbing as marquee.
  if (wandTool_) {
    if (auto commit = wandTool_->takeCommit()) {
      doc_->setSelection(commit->after ? commit->after->clone() : nullptr);
      undoStack_->push(std::make_unique<SelectionCommand>(
          doc_.get(), std::move(commit->before), std::move(commit->after),
          commit->label));
      if (canvas_) canvas_->refreshSelectionOverlay();
    }
  }
  // Lasso (freehand) commit — same plumbing; release closes the polygon
  // and hands us a before/after pair.
  if (lassoTool_) {
    if (auto commit = lassoTool_->takeCommit()) {
      doc_->setSelection(commit->after ? commit->after->clone() : nullptr);
      undoStack_->push(std::make_unique<SelectionCommand>(
          doc_.get(), std::move(commit->before), std::move(commit->after),
          commit->label));
      if (canvas_) canvas_->refreshSelectionOverlay();
    }
  }
  // Polygonal lasso commit — normally driven by Enter/Escape or click-on-
  // start-vertex, not by release; this path picks up the click-on-start
  // case (press builds the commit before layerPainted fires on release).
  if (polyLassoTool_) {
    if (auto commit = polyLassoTool_->takeCommit()) {
      doc_->setSelection(commit->after ? commit->after->clone() : nullptr);
      undoStack_->push(std::make_unique<SelectionCommand>(
          doc_.get(), std::move(commit->before), std::move(commit->after),
          commit->label));
      if (canvas_) canvas_->refreshSelectionOverlay();
    }
  }
  // Select-By-Color commit — same selection-command plumbing. The tool
  // commits on press (single-click gesture), so the commit is ready by the
  // time layerPainted fires.
  if (selectByColorTool_) {
    if (auto commit = selectByColorTool_->takeCommit()) {
      doc_->setSelection(commit->after ? commit->after->clone() : nullptr);
      undoStack_->push(std::make_unique<SelectionCommand>(
          doc_.get(), std::move(commit->before), std::move(commit->after),
          commit->label));
      if (canvas_) canvas_->refreshSelectionOverlay();
    }
  }
  // Crop commit. The CropCommand constructor snapshots the document, then
  // applies the crop in place; we only have to push it onto the stack and
  // refresh the UI. Dimensions change, so do a full recompose + rebuild the
  // ants overlay (the selection may have been cropped to null or shrunk).
  if (cropTool_) {
    if (auto commit = cropTool_->takeCommit()) {
      undoStack_->push(std::make_unique<CropCommand>(doc_.get(), commit->rect));
      if (canvas_) {
        canvas_->requestRecomposite();
        canvas_->refreshSelectionOverlay();
      }
      statusBar()->showMessage(tr("Cropped to %1×%2")
                                   .arg(commit->rect.w)
                                   .arg(commit->rect.h),
                               2000);
    }
  }
  // Move commit. The live drag already installed the new origin; we just
  // record the (before, after) pair so undo/redo can swap. apply() on the
  // command is idempotent against the already-set origin, so pushing it
  // here doesn't double-apply.
  if (moveTool_) {
    if (auto commit = moveTool_->takeCommit()) {
      undoStack_->push(std::make_unique<MoveLayerCommand>(
          doc_.get(), commit->layerId, commit->beforeX, commit->beforeY,
          commit->afterX, commit->afterY));
      statusBar()->showMessage(tr("Moved layer to (%1, %2)")
                                   .arg(commit->afterX)
                                   .arg(commit->afterY),
                               1500);
    }
  }
  if (layersPanel_) layersPanel_->refresh();
}

void MainWindow::setActiveTool(ToolId id) {
  if (id == activeToolId_) return;
  // Leaving the Transform tool auto-applies any pending transform so the
  // preview isn't silently discarded. User can press Escape first to
  // discard explicitly.
  if (activeToolId_ == ToolId::Transform && id != ToolId::Transform) {
    commitTransformIfActive();
  }
  // Leaving the Polygonal Lasso with a half-drawn polygon: discard it.
  // An Enter press just before tool-switch would have committed already;
  // this path handles the "user clicked another tool button mid-draw"
  // case. A silent discard is the lesser surprise vs. committing an
  // unclosed shape.
  if (activeToolId_ == ToolId::PolyLasso && id != ToolId::PolyLasso &&
      polyLassoTool_ && polyLassoTool_->isBuilding()) {
    polyLassoTool_->cancel();
    if (canvas_) canvas_->update();
  }
  activeToolId_ = id;
  switch (id) {
    case ToolId::Brush:
      if (canvas_) canvas_->setTool(brushTool_.get());
      break;
    case ToolId::Marquee:
      if (canvas_) canvas_->setTool(marqueeTool_.get());
      break;
    case ToolId::Bucket:
      if (canvas_) canvas_->setTool(bucketTool_.get());
      break;
    case ToolId::MagicWand:
      if (canvas_) canvas_->setTool(wandTool_.get());
      break;
    case ToolId::Crop:
      if (canvas_) canvas_->setTool(cropTool_.get());
      break;
    case ToolId::Move:
      if (canvas_) canvas_->setTool(moveTool_.get());
      break;
    case ToolId::Transform:
      if (canvas_) canvas_->setTool(transformTool_.get());
      break;
    case ToolId::Lasso:
      if (canvas_) canvas_->setTool(lassoTool_.get());
      break;
    case ToolId::PolyLasso:
      if (canvas_) canvas_->setTool(polyLassoTool_.get());
      break;
    case ToolId::SelectByColor:
      if (canvas_) canvas_->setTool(selectByColorTool_.get());
      break;
  }
  if (canvas_) canvas_->setToolCursor(CanvasView::cursorForTool(id));
  if (toolsPanel_) toolsPanel_->setActiveTool(id);
  if (canvas_) canvas_->refreshBrushCursor();
}

void MainWindow::onToolPicked(ToolId id) {
  // Transform is modal and needs `enter()` on the tool before it's usable;
  // route through the menu-equivalent handler so clicking the picker button
  // behaves like triggering Edit → Free Transform.
  if (id == ToolId::Transform) {
    onEditFreeTransform();
    return;
  }
  setActiveTool(id);
}

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
  // Mask matches the layer's own backing-image dims (post-M2-S0 masks share
  // their owning layer's origin + size). For legacy blank layers that equals
  // the doc dims; for layers placed off-origin it correctly stays layer-
  // sized instead of doc-sized.
  const int mw = px->image.width();
  const int mh = px->image.height();
  auto stash = std::make_shared<std::unique_ptr<LayerMask>>();

  auto doIt = [this, px, stash, mw, mh]() mutable {
    if (*stash) {
      px->mask = std::move(*stash);
    } else {
      auto m = std::make_unique<LayerMask>(mw, mh);
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
  doc_->setActiveLayerId(layer->id);
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

Histogram4x256 MainWindow::histogramBelow(LayerBase* layer) {
  if (!doc_ || !layer) return Histogram4x256{};
  // Build the set of `layer`'s ancestor groups so we don't accidentally
  // hide them — flatten() emits groups *after* their children (child-then-
  // self), so a target inside a group has its parent group at a later
  // flatten index. Hiding the parent group would skip the recursion into
  // the group's earlier children too, which would wrongly remove them
  // from the preview composite even though they're below the target in
  // composite order.
  std::set<const LayerBase*> ancestors;
  for (LayerId cur = layer->id; cur != 0;) {
    auto loc = doc_->tree().locate(cur);
    if (!loc || !loc->parent) break;
    ancestors.insert(loc->parent);
    cur = loc->parent->id;
  }

  // Walk the flattened tree (matches compose's iteration order) and hide
  // every layer at-or-after the target — except for ancestor groups,
  // which must stay visible so their earlier children compose normally.
  std::vector<LayerBase*> flat = doc_->tree().flatten();
  std::vector<bool> saved;
  saved.reserve(flat.size());
  bool pastLayer = false;
  for (LayerBase* l : flat) {
    saved.push_back(l->visible);
    if (l == layer) pastLayer = true;
    if (pastLayer && ancestors.count(l) == 0) l->visible = false;
  }
  TuxImage preview(doc_->width(), doc_->height());
  compose(doc_->tree(), preview);
  for (std::size_t i = 0; i < flat.size(); ++i) {
    flat[i]->visible = saved[i];
  }
  return computeHistogram(preview, doc_->selection());
}

void MainWindow::bindActiveAdjustmentToDock() {
  if (!propertiesDock_ || !doc_) return;
  LayerBase* layer = doc_->activeLayer();
  if (!layer) {
    propertiesDock_->bindNothing();
    return;
  }
  // M6-S0: groups bind to their own properties pane (name + blend +
  // opacity + clip-to-below). Static cast is safe because `kind() == Group`
  // implies the runtime type.
  if (layer->kind() == LayerKind::Group) {
    propertiesDock_->bindGroup(static_cast<GroupLayer*>(layer));
    return;
  }
  if (auto* levels = dynamic_cast<LevelsAdjustment*>(layer)) {
    propertiesDock_->bindLevels(levels, histogramBelow(levels));
    return;
  }
  if (auto* curves = dynamic_cast<CurvesAdjustment*>(layer)) {
    propertiesDock_->bindCurves(curves, histogramBelow(curves));
    return;
  }
  if (auto* hs = dynamic_cast<HueSaturation*>(layer)) {
    propertiesDock_->bindHueSat(hs);
    return;
  }
  if (auto* bc = dynamic_cast<BrightnessContrast*>(layer)) {
    propertiesDock_->bindBrightnessContrast(bc);
    return;
  }
  // No adjustment kind matched — empty state.
  propertiesDock_->bindNothing();
}

void MainWindow::onLayerAddLevels() {
  if (!doc_) return;

  // Histogram of the composite below the new adjustment's insert position.
  // Compute BEFORE adding the layer so the histogram reflects "what's
  // about to be adjusted" — matches PS.
  TuxImage preview(doc_->width(), doc_->height());
  compose(doc_->tree(), preview);
  Histogram4x256 hist = computeHistogram(preview, doc_->selection());

  auto layer = std::make_unique<LevelsAdjustment>();
  layer->name = "Levels";
  const PaintTarget prevPaintTarget = doc_->paintTarget();
  const LayerId prevActiveId = doc_->activeLayerId();

  // Insert with identity params; bind the dock so the user can adjust
  // immediately. No more modal Cancel — Ctrl+Z removes the layer if the
  // user changes their mind.
  LevelsAdjustment* raw = doc_->addAdjustmentLayer(std::move(layer));
  const LayerId addedId = raw->id;
  // M5-S5: compute the target slot from the prev active layer (active was
  // bumped to the new adjustment by addAdjustmentLayer). This routes the
  // adjustment into the active group / above the active layer rather than
  // always landing at root top.
  const auto slot = computeAdjustmentInsertSlot(prevActiveId);
  // Pull the just-added layer out of root so the closure can reinstall it
  // at the target slot.
  auto stash = std::make_shared<std::unique_ptr<LayerBase>>(
      doc_->tree().removeAt(doc_->tree().size() - 1));

  auto resolveParent = [this](LayerId pid) -> GroupLayer* {
    if (pid == 0) return nullptr;
    return dynamic_cast<GroupLayer*>(doc_->tree().findById(pid));
  };

  auto doIt = [this, stash, slot, addedId, resolveParent]() mutable {
    if (!*stash) return;
    doc_->tree().insertAtPath(resolveParent(slot.parentId), slot.index,
                               std::move(*stash));
    doc_->setActiveLayerId(addedId);
    doc_->setPaintTarget(PaintTarget::Mask);
    refreshAfterUndoRedo();
    bindActiveAdjustmentToDock();
  };
  auto undoIt = [this, stash, slot, prevActiveId, prevPaintTarget,
                 resolveParent]() mutable {
    *stash = doc_->tree().removeFromPath(resolveParent(slot.parentId),
                                          slot.index);
    doc_->setActiveLayerId(prevActiveId);
    doc_->setPaintTarget(prevPaintTarget);
    refreshAfterUndoRedo();
    bindActiveAdjustmentToDock();
  };

  doIt();
  // raw is back in the tree after doIt(); push the command + bind dock.
  undoStack_->push(std::make_unique<LayerOpCommand>(
      "Add Levels Adjustment", std::move(doIt), std::move(undoIt)));
  propertiesDock_->bindLevels(raw, hist);
  propertiesDock_->raise();
}

void MainWindow::onLayerAddCurves() {
  if (!doc_) return;

  TuxImage preview(doc_->width(), doc_->height());
  compose(doc_->tree(), preview);
  Histogram4x256 hist = computeHistogram(preview, doc_->selection());

  auto layer = std::make_unique<CurvesAdjustment>();
  layer->name = "Curves";
  const PaintTarget prevPaintTarget = doc_->paintTarget();
  const LayerId prevActiveId = doc_->activeLayerId();

  CurvesAdjustment* raw = doc_->addAdjustmentLayer(std::move(layer));
  const LayerId addedId = raw->id;
  const auto slot = computeAdjustmentInsertSlot(prevActiveId);
  auto stash = std::make_shared<std::unique_ptr<LayerBase>>(
      doc_->tree().removeAt(doc_->tree().size() - 1));

  auto resolveParent = [this](LayerId pid) -> GroupLayer* {
    if (pid == 0) return nullptr;
    return dynamic_cast<GroupLayer*>(doc_->tree().findById(pid));
  };

  auto doIt = [this, stash, slot, addedId, resolveParent]() mutable {
    if (!*stash) return;
    doc_->tree().insertAtPath(resolveParent(slot.parentId), slot.index,
                               std::move(*stash));
    doc_->setActiveLayerId(addedId);
    doc_->setPaintTarget(PaintTarget::Mask);
    refreshAfterUndoRedo();
    bindActiveAdjustmentToDock();
  };
  auto undoIt = [this, stash, slot, prevActiveId, prevPaintTarget,
                 resolveParent]() mutable {
    *stash = doc_->tree().removeFromPath(resolveParent(slot.parentId),
                                          slot.index);
    doc_->setActiveLayerId(prevActiveId);
    doc_->setPaintTarget(prevPaintTarget);
    refreshAfterUndoRedo();
    bindActiveAdjustmentToDock();
  };

  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>(
      "Add Curves Adjustment", std::move(doIt), std::move(undoIt)));
  propertiesDock_->bindCurves(raw, hist);
  propertiesDock_->raise();
}

void MainWindow::onLayerAddBrightnessContrast() {
  if (!doc_) return;

  auto layer = std::make_unique<BrightnessContrast>();
  layer->name = "Brightness/Contrast";
  const PaintTarget prevPaintTarget = doc_->paintTarget();
  const LayerId prevActiveId = doc_->activeLayerId();

  BrightnessContrast* raw = doc_->addAdjustmentLayer(std::move(layer));
  const LayerId addedId = raw->id;
  const auto slot = computeAdjustmentInsertSlot(prevActiveId);
  auto stash = std::make_shared<std::unique_ptr<LayerBase>>(
      doc_->tree().removeAt(doc_->tree().size() - 1));

  auto resolveParent = [this](LayerId pid) -> GroupLayer* {
    if (pid == 0) return nullptr;
    return dynamic_cast<GroupLayer*>(doc_->tree().findById(pid));
  };

  auto doIt = [this, stash, slot, addedId, resolveParent]() mutable {
    if (!*stash) return;
    doc_->tree().insertAtPath(resolveParent(slot.parentId), slot.index,
                               std::move(*stash));
    doc_->setActiveLayerId(addedId);
    doc_->setPaintTarget(PaintTarget::Mask);
    refreshAfterUndoRedo();
    bindActiveAdjustmentToDock();
  };
  auto undoIt = [this, stash, slot, prevActiveId, prevPaintTarget,
                 resolveParent]() mutable {
    *stash = doc_->tree().removeFromPath(resolveParent(slot.parentId),
                                          slot.index);
    doc_->setActiveLayerId(prevActiveId);
    doc_->setPaintTarget(prevPaintTarget);
    refreshAfterUndoRedo();
    bindActiveAdjustmentToDock();
  };

  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>(
      "Add Brightness/Contrast Adjustment", std::move(doIt),
      std::move(undoIt)));
  propertiesDock_->bindBrightnessContrast(raw);
  propertiesDock_->raise();
}

void MainWindow::onLayerAddHueSaturation() {
  if (!doc_) return;

  auto layer = std::make_unique<HueSaturation>();
  layer->name = "Hue/Saturation";
  const PaintTarget prevPaintTarget = doc_->paintTarget();
  const LayerId prevActiveId = doc_->activeLayerId();

  HueSaturation* raw = doc_->addAdjustmentLayer(std::move(layer));
  const LayerId addedId = raw->id;
  const auto slot = computeAdjustmentInsertSlot(prevActiveId);
  auto stash = std::make_shared<std::unique_ptr<LayerBase>>(
      doc_->tree().removeAt(doc_->tree().size() - 1));

  auto resolveParent = [this](LayerId pid) -> GroupLayer* {
    if (pid == 0) return nullptr;
    return dynamic_cast<GroupLayer*>(doc_->tree().findById(pid));
  };

  auto doIt = [this, stash, slot, addedId, resolveParent]() mutable {
    if (!*stash) return;
    doc_->tree().insertAtPath(resolveParent(slot.parentId), slot.index,
                               std::move(*stash));
    doc_->setActiveLayerId(addedId);
    doc_->setPaintTarget(PaintTarget::Mask);
    refreshAfterUndoRedo();
    bindActiveAdjustmentToDock();
  };
  auto undoIt = [this, stash, slot, prevActiveId, prevPaintTarget,
                 resolveParent]() mutable {
    *stash = doc_->tree().removeFromPath(resolveParent(slot.parentId),
                                          slot.index);
    doc_->setActiveLayerId(prevActiveId);
    doc_->setPaintTarget(prevPaintTarget);
    refreshAfterUndoRedo();
    bindActiveAdjustmentToDock();
  };

  doIt();
  undoStack_->push(std::make_unique<LayerOpCommand>(
      "Add Hue/Saturation Adjustment", std::move(doIt), std::move(undoIt)));
  propertiesDock_->bindHueSat(raw);
  propertiesDock_->raise();
}

void MainWindow::onEditAdjustmentRequested(LayerBase* layer) {
  if (!doc_ || !layer) return;

  if (auto* levels = dynamic_cast<LevelsAdjustment*>(layer)) {
    // Properties dock takes the modal's place (M4-S2). bindLevels triggers
    // a snapshotBefore inside the pane, so the next slider release will
    // commit a `LayerParamsCommand` against the on-bind state.
    propertiesDock_->bindLevels(levels, histogramBelow(levels));
    propertiesDock_->raise();
    return;
  }

  if (auto* curves = dynamic_cast<CurvesAdjustment*>(layer)) {
    propertiesDock_->bindCurves(curves, histogramBelow(curves));
    propertiesDock_->raise();
    return;
  }

  if (auto* hs = dynamic_cast<HueSaturation*>(layer)) {
    propertiesDock_->bindHueSat(hs);
    propertiesDock_->raise();
    return;
  }

  if (auto* bc = dynamic_cast<BrightnessContrast*>(layer)) {
    propertiesDock_->bindBrightnessContrast(bc);
    propertiesDock_->raise();
    return;
  }
}

void MainWindow::onToggleClipToBelow() {
  if (!doc_) return;
  if (LayerBase* l = doc_->activeLayer()) onLayerToggleClipToBelow(l);
}

void MainWindow::onLayerToggleClipToBelow(LayerBase* layer) {
  if (!doc_ || !layer) return;
  // Bottom layer has no base to clip to — leave as-is. PS greys out the
  // menu item in this case; we just no-op.
  if (!doc_->tree().empty() && doc_->tree().at(0) == layer) {
    statusBar()->showMessage(
        tr("Bottom layer can't be clipped — needs a base layer below."),
        2000);
    return;
  }
  const LayerId id = layer->id;
  const bool wasClipped = layer->clipToBelow;
  layer->clipToBelow = !wasClipped;
  layersPanel_->refresh();
  canvas_->requestRecomposite();
  auto doIt = [this, id, newVal = !wasClipped]() {
    if (auto* l = doc_->tree().findById(id)) {
      l->clipToBelow = newVal;
      layersPanel_->refresh();
      canvas_->requestRecomposite();
    }
  };
  auto undoIt = [this, id, oldVal = wasClipped]() {
    if (auto* l = doc_->tree().findById(id)) {
      l->clipToBelow = oldVal;
      layersPanel_->refresh();
      canvas_->requestRecomposite();
    }
  };
  const std::string label =
      wasClipped ? "Release Clipping Mask" : "Create Clipping Mask";
  undoStack_->push(std::make_unique<LayerOpCommand>(label, std::move(doIt),
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

void MainWindow::onEditFreeTransform() {
  if (!doc_ || !transformTool_) return;
  if (transformTool_->isActive()) return;  // already transforming
  if (!transformTool_->enter(*doc_)) {
    statusBar()->showMessage(
        tr("Free Transform needs an active pixel layer."), 3000);
    return;
  }
  setActiveTool(ToolId::Transform);
  if (canvas_) canvas_->requestRecomposite();
  statusBar()->showMessage(
      tr("Free Transform — drag to scale/rotate/move, Enter to commit, "
         "Esc to cancel"),
      4000);
}

bool MainWindow::commitTransformIfActive() {
  if (!transformTool_ || !transformTool_->isActive()) return false;
  auto p = transformTool_->commit();
  if (!p) return false;  // identity → nothing to push
  // Unlike the Move tool, the Transform tool's live preview is a compose
  // override — the real layer is untouched during the drag. Apply the
  // command's side-effect *before* pushing so UndoStack's "state already
  // matches after-commit" invariant holds.
  auto cmd = std::make_unique<TransformCommand>(
      doc_.get(), p->layerId, std::move(p->before), std::move(p->after),
      p->beforeX, p->beforeY, p->afterX, p->afterY);
  cmd->apply();
  undoStack_->push(std::move(cmd));
  if (canvas_) canvas_->requestRecomposite();
  if (layersPanel_) layersPanel_->refresh();
  return true;
}

void MainWindow::onTransformAccept() {
  // Enter key is shared between Transform commit and Polygonal Lasso
  // close — neither steals it from QInputDialogs because those capture
  // the key before this window-scope QAction sees it. Route by tool
  // state: Transform has priority (it's modal), then Polygonal Lasso.
  if (polyLassoTool_ && polyLassoTool_->isBuilding() && doc_ &&
      (!transformTool_ || !transformTool_->isActive())) {
    polyLassoTool_->finish(*doc_);
    onLayerPainted();
    if (canvas_) canvas_->update();
    return;
  }
  if (!transformTool_ || !transformTool_->isActive()) return;
  const bool pushed = commitTransformIfActive();
  // Drop out of the modal tool regardless — even an identity Enter should
  // exit Free Transform and return the user to a normal editing tool.
  if (activeToolId_ == ToolId::Transform) {
    activeToolId_ = ToolId::Brush;  // avoid setActiveTool's auto-commit path
    if (canvas_) canvas_->setTool(brushTool_.get());
    if (canvas_) canvas_->setToolCursor(CanvasView::cursorForTool(ToolId::Brush));
    if (toolsPanel_) toolsPanel_->setActiveTool(ToolId::Brush);
    if (canvas_) canvas_->refreshBrushCursor();
  }
  if (canvas_) canvas_->requestRecomposite();
  statusBar()->showMessage(
      pushed ? tr("Transform applied") : tr("Transform — nothing to apply"),
      1500);
}

void MainWindow::onTransformCancel() {
  // Escape shares the same key-routing logic as Enter: Transform first,
  // then Polygonal Lasso. A Lasso cancel discards the in-progress
  // polygon without touching the document selection.
  if (polyLassoTool_ && polyLassoTool_->isBuilding() &&
      (!transformTool_ || !transformTool_->isActive())) {
    polyLassoTool_->cancel();
    if (canvas_) canvas_->update();
    statusBar()->showMessage(tr("Polygonal lasso cancelled"), 1500);
    return;
  }
  if (!transformTool_ || !transformTool_->isActive()) return;
  transformTool_->cancel();
  if (activeToolId_ == ToolId::Transform) {
    activeToolId_ = ToolId::Brush;  // avoid setActiveTool's auto-commit path
    if (canvas_) canvas_->setTool(brushTool_.get());
    if (canvas_) canvas_->setToolCursor(CanvasView::cursorForTool(ToolId::Brush));
    if (toolsPanel_) toolsPanel_->setActiveTool(ToolId::Brush);
    if (canvas_) canvas_->refreshBrushCursor();
  }
  if (canvas_) canvas_->requestRecomposite();
  statusBar()->showMessage(tr("Transform cancelled"), 1500);
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
