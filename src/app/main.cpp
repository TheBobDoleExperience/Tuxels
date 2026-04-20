#include "app/MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  app.setApplicationName("Tuxels");
  app.setApplicationDisplayName("Tuxels");
  app.setOrganizationName("Tuxels");
  app.setApplicationVersion("0.0.1");

  // Multi-resolution app icon sourced from the embedded .qrc. Qt picks the
  // closest size for window decorations, taskbars, and the Wayland/X11
  // WM_CLASS icon hint — shipping several avoids upscale blur on HiDPI.
  QIcon icon;
  for (int s : {16, 32, 48, 64, 128, 256, 512}) {
    icon.addFile(QStringLiteral(":/icons/tuxels-%1.png").arg(s),
                 QSize(s, s));
  }
  QApplication::setWindowIcon(icon);

  tuxels::MainWindow window;
  window.show();
  return app.exec();
}
