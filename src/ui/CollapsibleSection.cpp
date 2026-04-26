#include "ui/CollapsibleSection.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>

namespace tuxels {

CollapsibleSection::CollapsibleSection(const QString& name,
                                       const QString& shortcut,
                                       QWidget* parent)
    : QWidget(parent) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  header_ = new QFrame(this);
  header_->setFrameShape(QFrame::NoFrame);
  header_->setCursor(Qt::PointingHandCursor);
  header_->setAutoFillBackground(true);
  header_->installEventFilter(this);

  auto* headerLayout = new QHBoxLayout(header_);
  headerLayout->setContentsMargins(6, 4, 6, 4);
  headerLayout->setSpacing(6);

  chevron_ = new QLabel(QStringLiteral("▾"), header_);
  chevron_->setFixedWidth(14);
  chevron_->setAlignment(Qt::AlignCenter);
  chevron_->setCursor(Qt::PointingHandCursor);
  chevron_->installEventFilter(this);

  nameLabel_ = new QLabel(QStringLiteral("<b>%1</b>").arg(name.toHtmlEscaped()),
                          header_);

  shortcutLabel_ = new QLabel(shortcut, header_);
  shortcutLabel_->setStyleSheet("color: rgba(200, 200, 200, 160);");
  shortcutLabel_->setVisible(!shortcut.isEmpty());

  headerLayout->addWidget(chevron_);
  headerLayout->addWidget(nameLabel_);
  headerLayout->addStretch(1);
  headerLayout->addWidget(shortcutLabel_);

  root->addWidget(header_);

  bodyContainer_ = new QWidget(this);
  auto* bodyLayout = new QVBoxLayout(bodyContainer_);
  bodyLayout->setContentsMargins(8, 4, 8, 8);
  bodyLayout->setSpacing(4);
  root->addWidget(bodyContainer_);

  updateChevronGlyph();
  updateHeaderStyle();
}

void CollapsibleSection::setBody(QWidget* body) {
  if (body_ == body) return;
  if (body_) {
    bodyContainer_->layout()->removeWidget(body_);
    body_->setParent(nullptr);
    body_->deleteLater();
  }
  body_ = body;
  if (body_) {
    body_->setParent(bodyContainer_);
    bodyContainer_->layout()->addWidget(body_);
  }
}

void CollapsibleSection::setExpanded(bool expanded) {
  if (expanded_ == expanded) return;
  expanded_ = expanded;
  bodyContainer_->setVisible(expanded_);
  updateChevronGlyph();
}

void CollapsibleSection::setActive(bool active) {
  if (active_ == active) return;
  active_ = active;
  updateHeaderStyle();
}

void CollapsibleSection::simulateHeaderClick() { emit headerClicked(); }

void CollapsibleSection::simulateChevronClick() {
  setExpanded(!expanded_);
  emit chevronClicked();
}

void CollapsibleSection::updateChevronGlyph() {
  chevron_->setText(expanded_ ? QStringLiteral("▾") : QStringLiteral("▸"));
}

void CollapsibleSection::updateHeaderStyle() {
  if (active_) {
    header_->setStyleSheet(
        "QFrame { background-color: rgba(60, 120, 200, 90); "
        "border-left: 3px solid rgb(60, 160, 240); }");
  } else {
    header_->setStyleSheet(
        "QFrame { background-color: rgba(255, 255, 255, 12); "
        "border-left: 3px solid transparent; }");
  }
}

bool CollapsibleSection::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() != QEvent::MouseButtonPress) {
    return QWidget::eventFilter(watched, event);
  }
  auto* me = static_cast<QMouseEvent*>(event);
  if (me->button() != Qt::LeftButton) {
    return QWidget::eventFilter(watched, event);
  }
  // Chevron is a child of header — its eventFilter fires first because the
  // event is delivered to the topmost child it hits. Returning true consumes
  // the event so the header's branch below never sees a chevron click.
  if (watched == chevron_) {
    setExpanded(!expanded_);
    emit chevronClicked();
    return true;
  }
  if (watched == header_) {
    emit headerClicked();
    return true;
  }
  return QWidget::eventFilter(watched, event);
}

}  // namespace tuxels
