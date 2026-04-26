#include "ui/PropertiesPaneLevels.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace tuxels {

// Lightweight histogram backdrop. One channel at a time, picked by the
// pane's channel combo. Lifted from LevelsDialog.cpp so the pane is
// self-contained.
class LevelsHistogramView : public QWidget {
 public:
  explicit LevelsHistogramView(QWidget* parent = nullptr) : QWidget(parent) {
    setMinimumSize(256, 96);
    setAutoFillBackground(true);
  }

  void setData(Histogram4x256 hist) {
    hist_ = hist;
    update();
  }

  void setChannel(int ch) {
    if (ch == channel_) return;
    channel_ = std::clamp(ch, 0, 3);
    update();
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.fillRect(rect(), QColor(28, 28, 32));
    if (hist_.total == 0) return;

    static const int kMap[4] = {3, 0, 1, 2};
    const int row = kMap[channel_];

    uint32_t peak = 1;
    for (int i = 0; i < 256; ++i) peak = std::max(peak, hist_.buckets[row][i]);

    const int w = width();
    const int h = height();
    QColor barColor(200, 200, 200);
    if (channel_ == 1) barColor = QColor(220, 80, 80);
    else if (channel_ == 2) barColor = QColor(80, 200, 80);
    else if (channel_ == 3) barColor = QColor(80, 120, 220);

    p.setPen(Qt::NoPen);
    p.setBrush(barColor);
    for (int i = 0; i < 256; ++i) {
      const int x0 = (i * w) / 256;
      const int x1 = ((i + 1) * w) / 256;
      const int bw = std::max(1, x1 - x0);
      const int bh = static_cast<int>(
          (static_cast<qint64>(hist_.buckets[row][i]) * h) / peak);
      p.drawRect(x0, h - bh, bw, bh);
    }
  }

 private:
  Histogram4x256 hist_{};
  int channel_ = 0;
};

namespace {

constexpr int kSliderSteps = 1000;

int floatToSlider01(float v) {
  return static_cast<int>(std::round(std::clamp(v, 0.f, 1.f) * kSliderSteps));
}
float sliderToFloat01(int s) { return static_cast<float>(s) / kSliderSteps; }

int gammaToSlider(float g) {
  const float t = (std::log(std::clamp(g, 0.1f, 9.99f)) - std::log(0.1f)) /
                  (std::log(9.99f) - std::log(0.1f));
  return static_cast<int>(std::round(t * kSliderSteps));
}
float sliderToGamma(int s) {
  const float t = static_cast<float>(s) / kSliderSteps;
  return static_cast<float>(
      std::exp(std::log(0.1f) + t * (std::log(9.99f) - std::log(0.1f))));
}

}  // namespace

