#include "app/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

#include "core/Document.h"
#include "layers/PixelLayer.h"
#include "ui/CanvasView.h"
#include "ui/LayersPanel.h"

namespace tuxels {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("Tuxels");
  resize(1400, 900);

  canvas_ = new CanvasView(this);
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

  mb->addMenu(tr("&Edit"));
  mb->addMenu(tr("&Image"));

  auto* layerMenu = mb->addMenu(tr("&Layer"));
  auto* addLayerAct = layerMenu->addAction(tr("&New Layer"));
  addLayerAct->setShortcut(QKeySequence(tr("Ctrl+Shift+N")));
  connect(addLayerAct, &QAction::triggered, this, &MainWindow::onLayerAdd);

  auto* delLayerAct = layerMenu->addAction(tr("&Delete Layer"));
  connect(delLayerAct, &QAction::triggered, this, &MainWindow::onLayerDelete);

  mb->addMenu(tr("&Select"));
  mb->addMenu(tr("&Filter"));
  mb->addMenu(tr("&View"));
  mb->addMenu(tr("&Window"));
  mb->addMenu(tr("&Help"));
}

void MainWindow::buildDocks() {
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
}

void MainWindow::setDocument(std::unique_ptr<Document> doc) {
  doc_ = std::move(doc);
  canvas_->setDocument(doc_.get());
  layersPanel_->setDocument(doc_.get());
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
  QMessageBox::information(this, tr("Open"),
                           tr("PNG Open is wired in the next step (S6)."));
}

void MainWindow::onFileExport() {
  QMessageBox::information(this, tr("Export"),
                           tr("PNG Export is wired in the next step (S6)."));
}

void MainWindow::onLayerAdd() {
  if (!doc_) return;
  const int n = static_cast<int>(doc_->tree().size()) + 1;
  doc_->addBlankPixelLayer("Layer " + std::to_string(n));
  layersPanel_->refresh();
  canvas_->requestRecomposite();
}

void MainWindow::onLayerDelete() {
  if (!doc_) return;
  int i = doc_->activeLayerIndex();
  if (i < 0 || static_cast<std::size_t>(i) >= doc_->tree().size()) return;
  doc_->tree().removeAt(static_cast<std::size_t>(i));
  doc_->setActiveLayerIndex(i - 1);
  layersPanel_->refresh();
  canvas_->requestRecomposite();
}

void MainWindow::onLayerMoveUp() {
  if (!doc_) return;
  int i = doc_->activeLayerIndex();
  if (i < 0 || i + 1 >= static_cast<int>(doc_->tree().size())) return;
  doc_->tree().move(static_cast<std::size_t>(i), static_cast<std::size_t>(i + 1));
  doc_->setActiveLayerIndex(i + 1);
  layersPanel_->refresh();
  canvas_->requestRecomposite();
}

void MainWindow::onLayerMoveDown() {
  if (!doc_) return;
  int i = doc_->activeLayerIndex();
  if (i <= 0) return;
  doc_->tree().move(static_cast<std::size_t>(i), static_cast<std::size_t>(i - 1));
  doc_->setActiveLayerIndex(i - 1);
  layersPanel_->refresh();
  canvas_->requestRecomposite();
}

void MainWindow::onLayerPanelMutated() {
  if (canvas_) canvas_->requestRecomposite();
}

void MainWindow::onActiveLayerChanged() {
  // Placeholder for future painting-target updates.
}

}  // namespace tuxels
