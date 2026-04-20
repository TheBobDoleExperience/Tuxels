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
#include "tools/BucketTool.h"
#include "tools/LassoTool.h"
#include "tools/MagicWandTool.h"
#include "tools/PolyLassoTool.h"
#include "tools/SelectByColorTool.h"

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

  pickLassoBtn_ = new QToolButton(pickerRow);
  pickLassoBtn_->setText("L");
  pickLassoBtn_->setToolTip(tr("Lasso — freehand selection  (L)"));
  pickLassoBtn_->setCheckable(true);
  pickerGroup->addButton(pickLassoBtn_);
  connect(pickLassoBtn_, &QToolButton::clicked, this,
          [this]() { emit toolPicked(ToolId::Lasso); });

  pickPolyLassoBtn_ = new QToolButton(pickerRow);
  pickPolyLassoBtn_->setText("P");
  pickPolyLassoBtn_->setToolTip(
      tr("Polygonal Lasso — click vertices, Enter to close"));
  pickPolyLassoBtn_->setCheckable(true);
  pickerGroup->addButton(pickPolyLassoBtn_);
  connect(pickPolyLassoBtn_, &QToolButton::clicked, this,
          [this]() { emit toolPicked(ToolId::PolyLasso); });

  pickBucketBtn_ = new QToolButton(pickerRow);
  pickBucketBtn_->setText("G");
  pickBucketBtn_->setToolTip(tr("Paint Bucket  (G)"));
  pickBucketBtn_->setCheckable(true);
  pickerGroup->addButton(pickBucketBtn_);
  connect(pickBucketBtn_, &QToolButton::clicked, this,
          [this]() { emit toolPicked(ToolId::Bucket); });

  pickWandBtn_ = new QToolButton(pickerRow);
  pickWandBtn_->setText("W");
  pickWandBtn_->setToolTip(tr("Magic Wand  (W)"));
  pickWandBtn_->setCheckable(true);
  pickerGroup->addButton(pickWandBtn_);
  connect(pickWandBtn_, &QToolButton::clicked, this,
          [this]() { emit toolPicked(ToolId::MagicWand); });

  pickSelectByColorBtn_ = new QToolButton(pickerRow);
  pickSelectByColorBtn_->setText("Y");
  pickSelectByColorBtn_->setToolTip(
      tr("Select By Color — non-contiguous color pick  (Shift+W)"));
  pickSelectByColorBtn_->setCheckable(true);
  pickerGroup->addButton(pickSelectByColorBtn_);
  connect(pickSelectByColorBtn_, &QToolButton::clicked, this,
          [this]() { emit toolPicked(ToolId::SelectByColor); });

  pickCropBtn_ = new QToolButton(pickerRow);
  pickCropBtn_->setText("C");
  pickCropBtn_->setToolTip(tr("Crop  (C)"));
  pickCropBtn_->setCheckable(true);
  pickerGroup->addButton(pickCropBtn_);
  connect(pickCropBtn_, &QToolButton::clicked, this,
          [this]() { emit toolPicked(ToolId::Crop); });

  pickMoveBtn_ = new QToolButton(pickerRow);
  pickMoveBtn_->setText("V");
  pickMoveBtn_->setToolTip(tr("Move  (V)"));
  pickMoveBtn_->setCheckable(true);
  pickerGroup->addButton(pickMoveBtn_);
  connect(pickMoveBtn_, &QToolButton::clicked, this,
          [this]() { emit toolPicked(ToolId::Move); });

  pickTransformBtn_ = new QToolButton(pickerRow);
  pickTransformBtn_->setText("T");
  pickTransformBtn_->setToolTip(tr("Free Transform  (Ctrl+T)"));
  pickTransformBtn_->setCheckable(true);
  pickerGroup->addButton(pickTransformBtn_);
  connect(pickTransformBtn_, &QToolButton::clicked, this,
          [this]() { emit toolPicked(ToolId::Transform); });

  pickerLayout->addWidget(pickBrushBtn_);
  pickerLayout->addWidget(pickMarqueeBtn_);
  pickerLayout->addWidget(pickLassoBtn_);
  pickerLayout->addWidget(pickPolyLassoBtn_);
  pickerLayout->addWidget(pickBucketBtn_);
  pickerLayout->addWidget(pickWandBtn_);
  pickerLayout->addWidget(pickSelectByColorBtn_);
  pickerLayout->addWidget(pickCropBtn_);
  pickerLayout->addWidget(pickMoveBtn_);
  pickerLayout->addWidget(pickTransformBtn_);
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

  // -- Lasso options (visible when Lasso OR Polygonal Lasso is active) ----
  // Shared combine-mode row — same four buttons as Marquee. The mode is
  // pushed into both lasso tools so swapping between Lasso and Polygonal
  // Lasso preserves the user's chosen combine behavior.
  lassoGroup_ = new QWidget(root);
  auto* lassoVbox = new QVBoxLayout(lassoGroup_);
  lassoVbox->setContentsMargins(0, 0, 0, 0);
  lassoVbox->setSpacing(4);
  auto* lassoHeader = new QLabel(tr("<b>Lasso</b>"), lassoGroup_);
  lassoVbox->addWidget(lassoHeader);
  auto* lassoModeRow = new QWidget(lassoGroup_);
  auto* lassoModeLayout = new QHBoxLayout(lassoModeRow);
  lassoModeLayout->setContentsMargins(0, 0, 0, 0);
  lassoModeLayout->setSpacing(4);
  auto* lassoModeGroup = new QButtonGroup(this);
  lassoModeGroup->setExclusive(true);
  auto makeLassoModeBtn = [&](const QString& text, const QString& tip,
                               SelectionMode m, bool checked) {
    auto* b = new QToolButton(lassoModeRow);
    b->setText(text);
    b->setToolTip(tip);
    b->setCheckable(true);
    b->setChecked(checked);
    lassoModeGroup->addButton(b);
    lassoModeLayout->addWidget(b);
    connect(b, &QToolButton::clicked, this,
            [this, m]() { emit lassoModeChanged(m); });
    return b;
  };
  lassoReplaceBtn_ =
      makeLassoModeBtn("New", tr("Replace selection  (no modifier)"),
                       SelectionMode::Replace, true);
  lassoAddBtn_ =
      makeLassoModeBtn("+", tr("Add to selection  (Shift)"),
                       SelectionMode::Add, false);
  lassoSubtractBtn_ =
      makeLassoModeBtn("−", tr("Subtract from selection  (Alt)"),
                       SelectionMode::Subtract, false);
  lassoIntersectBtn_ =
      makeLassoModeBtn("∩", tr("Intersect with selection  (Shift+Alt)"),
                       SelectionMode::Intersect, false);
  lassoModeLayout->addStretch(1);
  lassoVbox->addWidget(lassoModeRow);
  lassoGroup_->setVisible(false);
  vbox->addWidget(lassoGroup_);

  // -- Bucket options (visible only when Bucket is active) ----------------
  bucketGroup_ = new QWidget(root);
  auto* bucketVbox = new QVBoxLayout(bucketGroup_);
  bucketVbox->setContentsMargins(0, 0, 0, 0);
  bucketVbox->setSpacing(4);
  auto* bucketHeader = new QLabel(tr("<b>Bucket</b>"), bucketGroup_);
  bucketVbox->addWidget(bucketHeader);
  auto* bucketForm = new QFormLayout();
  bucketForm->setContentsMargins(0, 0, 0, 0);
  bucketForm->setSpacing(4);

  auto makeIntRow = [&](QSlider*& slider, QLabel*& label, int rangeMax,
                         const QString& suffix) {
    auto* row = new QWidget(bucketGroup_);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(0, rangeMax);
    slider->setSingleStep(1);
    label = new QLabel(QStringLiteral("0%1").arg(suffix), row);
    label->setFixedWidth(42);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(slider, 1);
    layout->addWidget(label);
    return row;
  };

  // Tolerance: 0–255 matches Photoshop's UI range; we divide by 255 before
  // handing to the flood fill (normalized float distance).
  auto* tolRow = makeIntRow(bucketToleranceSlider_, bucketToleranceLabel_,
                            255, "");
  bucketToleranceLabel_->setText("0");
  connect(bucketToleranceSlider_, &QSlider::valueChanged, this,
          &ToolsPanel::onBucketToleranceChanged);
  bucketForm->addRow(tr("Tolerance"), tolRow);

  auto* fillOpRow =
      makeIntRow(bucketOpacitySlider_, bucketOpacityLabel_, 100, "%");
  bucketOpacitySlider_->setValue(100);
  bucketOpacityLabel_->setText("100%");
  connect(bucketOpacitySlider_, &QSlider::valueChanged, this,
          &ToolsPanel::onBucketOpacityChanged);
  bucketForm->addRow(tr("Opacity"), fillOpRow);

  bucketVbox->addLayout(bucketForm);
  bucketGroup_->setVisible(false);
  vbox->addWidget(bucketGroup_);

  // -- Magic wand options (visible only when Wand is active) -------------
  wandGroup_ = new QWidget(root);
  auto* wandVbox = new QVBoxLayout(wandGroup_);
  wandVbox->setContentsMargins(0, 0, 0, 0);
  wandVbox->setSpacing(4);
  wandHeader_ = new QLabel(tr("<b>Magic Wand</b>"), wandGroup_);
  wandVbox->addWidget(wandHeader_);

  // Combine-mode row — same four buttons as the Marquee options row, kept
  // separate so each tool carries its own persistent mode. Alt-based
  // modifiers may still be hijacked by the WM; these buttons are the
  // hijack-proof path.
  auto* wandModeRow = new QWidget(wandGroup_);
  auto* wandModeLayout = new QHBoxLayout(wandModeRow);
  wandModeLayout->setContentsMargins(0, 0, 0, 0);
  wandModeLayout->setSpacing(4);
  auto* wandModeGroup = new QButtonGroup(this);
  wandModeGroup->setExclusive(true);
  auto makeWandModeBtn = [&](const QString& text, const QString& tip,
                              SelectionMode m, bool checked) {
    auto* b = new QToolButton(wandModeRow);
    b->setText(text);
    b->setToolTip(tip);
    b->setCheckable(true);
    b->setChecked(checked);
    wandModeGroup->addButton(b);
    wandModeLayout->addWidget(b);
    connect(b, &QToolButton::clicked, this,
            [this, m]() { emit wandModeChanged(m); });
    return b;
  };
  wandReplaceBtn_ = makeWandModeBtn(
      "New", tr("Replace selection  (no modifier)"), SelectionMode::Replace,
      true);
  wandAddBtn_ = makeWandModeBtn(
      "+", tr("Add to selection  (Shift)"), SelectionMode::Add, false);
  wandSubtractBtn_ = makeWandModeBtn(
      "−", tr("Subtract from selection  (Alt)"), SelectionMode::Subtract,
      false);
  wandIntersectBtn_ = makeWandModeBtn(
      "∩", tr("Intersect with selection  (Shift+Alt)"),
      SelectionMode::Intersect, false);
  wandModeLayout->addStretch(1);
  wandVbox->addWidget(wandModeRow);

  auto* wandForm = new QFormLayout();
  wandForm->setContentsMargins(0, 0, 0, 0);
  wandForm->setSpacing(4);
  auto* wandTolRow = new QWidget(wandGroup_);
  auto* wandTolLayout = new QHBoxLayout(wandTolRow);
  wandTolLayout->setContentsMargins(0, 0, 0, 0);
  wandToleranceSlider_ = new QSlider(Qt::Horizontal, wandTolRow);
  wandToleranceSlider_->setRange(0, 255);
  wandToleranceSlider_->setSingleStep(1);
  wandToleranceLabel_ = new QLabel("0", wandTolRow);
  wandToleranceLabel_->setFixedWidth(42);
  wandToleranceLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  wandTolLayout->addWidget(wandToleranceSlider_, 1);
  wandTolLayout->addWidget(wandToleranceLabel_);
  connect(wandToleranceSlider_, &QSlider::valueChanged, this,
          &ToolsPanel::onWandToleranceChanged);
  wandForm->addRow(tr("Tolerance"), wandTolRow);
  wandVbox->addLayout(wandForm);
  wandGroup_->setVisible(false);
  vbox->addWidget(wandGroup_);

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

  // Dynamics: per-stamp size + opacity jitter (0–100%, 0 disables) and a
  // spacing slider that maps to spacingRatio ∈ [0.01, 1.0]. These three
  // rows are the user-facing surface for S6 brush dynamics.
  auto* sizeJitterRow = makePctRow(sizeJitterSlider_, sizeJitterLabel_);
  sizeJitterSlider_->setValue(0);
  sizeJitterLabel_->setText("0%");
  sizeJitterRow->setToolTip(
      tr("Size jitter — per-stamp random size variation"));
  connect(sizeJitterSlider_, &QSlider::valueChanged, this,
          &ToolsPanel::onSizeJitterChanged);
  form->addRow(tr("Size Jitter"), sizeJitterRow);

  auto* opJitterRow = makePctRow(opacityJitterSlider_, opacityJitterLabel_);
  opacityJitterSlider_->setValue(0);
  opacityJitterLabel_->setText("0%");
  opJitterRow->setToolTip(
      tr("Opacity jitter — per-stamp random opacity variation"));
  connect(opacityJitterSlider_, &QSlider::valueChanged, this,
          &ToolsPanel::onOpacityJitterChanged);
  form->addRow(tr("Opacity Jitter"), opJitterRow);

  auto* spacingRow = makePctRow(spacingSlider_, spacingLabel_);
  // Slider is in percent (1–100), maps linearly onto spacingRatio ∈
  // [0.01, 1.0]. Default matches RoundBrushParams' 0.1 → 10%.
  spacingSlider_->setRange(1, 100);
  spacingSlider_->setValue(10);
  spacingLabel_->setText("10%");
  spacingRow->setToolTip(
      tr("Spacing — stamp interval as fraction of brush size"));
  connect(spacingSlider_, &QSlider::valueChanged, this,
          &ToolsPanel::onSpacingChanged);
  form->addRow(tr("Spacing"), spacingRow);

  brushVbox->addLayout(form);
  vbox->addWidget(brushGroup_);
  vbox->addStretch(1);
  setWidget(root);

  updateSwatchColors();
}