PropertiesPaneLevels::PropertiesPaneLevels(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  channelCombo_ = new QComboBox(this);
  channelCombo_->addItem(tr("Composite"));
  channelCombo_->addItem(tr("Red"));
  channelCombo_->addItem(tr("Green"));
  channelCombo_->addItem(tr("Blue"));
  layout->addWidget(channelCombo_);

  hist_ = new LevelsHistogramView(this);
  layout->addWidget(hist_);

  auto* inputBox = new QGroupBox(tr("Input"), this);
  auto* inputForm = new QFormLayout(inputBox);
  inBlackSlider_ = new QSlider(Qt::Horizontal, this);
  inBlackSlider_->setRange(0, kSliderSteps);
  inBlackSpin_ = new QDoubleSpinBox(this);
  inBlackSpin_->setRange(0.0, 1.0);
  inBlackSpin_->setDecimals(3);
  inBlackSpin_->setSingleStep(0.01);
  auto* inBlackRow = new QHBoxLayout();
  inBlackRow->addWidget(inBlackSlider_, 1);
  inBlackRow->addWidget(inBlackSpin_);
  inputForm->addRow(tr("Black"), inBlackRow);

  gammaSlider_ = new QSlider(Qt::Horizontal, this);
  gammaSlider_->setRange(0, kSliderSteps);
  gammaSpin_ = new QDoubleSpinBox(this);
  gammaSpin_->setRange(0.1, 9.99);
  gammaSpin_->setDecimals(2);
  gammaSpin_->setSingleStep(0.01);
  auto* gammaRow = new QHBoxLayout();
  gammaRow->addWidget(gammaSlider_, 1);
  gammaRow->addWidget(gammaSpin_);
  inputForm->addRow(tr("Gamma"), gammaRow);

  inWhiteSlider_ = new QSlider(Qt::Horizontal, this);
  inWhiteSlider_->setRange(0, kSliderSteps);
  inWhiteSpin_ = new QDoubleSpinBox(this);
  inWhiteSpin_->setRange(0.0, 1.0);
  inWhiteSpin_->setDecimals(3);
  inWhiteSpin_->setSingleStep(0.01);
  auto* inWhiteRow = new QHBoxLayout();
  inWhiteRow->addWidget(inWhiteSlider_, 1);
  inWhiteRow->addWidget(inWhiteSpin_);
  inputForm->addRow(tr("White"), inWhiteRow);

  layout->addWidget(inputBox);

  auto* outputBox = new QGroupBox(tr("Output"), this);
  auto* outputForm = new QFormLayout(outputBox);
  outBlackSlider_ = new QSlider(Qt::Horizontal, this);
  outBlackSlider_->setRange(0, kSliderSteps);
  outBlackSpin_ = new QDoubleSpinBox(this);
  outBlackSpin_->setRange(0.0, 1.0);
  outBlackSpin_->setDecimals(3);
  outBlackSpin_->setSingleStep(0.01);
  auto* outBlackRow = new QHBoxLayout();
  outBlackRow->addWidget(outBlackSlider_, 1);
  outBlackRow->addWidget(outBlackSpin_);
  outputForm->addRow(tr("Black"), outBlackRow);

  outWhiteSlider_ = new QSlider(Qt::Horizontal, this);
  outWhiteSlider_->setRange(0, kSliderSteps);
  outWhiteSpin_ = new QDoubleSpinBox(this);
  outWhiteSpin_->setRange(0.0, 1.0);
  outWhiteSpin_->setDecimals(3);
  outWhiteSpin_->setSingleStep(0.01);
  auto* outWhiteRow = new QHBoxLayout();
  outWhiteRow->addWidget(outWhiteSlider_, 1);
  outWhiteRow->addWidget(outWhiteSpin_);
  outputForm->addRow(tr("White"), outWhiteRow);

  layout->addWidget(outputBox);
  layout->addStretch(1);

  // Wire all sliders + spin boxes. Drag-detection: sliderPressed snapshots,
  // sliderReleased commits if changed.
  for (auto* s : {inBlackSlider_, gammaSlider_, inWhiteSlider_,
                  outBlackSlider_, outWhiteSlider_}) {
    connect(s, &QSlider::sliderPressed, this,
            &PropertiesPaneLevels::onSliderPressed);
    connect(s, &QSlider::sliderReleased, this,
            &PropertiesPaneLevels::onSliderReleased);
  }
  // Spin boxes don't fire sliderPressed/Released; treat each
  // editingFinished as a commit (snapshot taken at the most recent bind /
  // commit).
  for (auto* sp : {inBlackSpin_, gammaSpin_, inWhiteSpin_, outBlackSpin_,
                   outWhiteSpin_}) {
    connect(sp, &QDoubleSpinBox::editingFinished, this,
            &PropertiesPaneLevels::onSpinEditingFinished);
  }

  connect(channelCombo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &PropertiesPaneLevels::onChannelChanged);
  connect(inBlackSlider_, &QSlider::valueChanged, this,
          &PropertiesPaneLevels::onInBlackChanged);
  connect(inBlackSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &PropertiesPaneLevels::onInBlackChanged);
  connect(gammaSlider_, &QSlider::valueChanged, this,
          &PropertiesPaneLevels::onGammaChanged);
  connect(gammaSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &PropertiesPaneLevels::onGammaChanged);
  connect(inWhiteSlider_, &QSlider::valueChanged, this,
          &PropertiesPaneLevels::onInWhiteChanged);
  connect(inWhiteSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &PropertiesPaneLevels::onInWhiteChanged);
  connect(outBlackSlider_, &QSlider::valueChanged, this,
          &PropertiesPaneLevels::onOutBlackChanged);
  connect(outBlackSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &PropertiesPaneLevels::onOutBlackChanged);
  connect(outWhiteSlider_, &QSlider::valueChanged, this,
          &PropertiesPaneLevels::onOutWhiteChanged);
  connect(outWhiteSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &PropertiesPaneLevels::onOutWhiteChanged);
}

