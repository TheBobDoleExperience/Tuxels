#pragma once

#include <QMainWindow>

namespace tuxels {

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 private:
  void buildMenus();
};

}  // namespace tuxels