void ToolsPanel::setActiveTool(ToolId id) {
  if (pickBrushBtn_) pickBrushBtn_->setChecked(id == ToolId::Brush);
  if (pickMarqueeBtn_) pickMarqueeBtn_->setChecked(id == ToolId::Marquee);
  if (pickBucketBtn_) pickBucketBtn_->setChecked(id == ToolId::Bucket);
  if (pickWandBtn_) pickWandBtn_->setChecked(id == ToolId::MagicWand);
  if (pickSelectByColorBtn_)
    pickSelectByColorBtn_->setChecked(id == ToolId::SelectByColor);
  if (pickCropBtn_) pickCropBtn_->setChecked(id == ToolId::Crop);
  if (pickMoveBtn_) pickMoveBtn_->setChecked(id == ToolId::Move);
  if (pickTransformBtn_) pickTransformBtn_->setChecked(id == ToolId::Transform);
  if (pickLassoBtn_) pickLassoBtn_->setChecked(id == ToolId::Lasso);
  if (pickPolyLassoBtn_)
    pickPolyLassoBtn_->setChecked(id == ToolId::PolyLasso);
  if (brushGroup_) brushGroup_->setVisible(id == ToolId::Brush);
  if (marqueeGroup_) marqueeGroup_->setVisible(id == ToolId::Marquee);
  if (bucketGroup_) bucketGroup_->setVisible(id == ToolId::Bucket);
  if (wandGroup_)
    wandGroup_->setVisible(id == ToolId::MagicWand ||
                           id == ToolId::SelectByColor);
  if (wandHeader_) {
    wandHeader_->setText(id == ToolId::SelectByColor
                             ? tr("<b>Select By Color</b>")
                             : tr("<b>Magic Wand</b>"));
  }
  if (lassoGroup_)
    lassoGroup_->setVisible(id == ToolId::Lasso || id == ToolId::PolyLasso);
}