void PropertiesPaneLevels::bind(LevelsAdjustment* layer, Histogram4x256 hist) {
  layer_ = layer;
  hist_->setData(hist);
  if (!layer_) return;
  snapshotBefore();
  loadChannelIntoWidgets(LevelsChannel::Composite);
}

void PropertiesPaneLevels::unbind() {
  layer_ = nullptr;
  hist_->setData(Histogram4x256{});
}

void PropertiesPaneLevels::snapshotBefore() {
  if (!layer_) return;
  paramsBefore_ = layer_->allParams();
}

bool PropertiesPaneLevels::paramsDifferFromBefore() const {
  if (!layer_) return false;
  const auto& cur = layer_->allParams();
  for (std::size_t i = 0; i < cur.size(); ++i) {
    const auto& a = cur[i];
    const auto& b = paramsBefore_[i];
    if (a.inBlack != b.inBlack || a.inWhite != b.inWhite ||
        a.gamma != b.gamma || a.outBlack != b.outBlack ||
        a.outWhite != b.outWhite) {
      return true;
    }
  }
  return false;
}

void PropertiesPaneLevels::loadChannelIntoWidgets(LevelsChannel ch) {
  if (!layer_) return;
  loading_ = true;
  activeChannel_ = ch;
  const LevelsParams& p = layer_->params(ch);
  inBlackSpin_->setValue(p.inBlack);
  inBlackSlider_->setValue(floatToSlider01(p.inBlack));
  gammaSpin_->setValue(p.gamma);
  gammaSlider_->setValue(gammaToSlider(p.gamma));
  inWhiteSpin_->setValue(p.inWhite);
  inWhiteSlider_->setValue(floatToSlider01(p.inWhite));
  outBlackSpin_->setValue(p.outBlack);
  outBlackSlider_->setValue(floatToSlider01(p.outBlack));
  outWhiteSpin_->setValue(p.outWhite);
  outWhiteSlider_->setValue(floatToSlider01(p.outWhite));
  hist_->setChannel(static_cast<int>(ch));
  loading_ = false;
}

void PropertiesPaneLevels::pushParamsFromWidgets() {
  if (loading_ || !layer_) return;
  LevelsParams p;
  p.inBlack = static_cast<float>(inBlackSpin_->value());
  p.inWhite = static_cast<float>(inWhiteSpin_->value());
  p.gamma = static_cast<float>(gammaSpin_->value());
  p.outBlack = static_cast<float>(outBlackSpin_->value());
  p.outWhite = static_cast<float>(outWhiteSpin_->value());
  if (p.inWhite <= p.inBlack) p.inWhite = std::min(1.f, p.inBlack + 1e-3f);
  layer_->setParams(activeChannel_, p);
  emit previewChanged();
}

void PropertiesPaneLevels::simulateSliderPressForTest() { onSliderPressed(); }
void PropertiesPaneLevels::simulateSliderReleaseForTest() {
  onSliderReleased();
}

