#include "ui/ToolsPanel.h"

#include <QButtonGroup>
#include <QColorDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>

#include "brush/RoundBrush.h"
#include "tools/BrushTool.h"

namespace tuxels {

namespace {

// A clickable color square. No Q_OBJECT → no MOC; uses a std::function
// callback instead of a signal, which keeps this fully self-contained in
// the .cpp and off AutoMOC's radar.
class Swatch : public QFrame {
 public:
  explicit Swatch(QWidget* parent = nullptr) : QFrame(parent) {
    setFrameShape(QFrame::Box);
    setLineWidth(1);
    setFixedSize(28, 28);
    setCursor(Qt::PointingHandCursor);
    setAutoFillBackground(true);
  }

  void setColor(const QColor& c) {
    QPalette pal = palette();
    pal.setColor(QPalette::Window, c);
    setPalette(pal);
  }

  void setOnClick(std::function<void()> cb) { cb_ = std::move(cb); }

 protected:
  void mousePressEvent(QMouseEvent* e) override {
    if (e->button() == Qt::LeftButton && cb_) cb_();
    QFrame::mousePressEvent(e);
  }

 private:
  std::function<void()> cb_;
};

Swatch* asSwatch(QFrame* f) { return static_cast<Swatch*>(f); }

}  // namespace

ToolsPanel::ToolsPanel(QWidget* parent) : QDockWidget(tr("Tools"), parent) {
  setObjectName("ToolsPanel");
  setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

  auto* root = new QWidget(this);
  auto* vbox = new QVBoxLayout(root);
  vbox->setContentsMargins(8, 8, 8, 8);
  vbox->setSpacing(8);

  // -- Tool picker --------------------------------------------------------
  auto* pickerRow = new QWidget(root);
  auto* pickerLayout = new QHBoxLayout(pickerRow);
  pickerLayout->setContentsMargins(0, 0, 0, 0);
  pickerLayout->setSpacing(4);
  auto* pickerGroup = new QButtonGroup(this);
  pickerGroup->setExclusive(true);

  pickBrushBtn_ = new QToolButton(pickerRow);
  pickBrushBtn_->setText("B");
  pickBrushBtn_->setToolTip(tr("Brush  (B)"));
  pickBrushBtn_->setCheckable(true);
  pickBrushBtn_->setChecked(true);
  pickerGroup->addButton(pickBrushBtn_);
  connect(pickBrushBtn_, &QToolButton::clicked, this,
          [this]() { emit toolPicked(ToolId::Brush); });

  pickMarqueeBtn_ = new QToolButton(pickerRow);
  pickMarqueeBtn_->setText("M");
  pickMarqueeBtn_->setToolTip(tr("Rectangular Marquee  (M)"));
  pickMarqueeBtn_->setCheckable(true);
  pickerGroup->addButton(pickMarqueeBtn_);
  connect(pickMarqueeBtn_, &QToolButton::clicked, this,
          [this]() { emit toolPicked(ToolId::Marquee); });

  pickerLayout->addWidget(pickBrushBtn_);
  pickerLayout->addWidget(pickMarqueeBtn_);
  pickerLayout->addStretch(1);
  vbox->addWidget(pickerRow);

  // -- Marquee options (visible only when Marquee is active) -------------
  // A persistent-mode row (New/Add/Subtract/Intersect) so Subtract and
  // Intersect stay reachable on window managers that grab Alt-drag for
  // window-move (GNOME default).
  marqueeGroup_ = new QWidget(root);
  auto* marqueeVbox = new QVBoxLayout(marqueeGroup_);
  marqueeVbox->setContentsMargins(0, 0, 0, 0);
  marqueeVbox->setSpacing(4);
  auto* marqueeHeader = new QLabel(tr("<b>Marquee</b>"), marqueeGroup_);
  marqueeVbox->addWidget(marqueeHeader);
  auto* modeRow = new QWidget(marqueeGroup_);
  auto* modeLayout = new QHBoxLayout(modeRow);
  modeLayout->setContentsMargins(0, 0, 0, 0);
  modeLayout->setSpacing(4);
  auto* modeGroup = new QButtonGroup(this);
  modeGroup->setExclusive(true);
  auto makeModeBtn = [&](const QString& text, const QString& tip,
                         SelectionMode m, bool checked) {
    auto* b = new QToolButton(modeRow);
    b->setText(text);
    b->setToolTip(tip);
    b->setCheckable(true);
    b->setChecked(checked);
    modeGroup->addButton(b);
    modeLayout->addWidget(b);
    connect(b, &QToolButton::clicked, this,
            [this, m]() { emit marqueeModeChanged(m); });
    return b;
  };
  marqueeReplaceBtn_ =
      makeModeBtn("New", tr("Replace selection  (no modifier)"),
                  SelectionMode::Replace, true);
  marqueeAddBtn_ =
      makeModeBtn("+", tr("Add to selection  (Shift)"), SelectionMode::Add,
                  false);
  marqueeSubtractBtn_ =
      makeModeBtn("−", tr("Subtract from selection  (Alt)"),
                  SelectionMode::Subtract, false);
  marqueeIntersectBtn_ =
      makeModeBtn("∩", tr("Intersect with selection  (Shift+Alt)"),
                  SelectionMode::Intersect, false);
  modeLayout->addStretch(1);
  marqueeVbox->addWidget(modeRow);
  marqueeGroup_->setVisible(false);
  vbox->addWidget(marqueeGroup_);

  // -- Brush group (hidden when non-brush tools are active) ---------------
  brushGroup_ = new QWidget(root);
  auto* brushVbox = new QVBoxLayout(brushGroup_);
  brushVbox->setContentsMargins(0, 0, 0, 0);
  brushVbox->setSpacing(8);

  // -- Color swatches -----------------------------------------------------
  auto* colorGroup = new QWidget(brushGroup_);
  auto* colorLayout = new QHBoxLayout(colorGroup);
  colorLayout->setContentsMargins(0, 0, 0, 0);
  colorLayout->setSpacing(4);

  auto* fg = new Swatch(colorGroup);
  fg->setToolTip(tr("Foreground color — click to pick"));
  fg->setOnClick([this]() { onFgSwatchClicked(); });
  fgSwatch_ = fg;

  auto* bg = new Swatch(colorGroup);
  bg->setToolTip(tr("Background color — click to pick"));
  bg->setOnClick([this]() { onBgSwatchClicked(); });
  bgSwatch_ = bg;

  swapBtn_ = new QToolButton(colorGroup);
  swapBtn_->setText("X");
  swapBtn_->setToolTip(tr("Swap foreground/background  (X)"));
  connect(swapBtn_, &QToolButton::clicked, this, &ToolsPanel::swapColors);

  resetBtn_ = new QToolButton(colorGroup);
  resetBtn_->setText("D");
  resetBtn_->setToolTip(tr("Default colors — black/white  (D)"));
  connect(resetBtn_, &QToolButton::clicked, this, &ToolsPanel::resetColors);

  colorLayout->addWidget(fg);
  colorLayout->addWidget(bg);
  colorLayout->addSpacing(8);
  colorLayout->addWidget(swapBtn_);
  colorLayout->addWidget(resetBtn_);
  colorLayout->addStretch(1);
  brushVbox->addWidget(colorGroup);

  // -- Brush parameters ---------------------------------------------------
  auto* brushHeader = new QLabel(tr("<b>Brush</b>"), brushGroup_);
  brushVbox->addWidget(brushHeader);

  auto* form = new QFormLayout();
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(4);

  auto* sizeRow = new QWidget(brushGroup_);
  auto* sizeLayout = new QHBoxLayout(sizeRow);
  sizeLayout->setContentsMargins(0, 0, 0, 0);
  sizeSlider_ = new QSlider(Qt::Horizontal, sizeRow);
  sizeSlider_->setRange(1, 500);
  sizeSlider_->setSingleStep(1);
  sizeSpin_ = new QSpinBox(sizeRow);
  sizeSpin_->setRange(1, 2048);
  sizeSpin_->setSuffix(" px");
  sizeSpin_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  sizeSpin_->setFixedWidth(72);
  sizeLayout->addWidget(sizeSlider_, 1);
  sizeLayout->addWidget(sizeSpin_);
  connect(sizeSlider_, &QSlider::valueChanged, this, &ToolsPanel::onSizeChanged);
  connect(sizeSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &ToolsPanel::onSizeChanged);
  form->addRow(tr("Size"), sizeRow);

  auto makePctRow = [&](QSlider*& slider, QLabel*& label) {
    auto* row = new QWidget(brushGroup_);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(0, 100);
    slider->setSingleStep(1);
    label = new QLabel("100%", row);
    label->setFixedWidth(42);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(slider, 1);
    layout->addWidget(label);
    return row;
  };

  auto* hardnessRow = makePctRow(hardnessSlider_, hardnessLabel_);
  connect(hardnessSlider_, &QSlider::valueChanged, this,
          &ToolsPanel::onHardnessChanged);
  form->addRow(tr("Hardness"), hardnessRow);

  auto* opacityRow = makePctRow(opacitySlider_, opacityLabel_);
  connect(opacitySlider_, &QSlider::valueChanged, this,
          &ToolsPanel::onOpacityChanged);
  form->addRow(tr("Opacity"), opacityRow);

  auto* flowRow = makePctRow(flowSlider_, flowLabel_);
  connect(flowSlider_, &QSlider::valueChanged, this,
          &ToolsPanel::onFlowChanged);
  form->addRow(tr("Flow"), flowRow);

  brushVbox->addLayout(form);
  vbox->addWidget(brushGroup_);
  vbox->addStretch(1);
  setWidget(root);

  updateSwatchColors();
}

void ToolsPanel::setActiveTool(ToolId id) {
  if (pickBrushBtn_) pickBrushBtn_->setChecked(id == ToolId::Brush);
  if (pickMarqueeBtn_) pickMarqueeBtn_->setChecked(id == ToolId::Marquee);
  if (brushGroup_) brushGroup_->setVisible(id == ToolId::Brush);
  if (marqueeGroup_) marqueeGroup_->setVisible(id == ToolId::Marquee);
}

void ToolsPanel::setMarqueeMode(SelectionMode m) {
  // Only `clicked` is wired (not `toggled`), so setChecked is silent.
  if (marqueeReplaceBtn_)   marqueeReplaceBtn_->setChecked(m == SelectionMode::Replace);
  if (marqueeAddBtn_)       marqueeAddBtn_->setChecked(m == SelectionMode::Add);
  if (marqueeSubtractBtn_)  marqueeSubtractBtn_->setChecked(m == SelectionMode::Subtract);
  if (marqueeIntersectBtn_) marqueeIntersectBtn_->setChecked(m == SelectionMode::Intersect);
}

void ToolsPanel::setBrushTool(BrushTool* tool) {
  brush_ = tool;
  refreshFromBrush();
  applyFgToBrush();
}

void ToolsPanel::refreshFromBrush() {
  if (!brush_) return;
  suppressEdits_ = true;
  const auto& p = brush_->brush().params();
  sizeSlider_->setValue(std::min(p.diameter, sizeSlider_->maximum()));
  sizeSpin_->setValue(p.diameter);
  const int hPct = static_cast<int>(std::lround(p.hardness * 100.f));
  const int oPct = static_cast<int>(std::lround(p.opacity * 100.f));
  const int fPct = static_cast<int>(std::lround(p.flow * 100.f));
  hardnessSlider_->setValue(hPct);
  hardnessLabel_->setText(QStringLiteral("%1%").arg(hPct));
  opacitySlider_->setValue(oPct);
  opacityLabel_->setText(QStringLiteral("%1%").arg(oPct));
  flowSlider_->setValue(fPct);
  flowLabel_->setText(QStringLiteral("%1%").arg(fPct));
  suppressEdits_ = false;
}

void ToolsPanel::onFgSwatchClicked() {
  QColor chosen = QColorDialog::getColor(
      fg_, this, tr("Pick foreground color"),
      QColorDialog::ShowAlphaChannel);
  if (!chosen.isValid()) return;
  fg_ = chosen;
  updateSwatchColors();
  applyFgToBrush();
}

void ToolsPanel::onBgSwatchClicked() {
  QColor chosen = QColorDialog::getColor(
      bg_, this, tr("Pick background color"),
      QColorDialog::ShowAlphaChannel);
  if (!chosen.isValid()) return;
  bg_ = chosen;
  updateSwatchColors();
}

void ToolsPanel::swapColors() {
  std::swap(fg_, bg_);
  updateSwatchColors();
  applyFgToBrush();
}

void ToolsPanel::resetColors() {
  fg_ = Qt::black;
  bg_ = Qt::white;
  updateSwatchColors();
  applyFgToBrush();
}

void ToolsPanel::onSizeChanged(int v) {
  if (suppressEdits_ || !brush_) return;
  suppressEdits_ = true;
  sizeSlider_->setValue(std::min(v, sizeSlider_->maximum()));
  sizeSpin_->setValue(v);
  suppressEdits_ = false;
  brush_->brush().setDiameter(v);
}

void ToolsPanel::onHardnessChanged(int v) {
  if (suppressEdits_ || !brush_) return;
  hardnessLabel_->setText(QStringLiteral("%1%").arg(v));
  brush_->brush().setHardness(v / 100.f);
}

void ToolsPanel::onOpacityChanged(int v) {
  if (suppressEdits_ || !brush_) return;
  opacityLabel_->setText(QStringLiteral("%1%").arg(v));
  brush_->brush().setOpacity(v / 100.f);
}

void ToolsPanel::onFlowChanged(int v) {
  if (suppressEdits_ || !brush_) return;
  flowLabel_->setText(QStringLiteral("%1%").arg(v));
  brush_->brush().setFlow(v / 100.f);
}

void ToolsPanel::applyFgToBrush() {
  if (!brush_) return;
  Rgba32F c;
  c.r = static_cast<float>(fg_.redF());
  c.g = static_cast<float>(fg_.greenF());
  c.b = static_cast<float>(fg_.blueF());
  c.a = static_cast<float>(fg_.alphaF());
  brush_->brush().setColor(c);
}

void ToolsPanel::updateSwatchColors() {
  if (fgSwatch_) asSwatch(fgSwatch_)->setColor(fg_);
  if (bgSwatch_) asSwatch(bgSwatch_)->setColor(bg_);
}

}  // namespace tuxels
