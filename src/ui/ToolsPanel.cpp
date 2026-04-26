#include "ui/ToolsPanel.h"

#include <QButtonGroup>
#include <QColorDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollArea>
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
#include "ui/CollapsibleSection.h"

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

  // Wrap everything in a QScrollArea so a tall expanded accordion stays
  // navigable in a short dock. The scroll target is the actual content widget
  // populated below.
  auto* scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);

  auto* root = new QWidget(scroll);
  auto* vbox = new QVBoxLayout(root);
  vbox->setContentsMargins(8, 8, 8, 8);
  vbox->setSpacing(6);

  // -- Pinned color swatch row -------------------------------------------
  // FG/BG colors aren't tool-specific (Brush + Bucket both consume them), so
  // they live above the accordion as global state.
  auto* colorGroup = new QWidget(root);
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
  vbox->addWidget(colorGroup);

  // Thin separator between the pinned color row and the accordion.
  auto* sep = new QFrame(root);
  sep->setFrameShape(QFrame::HLine);
  sep->setFrameShadow(QFrame::Sunken);
  vbox->addWidget(sep);

  // -- Accordion sections ------------------------------------------------
  // Order is independent of MainWindow keyboard order — choose what reads
  // best as a vertical inspector. Selection tools first, paint tools, then
  // transforms / crops at the bottom.
  addSection(root, ToolId::Move, tr("Move"), QStringLiteral("V"),
             buildMoveBody(root));
  addSection(root, ToolId::Marquee, tr("Rectangular Marquee"),
             QStringLiteral("M"), buildMarqueeBody(root));
  addSection(root, ToolId::Lasso, tr("Lasso"), QStringLiteral("L"),
             buildLassoBody(root, /*poly=*/false));
  addSection(root, ToolId::PolyLasso, tr("Polygonal Lasso"),
             QStringLiteral("P"), buildLassoBody(root, /*poly=*/true));
  addSection(root, ToolId::MagicWand, tr("Magic Wand"), QStringLiteral("W"),
             buildWandBody(root, /*sbc=*/false));
  addSection(root, ToolId::SelectByColor, tr("Select By Color"),
             QStringLiteral("⇧W"), buildWandBody(root, /*sbc=*/true));
  addSection(root, ToolId::Crop, tr("Crop"), QStringLiteral("C"),
             buildCropBody(root));
  addSection(root, ToolId::Brush, tr("Brush"), QStringLiteral("B"),
             buildBrushBody(root));
  addSection(root, ToolId::Bucket, tr("Paint Bucket"), QStringLiteral("G"),
             buildBucketBody(root));
  addSection(root, ToolId::Transform, tr("Free Transform"),
             QStringLiteral("⌃T"), buildTransformBody(root));

  // Brush starts as the active tool, matching MainWindow's default.
  setActiveTool(ToolId::Brush);

  vbox->addStretch(1);

  scroll->setWidget(root);
  setWidget(scroll);

  updateSwatchColors();
}

CollapsibleSection* ToolsPanel::addSection(QWidget* root, ToolId id,
                                           const QString& name,
                                           const QString& shortcut,
                                           QWidget* body) {
  auto* section = new CollapsibleSection(name, shortcut, root);
  section->setBody(body);
  connect(section, &CollapsibleSection::headerClicked, this,
          [this, id]() { emit toolPicked(id); });
  // chevronClicked is intentionally not wired — CollapsibleSection already
  // toggles its own expansion in the eventFilter; we don't need a slot.

  static_cast<QVBoxLayout*>(root->layout())->addWidget(section);
  sections_[static_cast<int>(id)] = section;
  return section;
}

QWidget* ToolsPanel::buildMoveBody(QWidget* parent) {
  auto* w = new QWidget(parent);
  auto* lay = new QVBoxLayout(w);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(2);
  auto* tip = new QLabel(
      tr("Drag in the canvas to reposition the active layer."), w);
  tip->setWordWrap(true);
  tip->setStyleSheet("color: rgba(200, 200, 200, 180);");
  lay->addWidget(tip);
  return w;
}

