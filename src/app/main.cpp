#include "app/MainWindow.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  app.setApplicationName("Tuxels");
  app.setApplicationDisplayName("Tuxels");
  app.setOrganizationName("Tuxels");
  app.setApplicationVersion("0.0.1");

  tuxels::MainWindow window;
  window.show();
  return app.exec();
}