void PropertiesPaneLevels::onSliderPressed() {
  // A drag is starting; snapshot the current layer params so the eventual
  // commit-on-release uses the right `before`.
  if (!layer_) return;
  dragging_ = true;
  snapshotBefore();
}

void PropertiesPaneLevels::onSliderReleased() {
  if (!layer_) return;
  dragging_ = false;
  if (paramsDifferFromBefore()) {
    emit commitRequested(layer_, paramsBefore_, layer_->allParams());
    snapshotBefore();
  }
}

void PropertiesPaneLevels::onSpinEditingFinished() {
  // For keyboard edits — no slider press/release brackets the change. Treat
  // each editingFinished as one commit.
  if (!layer_) return;
  if (paramsDifferFromBefore()) {
    emit commitRequested(layer_, paramsBefore_, layer_->allParams());
    snapshotBefore();
  }
}

void PropertiesPaneLevels::onChannelChanged(int idx) {
  // Channel switch is not an edit — just reload widgets to reflect the new
  // channel's params. snapshotBefore so the next edit's `before` matches
  // the on-load state.
  loadChannelIntoWidgets(static_cast<LevelsChannel>(idx));
  snapshotBefore();
}

void PropertiesPaneLevels::onInBlackChanged() {
  if (loading_) return;
  if (sender() == inBlackSlider_) {
    loading_ = true;
    inBlackSpin_->setValue(sliderToFloat01(inBlackSlider_->value()));
    loading_ = false;
  } else if (sender() == inBlackSpin_) {
    loading_ = true;
    inBlackSlider_->setValue(
        floatToSlider01(static_cast<float>(inBlackSpin_->value())));
    loading_ = false;
  }
  pushParamsFromWidgets();
}

void PropertiesPaneLevels::onInWhiteChanged() {
  if (loading_) return;
  if (sender() == inWhiteSlider_) {
    loading_ = true;
    inWhiteSpin_->setValue(sliderToFloat01(inWhiteSlider_->value()));
    loading_ = false;
  } else if (sender() == inWhiteSpin_) {
    loading_ = true;
    inWhiteSlider_->setValue(
        floatToSlider01(static_cast<float>(inWhiteSpin_->value())));
    loading_ = false;
  }
  pushParamsFromWidgets();
}

void PropertiesPaneLevels::onGammaChanged() {
  if (loading_) return;
  if (sender() == gammaSlider_) {
    loading_ = true;
    gammaSpin_->setValue(sliderToGamma(gammaSlider_->value()));
    loading_ = false;
  } else if (sender() == gammaSpin_) {
    loading_ = true;
    gammaSlider_->setValue(
        gammaToSlider(static_cast<float>(gammaSpin_->value())));
    loading_ = false;
  }
  pushParamsFromWidgets();
}

void PropertiesPaneLevels::onOutBlackChanged() {
  if (loading_) return;
  if (sender() == outBlackSlider_) {
    loading_ = true;
    outBlackSpin_->setValue(sliderToFloat01(outBlackSlider_->value()));
    loading_ = false;
  } else if (sender() == outBlackSpin_) {
    loading_ = true;
    outBlackSlider_->setValue(
        floatToSlider01(static_cast<float>(outBlackSpin_->value())));
    loading_ = false;
  }
  pushParamsFromWidgets();
}

void PropertiesPaneLevels::onOutWhiteChanged() {
  if (loading_) return;
  if (sender() == outWhiteSlider_) {
    loading_ = true;
    outWhiteSpin_->setValue(sliderToFloat01(outWhiteSlider_->value()));
    loading_ = false;
  } else if (sender() == outWhiteSpin_) {
    loading_ = true;
    outWhiteSlider_->setValue(
        floatToSlider01(static_cast<float>(outWhiteSpin_->value())));
    loading_ = false;
  }
  pushParamsFromWidgets();
}

}  // namespace tuxels