QWidget* ToolsPanel::buildMarqueeBody(QWidget* parent) {
  auto* w = new QWidget(parent);
  auto* lay = new QVBoxLayout(w);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(4);

  auto* modeRow = new QWidget(w);
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
  marqueeReplaceBtn_ = makeModeBtn(
      "New", tr("Replace selection  (no modifier)"), SelectionMode::Replace,
      true);
  marqueeAddBtn_ = makeModeBtn("+", tr("Add to selection  (Shift)"),
                               SelectionMode::Add, false);
  marqueeSubtractBtn_ =
      makeModeBtn("−", tr("Subtract from selection  (Alt)"),
                  SelectionMode::Subtract, false);
  marqueeIntersectBtn_ =
      makeModeBtn("∩", tr("Intersect with selection  (Shift+Alt)"),
                  SelectionMode::Intersect, false);
  modeLayout->addStretch(1);
  lay->addWidget(modeRow);
  return w;
}

QWidget* ToolsPanel::buildLassoBody(QWidget* parent, bool poly) {
  auto* w = new QWidget(parent);
  auto* lay = new QVBoxLayout(w);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(4);

  auto* modeRow = new QWidget(w);
  auto* modeLayout = new QHBoxLayout(modeRow);
  modeLayout->setContentsMargins(0, 0, 0, 0);
  modeLayout->setSpacing(4);
  auto* modeGroup = new QButtonGroup(this);
  modeGroup->setExclusive(true);

  // Both Lasso and PolyLasso share one persistent combine mode in MainWindow
  // — emit the same `lassoModeChanged` signal from either section's buttons.
  // setLassoMode below mirrors the state into both rows.
  QToolButton** rep = poly ? &polyLassoReplaceBtn_ : &lassoReplaceBtn_;
  QToolButton** add = poly ? &polyLassoAddBtn_ : &lassoAddBtn_;
  QToolButton** sub = poly ? &polyLassoSubtractBtn_ : &lassoSubtractBtn_;
  QToolButton** ins = poly ? &polyLassoIntersectBtn_ : &lassoIntersectBtn_;

  auto makeModeBtn = [&](QToolButton** dst, const QString& text,
                         const QString& tip, SelectionMode m, bool checked) {
    auto* b = new QToolButton(modeRow);
    b->setText(text);
    b->setToolTip(tip);
    b->setCheckable(true);
    b->setChecked(checked);
    modeGroup->addButton(b);
    modeLayout->addWidget(b);
    connect(b, &QToolButton::clicked, this,
            [this, m]() { emit lassoModeChanged(m); });
    *dst = b;
  };
  makeModeBtn(rep, "New", tr("Replace selection  (no modifier)"),
              SelectionMode::Replace, true);
  makeModeBtn(add, "+", tr("Add to selection  (Shift)"), SelectionMode::Add,
              false);
  makeModeBtn(sub, "−", tr("Subtract from selection  (Alt)"),
              SelectionMode::Subtract, false);
  makeModeBtn(ins, "∩", tr("Intersect with selection  (Shift+Alt)"),
              SelectionMode::Intersect, false);
  modeLayout->addStretch(1);
  lay->addWidget(modeRow);

  if (poly) {
    auto* tip = new QLabel(
        tr("Click to add a vertex; Enter closes, Escape cancels."), w);
    tip->setWordWrap(true);
    tip->setStyleSheet("color: rgba(200, 200, 200, 180);");
    lay->addWidget(tip);
  }
  return w;
}

