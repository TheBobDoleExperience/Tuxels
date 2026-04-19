#include "app/MainWindow.h"

#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>

namespace tuxels {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("Tuxels");
  resize(1280, 800);

  auto* placeholder = new QLabel(tr("Canvas placeholder — layers & tools coming online."), this);
  placeholder->setAlignment(Qt::AlignCenter);
  setCentralWidget(placeholder);

  buildMenus();
  statusBar()->showMessage(tr("Ready"));
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenus() {
  auto* mb = menuBar();
  mb->addMenu(tr("&File"));
  mb->addMenu(tr("&Edit"));
  mb->addMenu(tr("&Image"));
  mb->addMenu(tr("&Layer"));
  mb->addMenu(tr("&Select"));
  mb->addMenu(tr("&Filter"));
  mb->addMenu(tr("&View"));
  mb->addMenu(tr("&Window"));
  mb->addMenu(tr("&Help"));
}

}  // namespace tuxels
