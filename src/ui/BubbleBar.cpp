#include "BubbleBar.h"
#include "BubbleButton.h"
#include "theme/Theme.h"

#include <QGridLayout>
#include <QResizeEvent>

namespace {
constexpr int kBubbleColumns = 64; // keep the icons on a single row
}

namespace OpenMix {

BubbleBar::BubbleBar(QWidget* parent) : QWidget(parent) {
    setObjectName(Theme::ObjectNames::BubbleBar);
    setupUi();
}

void BubbleBar::setupUi() {
    m_layout = new QGridLayout(this);
    m_layout->setContentsMargins(Theme::SpacingXS, Theme::SpacingXS, Theme::SpacingXS,
                                 Theme::SpacingXS);
    m_layout->setSpacing(Theme::SpacingXXS);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

BubbleButton* BubbleBar::addButton(const QString& id, const QString& icon, const QString& tooltip) {
    if (m_buttons.contains(id)) {
        return m_buttons[id];
    }

    const int idx = m_buttons.size();
    BubbleButton* button = new BubbleButton(icon, tooltip, this);
    m_buttons[id] = button;
    m_layout->addWidget(button, idx / kBubbleColumns, idx % kBubbleColumns);

    connect(button, &QPushButton::clicked,
            [this, id](bool checked) { emit buttonClicked(id, checked); });

    adjustSize();
    return button;
}

BubbleButton* BubbleBar::addButton(const QString& id, const QIcon& icon, const QString& tooltip) {
    if (m_buttons.contains(id)) {
        return m_buttons[id];
    }

    const int idx = m_buttons.size();
    BubbleButton* button = new BubbleButton(icon, tooltip, this);
    m_buttons[id] = button;
    m_layout->addWidget(button, idx / kBubbleColumns, idx % kBubbleColumns);

    connect(button, &QPushButton::clicked,
            [this, id](bool checked) { emit buttonClicked(id, checked); });

    adjustSize();
    return button;
}

BubbleButton* BubbleBar::button(const QString& id) const { return m_buttons.value(id, nullptr); }

void BubbleBar::removeButton(const QString& id) {
    if (BubbleButton* btn = m_buttons.take(id)) {
        m_layout->removeWidget(btn);
        btn->deleteLater();
        adjustSize();
    }
}

void BubbleBar::setButtonActive(const QString& id, bool active) {
    if (BubbleButton* btn = m_buttons.value(id)) {
        btn->setActive(active);
        btn->setChecked(active);
    }
}

void BubbleBar::setButtonBadge(const QString& id, const QString& badge) {
    if (BubbleButton* btn = m_buttons.value(id)) {
        btn->setBadge(badge);
    }
}

QStringList BubbleBar::buttonIds() const { return m_buttons.keys(); }

void BubbleBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updatePosition();
}

void BubbleBar::updatePosition() { adjustSize(); }

} // namespace OpenMix
