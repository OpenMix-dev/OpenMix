#include "CollapsibleSection.h"

#include <QLayout>
#include <QToolButton>
#include <QVBoxLayout>

namespace OpenMix {

CollapsibleSection::CollapsibleSection(const QString& title, QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_toggle = new QToolButton(this);
    m_toggle->setText(title);
    m_toggle->setCheckable(true);
    m_toggle->setChecked(false);
    m_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_toggle->setArrowType(Qt::RightArrow);
    m_toggle->setAutoRaise(true);
    m_toggle->setFocusPolicy(Qt::TabFocus);
    m_toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_toggle->setCursor(Qt::PointingHandCursor);
    m_toggle->setStyleSheet(
        "QToolButton { border: none; font-weight: bold; text-align: left; padding: 6px 2px; }");

    m_body = new QWidget(this);
    m_body->setVisible(false);

    outer->addWidget(m_toggle);
    outer->addWidget(m_body);

    connect(m_toggle, &QToolButton::toggled, this, [this](bool on) {
        m_body->setVisible(on);
        m_toggle->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
    });
}

void CollapsibleSection::setContentLayout(QLayout* layout) {
    if (QLayout* old = m_body->layout())
        delete old;
    m_body->setLayout(layout);
}

void CollapsibleSection::setExpanded(bool expanded) { m_toggle->setChecked(expanded); }

bool CollapsibleSection::isExpanded() const { return m_toggle->isChecked(); }

} // namespace OpenMix