void ToolsPanel::setMarqueeMode(SelectionMode m) {
  // Only `clicked` is wired (not `toggled`), so setChecked is silent.
  if (marqueeReplaceBtn_)   marqueeReplaceBtn_->setChecked(m == SelectionMode::Replace);
  if (marqueeAddBtn_)       marqueeAddBtn_->setChecked(m == SelectionMode::Add);
  if (marqueeSubtractBtn_)  marqueeSubtractBtn_->setChecked(m == SelectionMode::Subtract);
  if (marqueeIntersectBtn_) marqueeIntersectBtn_->setChecked(m == SelectionMode::Intersect);
}

void ToolsPanel::setWandMode(SelectionMode m) {
  if (wandReplaceBtn_)   wandReplaceBtn_->setChecked(m == SelectionMode::Replace);
  if (wandAddBtn_)       wandAddBtn_->setChecked(m == SelectionMode::Add);
  if (wandSubtractBtn_)  wandSubtractBtn_->setChecked(m == SelectionMode::Subtract);
  if (wandIntersectBtn_) wandIntersectBtn_->setChecked(m == SelectionMode::Intersect);
}

void ToolsPanel::setLassoMode(SelectionMode m) {
  if (lassoReplaceBtn_)   lassoReplaceBtn_->setChecked(m == SelectionMode::Replace);
  if (lassoAddBtn_)       lassoAddBtn_->setChecked(m == SelectionMode::Add);
  if (lassoSubtractBtn_)  lassoSubtractBtn_->setChecked(m == SelectionMode::Subtract);
  if (lassoIntersectBtn_) lassoIntersectBtn_->setChecked(m == SelectionMode::Intersect);
}

