#pragma once

#include <QWidget>

#include "core/Histogram.h"
#include "layers/CurvesAdjustment.h"

class QComboBox;

namespace tuxels {

class CurveEditor;

// Non-modal Curves editor for the Properties dock (M4-S3). Mirrors
// `PropertiesPaneLevels`: channel combo + reusable `CurveEditor` widget
// (lifted out of CurvesDialog) + histogram backdrop driven by the editor.
//
// Snapshot/commit discipline:
//   - bind() snapshots `paramsBefore_` (per-channel point arrays) and
//     loads the active channel into the editor.
//   - Editor's `previewChanged` mutates the bound layer's points for the
//     active channel and emits the pane's `previewChanged`.
//   - Editor's `interactionEnded` (= mouse release after drag/add OR
//     right-click after remove) checks if any channel's points differ
//     from `paramsBefore_`, emits `commitRequested(layer, before, after)`,
//     re-snapshots so the next gesture's `before` is correct.
//   - Channel switch reloads the editor with the new channel's points
//     and re-snapshots — no commit emitted.
class PropertiesPaneCurves : public QWidget {
  Q_OBJECT

 public:
  explicit PropertiesPaneCurves(QWidget* parent = nullptr);

  void bind(CurvesAdjustment* layer, Histogram4x256 hist);
  void unbind();

  CurvesAdjustment* boundLayer() const { return layer_; }
  const CurvesAdjustment::PointsArray& paramsBefore() const {
    return paramsBefore_;
  }

  // Test hooks
  CurveEditor* editorForTest() const { return editor_; }
  QComboBox* channelComboForTest() const { return channelCombo_; }

 signals:
  void previewChanged();
  void commitRequested(CurvesAdjustment* layer,
                       CurvesAdjustment::PointsArray before,
                       CurvesAdjustment::PointsArray after);

 private slots:
  void onChannelChanged(int idx);
  void onEditorPreviewChanged();
  void onEditorInteractionEnded();

 private:
  void snapshotBefore();
  void loadChannel(CurvesChannel ch);
  bool pointsDifferFromBefore() const;

  CurvesAdjustment* layer_ = nullptr;
  CurvesAdjustment::PointsArray paramsBefore_{};
  CurvesChannel activeChannel_ = CurvesChannel::Composite;

  QComboBox* channelCombo_ = nullptr;
  CurveEditor* editor_ = nullptr;
};

}  // namespace tuxels
