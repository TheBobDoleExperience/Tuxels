#pragma once

#include <QDialog>
#include <array>
#include <vector>

#include "core/Histogram.h"
#include "layers/CurvesAdjustment.h"

class QComboBox;

namespace tuxels {

class CurveEditor;

// Modal editor for a CurvesAdjustment. Snapshots the on-open points per
// channel so Cancel is a true no-op on the layer; mutates the layer in
// place during the dialog so the caller's `previewChanged` slot can
// request live recomposites.
class CurvesDialog : public QDialog {
  Q_OBJECT

 public:
  CurvesDialog(CurvesAdjustment* layer, Histogram4x256 hist,
               QWidget* parent = nullptr);

  const CurvesAdjustment::PointsArray& pointsBefore() const { return before_; }

 signals:
  void previewChanged();

 private slots:
  void onChannelChanged(int idx);
  void onEditorPointsChanged();

 private:
  void reject() override;

  CurvesAdjustment* layer_;
  CurvesAdjustment::PointsArray before_{};
  CurvesChannel activeChannel_ = CurvesChannel::Composite;

  QComboBox* channelCombo_ = nullptr;
  CurveEditor* editor_ = nullptr;
};

}  // namespace tuxels