QWidget* ToolsPanel::buildWandBody(QWidget* parent, bool sbc) {
  auto* w = new QWidget(parent);
  auto* lay = new QVBoxLayout(w);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(4);

  // Combine-mode row — same four buttons as the Marquee. Wand and SBC share
  // a single persistent mode through MainWindow; setWandMode mirrors state
  // into both rows.
  auto* modeRow = new QWidget(w);
  auto* modeLayout = new QHBoxLayout(modeRow);
  modeLayout->setContentsMargins(0, 0, 0, 0);
  modeLayout->setSpacing(4);
  auto* modeGroup = new QButtonGroup(this);
  modeGroup->setExclusive(true);

  QToolButton** rep = sbc ? &sbcReplaceBtn_ : &wandReplaceBtn_;
  QToolButton** add = sbc ? &sbcAddBtn_ : &wandAddBtn_;
  QToolButton** sub = sbc ? &sbcSubtractBtn_ : &wandSubtractBtn_;
  QToolButton** ins = sbc ? &sbcIntersectBtn_ : &wandIntersectBtn_;

  auto makeModeBtn = [&](QToolButton** dst, const QString& text,
                         const QString& tip, SelectionMode m, bool checked) {
    auto* b = new QToolButton(modeRow);
    b->setText(text);
    b->setToolTip(tip);
    b->setCheckable(true);
    b->setChecked(checked);
    modeGroup->addButton(b);
    modeLayout->addWidget(b);
    connect(b, &QToolButton::clicked, this,
            [this, m]() { emit wandModeChanged(m); });
    *dst = b;
  };
  makeModeBtn(rep, "New", tr("Replace selection  (no modifier)"),
              SelectionMode::Replace, true);
  makeModeBtn(add, "+", tr("Add to selection  (Shift)"), SelectionMode::Add,
              false);
  makeModeBtn(sub, "−", tr("Subtract from selection  (Alt)"),
              SelectionMode::Subtract, false);
  makeModeBtn(ins, "∩", tr("Intersect with selection  (Shift+Alt)"),
              SelectionMode::Intersect, false);
  modeLayout->addStretch(1);
  lay->addWidget(modeRow);

  // Tolerance slider — separate widget per section, kept in sync via the
  // shared on-change handler below. The tolerance is shared between Wand
  // and SBC tools because Photoshop groups them.
  auto* tolRow = new QWidget(w);
  auto* tolLayout = new QHBoxLayout(tolRow);
  tolLayout->setContentsMargins(0, 0, 0, 0);
  auto* tolLabel = new QLabel(tr("Tolerance"), tolRow);
  tolLabel->setFixedWidth(72);
  tolLayout->addWidget(tolLabel);
  auto* tolSlider = new QSlider(Qt::Horizontal, tolRow);
  tolSlider->setRange(0, 255);
  tolSlider->setSingleStep(1);
  auto* tolValue = new QLabel("0", tolRow);
  tolValue->setFixedWidth(42);
  tolValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  tolLayout->addWidget(tolSlider, 1);
  tolLayout->addWidget(tolValue);
  if (sbc) {
    sbcToleranceSlider_ = tolSlider;
    sbcToleranceLabel_ = tolValue;
  } else {
    wandToleranceSlider_ = tolSlider;
    wandToleranceLabel_ = tolValue;
  }
  connect(tolSlider, &QSlider::valueChanged, this,
          &ToolsPanel::onWandToleranceChanged);
  lay->addWidget(tolRow);
  return w;
}

QWidget* ToolsPanel::buildCropBody(QWidget* parent) {
  auto* w = new QWidget(parent);
  auto* lay = new QVBoxLayout(w);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(2);
  auto* tip = new QLabel(
      tr("Drag a rectangle in the canvas; release to crop. Undo to revert."),
      w);
  tip->setWordWrap(true);
  tip->setStyleSheet("color: rgba(200, 200, 200, 180);");
  lay->addWidget(tip);
  return w;
}