void ToolsPanel::setBrushTool(BrushTool* tool) {
  brush_ = tool;
  refreshFromBrush();
  applyFgToBrush();
}

void ToolsPanel::setBucketTool(BucketTool* tool) {
  bucket_ = tool;
  if (!bucket_) return;
  // Push current slider values into the tool so it starts in the state the
  // UI claims (0 tolerance, 100% opacity by default).
  onBucketToleranceChanged(bucketToleranceSlider_
                               ? bucketToleranceSlider_->value()
                               : 0);
  onBucketOpacityChanged(bucketOpacitySlider_
                             ? bucketOpacitySlider_->value()
                             : 100);
  // Seed the fill color from the current fg swatch.
  applyFgToBrush();
}

void ToolsPanel::setMagicWandTool(MagicWandTool* tool) {
  wand_ = tool;
  if (!wand_) return;
  onWandToleranceChanged(wandToleranceSlider_ ? wandToleranceSlider_->value()
                                              : 0);
}

void ToolsPanel::setSelectByColorTool(SelectByColorTool* tool) {
  selectByColor_ = tool;
  if (!selectByColor_) return;
  // Seed the tool's tolerance from the shared wand slider so its first
  // click uses the value the user sees in the panel.
  onWandToleranceChanged(wandToleranceSlider_ ? wandToleranceSlider_->value()
                                              : 0);
}

