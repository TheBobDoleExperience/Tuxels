#include "ui/LevelsDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>

namespace tuxels {

// Lightweight bucket-bar histogram. One channel at a time (cheaper to draw
// and matches the dialog's combo selection). The widget owns a reference to
// the full 4×256 snapshot; `setChannel` just re-picks which row to render.
class LevelsHistogramView : public QWidget {
 public:
  LevelsHistogramView(Histogram4x256 hist, QWidget* parent = nullptr)
      : QWidget(parent), hist_(hist) {
    setMinimumSize(256, 96);
    setAutoFillBackground(true);
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

    // Luma row (index 3) is what the composite selector paints; R/G/B pick
    // their own channel. Map our LevelsChannel ordinal onto histogram rows:
    // Composite(0) → luma(3); R(1)→R(0); G(2)→G(1); B(3)→B(2).
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
  Histogram4x256 hist_;
  int channel_ = 0;
};

namespace {

// Sliders carry integer positions; spin boxes carry the floats. We pick 1000
// steps for black/white/output (0..1 range) and 1000 steps for gamma (0.1..9.99).
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

LevelsDialog::LevelsDialog(LevelsAdjustment* layer, Histogram4x256 hist,
                           QWidget* parent)
    : QDialog(parent), layer_(layer), before_(layer->allParams()) {
  setWindowTitle(tr("Levels"));
  setModal(true);

  auto* layout = new QVBoxLayout(this);

  channelCombo_ = new QComboBox(this);
  channelCombo_->addItem(tr("Composite"));
  channelCombo_->addItem(tr("Red"));
  channelCombo_->addItem(tr("Green"));
  channelCombo_->addItem(tr("Blue"));
  layout->addWidget(channelCombo_);

  hist_ = new LevelsHistogramView(hist, this);
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

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);

  connect(channelCombo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &LevelsDialog::onChannelChanged);
  connect(inBlackSlider_, &QSlider::valueChanged, this,
          &LevelsDialog::onInBlackChanged);
  connect(inBlackSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &LevelsDialog::onInBlackChanged);
  connect(gammaSlider_, &QSlider::valueChanged, this,
          &LevelsDialog::onGammaChanged);
  connect(gammaSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &LevelsDialog::onGammaChanged);
  connect(inWhiteSlider_, &QSlider::valueChanged, this,
          &LevelsDialog::onInWhiteChanged);
  connect(inWhiteSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &LevelsDialog::onInWhiteChanged);
  connect(outBlackSlider_, &QSlider::valueChanged, this,
          &LevelsDialog::onOutBlackChanged);
  connect(outBlackSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &LevelsDialog::onOutBlackChanged);
  connect(outWhiteSlider_, &QSlider::valueChanged, this,
          &LevelsDialog::onOutWhiteChanged);
  connect(outWhiteSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &LevelsDialog::onOutWhiteChanged);

  loadChannelIntoWidgets(LevelsChannel::Composite);
}

void LevelsDialog::loadChannelIntoWidgets(LevelsChannel ch) {
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

void LevelsDialog::pushParamsFromWidgets() {
  if (loading_) return;
  LevelsParams p;
  p.inBlack = static_cast<float>(inBlackSpin_->value());
  p.inWhite = static_cast<float>(inWhiteSpin_->value());
  p.gamma = static_cast<float>(gammaSpin_->value());
  p.outBlack = static_cast<float>(outBlackSpin_->value());
  p.outWhite = static_cast<float>(outWhiteSpin_->value());
  // Prevent degenerate input range (inWhite must stay strictly greater than
  // inBlack). Nudge inWhite above inBlack silently rather than surfacing a
  // validator error — matches how PS handles the crossing slider case.
  if (p.inWhite <= p.inBlack) p.inWhite = std::min(1.f, p.inBlack + 1e-3f);
  layer_->setParams(activeChannel_, p);
  emit previewChanged();
}

void LevelsDialog::onChannelChanged(int idx) {
  loadChannelIntoWidgets(static_cast<LevelsChannel>(idx));
}

void LevelsDialog::onInBlackChanged() {
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

void LevelsDialog::onInWhiteChanged() {
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

void LevelsDialog::onGammaChanged() {
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

void LevelsDialog::onOutBlackChanged() {
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

void LevelsDialog::onOutWhiteChanged() {
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

void LevelsDialog::reject() {
  // Cancel: restore the params snapshotted at open so the live-preview
  // edits don't leak onto the layer. The caller sees a no-op.
  layer_->setAllParams(before_);
  emit previewChanged();
  QDialog::reject();
}

}  // namespace tuxels