QWidget* ToolsPanel::buildBrushBody(QWidget* parent) {
  auto* w = new QWidget(parent);
  auto* lay = new QVBoxLayout(w);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(4);

  auto* form = new QFormLayout();
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(4);

  auto* sizeRow = new QWidget(w);
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
  connect(sizeSlider_, &QSlider::valueChanged, this,
          &ToolsPanel::onSizeChanged);
  connect(sizeSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &ToolsPanel::onSizeChanged);
  form->addRow(tr("Size"), sizeRow);

  auto makePctRow = [&](QSlider*& slider, QLabel*& label) {
    auto* row = new QWidget(w);
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
  spacingSlider_->setRange(1, 100);
  spacingSlider_->setValue(10);
  spacingLabel_->setText("10%");
  spacingRow->setToolTip(
      tr("Spacing — stamp interval as fraction of brush size"));
  connect(spacingSlider_, &QSlider::valueChanged, this,
          &ToolsPanel::onSpacingChanged);
  form->addRow(tr("Spacing"), spacingRow);

  lay->addLayout(form);
  return w;
}

QWidget* ToolsPanel::buildBucketBody(QWidget* parent) {
  auto* w = new QWidget(parent);
  auto* lay = new QVBoxLayout(w);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(4);

  auto* form = new QFormLayout();
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(4);

  auto makeIntRow = [&](QSlider*& slider, QLabel*& label, int rangeMax,
                        const QString& suffix) {
    auto* row = new QWidget(w);
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

  auto* tolRow = makeIntRow(bucketToleranceSlider_, bucketToleranceLabel_,
                            255, "");
  bucketToleranceLabel_->setText("0");
  connect(bucketToleranceSlider_, &QSlider::valueChanged, this,
          &ToolsPanel::onBucketToleranceChanged);
  form->addRow(tr("Tolerance"), tolRow);

  auto* fillOpRow =
      makeIntRow(bucketOpacitySlider_, bucketOpacityLabel_, 100, "%");
  bucketOpacitySlider_->setValue(100);
  bucketOpacityLabel_->setText("100%");
  connect(bucketOpacitySlider_, &QSlider::valueChanged, this,
          &ToolsPanel::onBucketOpacityChanged);
  form->addRow(tr("Opacity"), fillOpRow);

  lay->addLayout(form);
  return w;
}

QWidget* ToolsPanel::buildTransformBody(QWidget* parent) {
  auto* w = new QWidget(parent);
  auto* lay = new QVBoxLayout(w);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(2);
  auto* tip = new QLabel(
      tr("Ctrl+T enters Free Transform. Drag corners to scale (Shift = "
         "lock aspect), edges to rotate (Shift = 15° snaps), inside to "
         "translate, the pivot dot to relocate. Enter commits, Escape "
         "cancels."),
      w);
  tip->setWordWrap(true);
  tip->setStyleSheet("color: rgba(200, 200, 200, 180);");
  lay->addWidget(tip);
  return w;
}

CollapsibleSection* ToolsPanel::sectionFor(ToolId id) const {
  auto it = sections_.find(static_cast<int>(id));
  return it == sections_.end() ? nullptr : it->second;
}

void ToolsPanel::setActiveTool(ToolId id) {
  // Highlight the active tool's section header. Manual collapse state is
  // preserved — we never call setExpanded here.
  for (auto& [secId, section] : sections_) {
    section->setActive(secId == static_cast<int>(id));
  }
}

void ToolsPanel::setMarqueeMode(SelectionMode m) {
  if (marqueeReplaceBtn_)   marqueeReplaceBtn_->setChecked(m == SelectionMode::Replace);
  if (marqueeAddBtn_)       marqueeAddBtn_->setChecked(m == SelectionMode::Add);
  if (marqueeSubtractBtn_)  marqueeSubtractBtn_->setChecked(m == SelectionMode::Subtract);
  if (marqueeIntersectBtn_) marqueeIntersectBtn_->setChecked(m == SelectionMode::Intersect);
}

void ToolsPanel::setWandMode(SelectionMode m) {
  // Mirror state into both Wand and SBC sections — they share a persistent
  // mode in MainWindow.
  if (wandReplaceBtn_)   wandReplaceBtn_->setChecked(m == SelectionMode::Replace);
  if (wandAddBtn_)       wandAddBtn_->setChecked(m == SelectionMode::Add);
  if (wandSubtractBtn_)  wandSubtractBtn_->setChecked(m == SelectionMode::Subtract);
  if (wandIntersectBtn_) wandIntersectBtn_->setChecked(m == SelectionMode::Intersect);
  if (sbcReplaceBtn_)    sbcReplaceBtn_->setChecked(m == SelectionMode::Replace);
  if (sbcAddBtn_)        sbcAddBtn_->setChecked(m == SelectionMode::Add);
  if (sbcSubtractBtn_)   sbcSubtractBtn_->setChecked(m == SelectionMode::Subtract);
  if (sbcIntersectBtn_)  sbcIntersectBtn_->setChecked(m == SelectionMode::Intersect);
}

void ToolsPanel::setLassoMode(SelectionMode m) {
  if (lassoReplaceBtn_)       lassoReplaceBtn_->setChecked(m == SelectionMode::Replace);
  if (lassoAddBtn_)           lassoAddBtn_->setChecked(m == SelectionMode::Add);
  if (lassoSubtractBtn_)      lassoSubtractBtn_->setChecked(m == SelectionMode::Subtract);
  if (lassoIntersectBtn_)     lassoIntersectBtn_->setChecked(m == SelectionMode::Intersect);
  if (polyLassoReplaceBtn_)   polyLassoReplaceBtn_->setChecked(m == SelectionMode::Replace);
  if (polyLassoAddBtn_)       polyLassoAddBtn_->setChecked(m == SelectionMode::Add);
  if (polyLassoSubtractBtn_)  polyLassoSubtractBtn_->setChecked(m == SelectionMode::Subtract);
  if (polyLassoIntersectBtn_) polyLassoIntersectBtn_->setChecked(m == SelectionMode::Intersect);
}

void ToolsPanel::setBrushTool(BrushTool* tool) {
  brush_ = tool;
  refreshFromBrush();
  applyFgToBrush();
}

void ToolsPanel::setBucketTool(BucketTool* tool) {
  bucket_ = tool;
  if (!bucket_) return;
  onBucketToleranceChanged(bucketToleranceSlider_
                               ? bucketToleranceSlider_->value()
                               : 0);
  onBucketOpacityChanged(bucketOpacitySlider_
                             ? bucketOpacitySlider_->value()
                             : 100);
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
  onWandToleranceChanged(wandToleranceSlider_ ? wandToleranceSlider_->value()
                                              : 0);
}

void ToolsPanel::setLassoTools(LassoTool* lasso, PolyLassoTool* polyLasso) {
  lasso_ = lasso;
  polyLasso_ = polyLasso;
}

void ToolsPanel::refreshFromBrush() {
  if (!brush_) return;
  suppressEdits_ = true;
  const auto& p = brush_->brush().params();
  if (sizeSlider_) sizeSlider_->setValue(std::min(p.diameter, sizeSlider_->maximum()));
  if (sizeSpin_) sizeSpin_->setValue(p.diameter);
  const int hPct = static_cast<int>(std::lround(p.hardness * 100.f));
  const int oPct = static_cast<int>(std::lround(p.opacity * 100.f));
  const int fPct = static_cast<int>(std::lround(p.flow * 100.f));
  if (hardnessSlider_) hardnessSlider_->setValue(hPct);
  if (hardnessLabel_) hardnessLabel_->setText(QStringLiteral("%1%").arg(hPct));
  if (opacitySlider_) opacitySlider_->setValue(oPct);
  if (opacityLabel_) opacityLabel_->setText(QStringLiteral("%1%").arg(oPct));
  if (flowSlider_) flowSlider_->setValue(fPct);
  if (flowLabel_) flowLabel_->setText(QStringLiteral("%1%").arg(fPct));
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
  if (sizeSlider_) sizeSlider_->setValue(std::min(v, sizeSlider_->maximum()));
  if (sizeSpin_) sizeSpin_->setValue(v);
  suppressEdits_ = false;
  brush_->brush().setDiameter(v);
}

void ToolsPanel::onHardnessChanged(int v) {
  if (suppressEdits_ || !brush_) return;
  if (hardnessLabel_) hardnessLabel_->setText(QStringLiteral("%1%").arg(v));
  brush_->brush().setHardness(v / 100.f);
}

void ToolsPanel::onOpacityChanged(int v) {
  if (suppressEdits_ || !brush_) return;
  if (opacityLabel_) opacityLabel_->setText(QStringLiteral("%1%").arg(v));
  brush_->brush().setOpacity(v / 100.f);
}

void ToolsPanel::onFlowChanged(int v) {
  if (suppressEdits_ || !brush_) return;
  if (flowLabel_) flowLabel_->setText(QStringLiteral("%1%").arg(v));
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
  // Mirror the value into both sliders so dragging in one section reflects
  // in the other (the two sliders share underlying tool tolerance).
  suppressEdits_ = true;
  if (wandToleranceSlider_ && wandToleranceSlider_->value() != v)
    wandToleranceSlider_->setValue(v);
  if (sbcToleranceSlider_ && sbcToleranceSlider_->value() != v)
    sbcToleranceSlider_->setValue(v);
  suppressEdits_ = false;

  if (wandToleranceLabel_) wandToleranceLabel_->setText(QString::number(v));
  if (sbcToleranceLabel_) sbcToleranceLabel_->setText(QString::number(v));
  const float t = v / 255.f;
  if (wand_) wand_->setTolerance(t);
  if (selectByColor_) selectByColor_->setTolerance(t);
}

void ToolsPanel::updateSwatchColors() {
  if (fgSwatch_) asSwatch(fgSwatch_)->setColor(fg_);
  if (bgSwatch_) asSwatch(bgSwatch_)->setColor(bg_);
}

}  // namespace tuxels