void ToolsPanel::setLassoTools(LassoTool* lasso, PolyLassoTool* polyLasso) {
  lasso_ = lasso;
  polyLasso_ = polyLasso;
  // No tolerance/opacity sliders yet — the only control is the shared
  // combine-mode row, which is already wired via the lassoModeChanged
  // signal. MainWindow seeds the options row from the tool's initial
  // mode in buildDocks.
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
  const int sjPct = static_cast<int>(std::lround(p.sizeJitter * 100.f));
  const int ojPct = static_cast<int>(std::lround(p.opacityJitter * 100.f));
  const int spPct = static_cast<int>(std::lround(p.spacingRatio * 100.f));
  if (sizeJitterSlider_) {
    sizeJitterSlider_->setValue(sjPct);
    sizeJitterLabel_->setText(QStringLiteral("%1%").arg(sjPct));
  }
  if (opacityJitterSlider_) {
    opacityJitterSlider_->setValue(ojPct);
    opacityJitterLabel_->setText(QStringLiteral("%1%").arg(ojPct));
  }
  if (spacingSlider_) {
    spacingSlider_->setValue(std::clamp(spPct, 1, 100));
    spacingLabel_->setText(QStringLiteral("%1%").arg(spPct));
  }
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

void ToolsPanel::onSizeJitterChanged(int v) {
  if (suppressEdits_ || !brush_) return;
  if (sizeJitterLabel_) sizeJitterLabel_->setText(QStringLiteral("%1%").arg(v));
  brush_->brush().setSizeJitter(v / 100.f);
}

void ToolsPanel::onOpacityJitterChanged(int v) {
  if (suppressEdits_ || !brush_) return;
  if (opacityJitterLabel_)
    opacityJitterLabel_->setText(QStringLiteral("%1%").arg(v));
  brush_->brush().setOpacityJitter(v / 100.f);
}

void ToolsPanel::onSpacingChanged(int v) {
  if (suppressEdits_ || !brush_) return;
  if (spacingLabel_) spacingLabel_->setText(QStringLiteral("%1%").arg(v));
  brush_->brush().setSpacingRatio(v / 100.f);
}

void ToolsPanel::applyFgToBrush() {
  Rgba32F c;
  c.r = static_cast<float>(fg_.redF());
  c.g = static_cast<float>(fg_.greenF());
  c.b = static_cast<float>(fg_.blueF());
  c.a = static_cast<float>(fg_.alphaF());
  if (brush_) brush_->brush().setColor(c);
  if (bucket_) bucket_->setColor(c);
}

void ToolsPanel::onBucketToleranceChanged(int v) {
  if (bucketToleranceLabel_) bucketToleranceLabel_->setText(QString::number(v));
  if (bucket_) bucket_->setTolerance(v / 255.f);
}

void ToolsPanel::onBucketOpacityChanged(int v) {
  if (bucketOpacityLabel_)
    bucketOpacityLabel_->setText(QStringLiteral("%1%").arg(v));
  if (bucket_) bucket_->setOpacity(v / 100.f);
}

void ToolsPanel::onWandToleranceChanged(int v) {
  if (wandToleranceLabel_) wandToleranceLabel_->setText(QString::number(v));
  const float t = v / 255.f;
  if (wand_) wand_->setTolerance(t);
  if (selectByColor_) selectByColor_->setTolerance(t);
}

void ToolsPanel::updateSwatchColors() {
  if (fgSwatch_) asSwatch(fgSwatch_)->setColor(fg_);
  if (bgSwatch_) asSwatch(bgSwatch_)->setColor(bg_);
}

}  // namespace tuxels
