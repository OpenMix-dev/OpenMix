#include "ChannelStripPanel.h"

#include "app/Application.h"
#include "core/Actor.h"
#include "core/ActorProfileLibrary.h"
#include "core/ChannelMonitor.h"
#include "core/Show.h"
#include "theme/Theme.h"

#include <QGridLayout>
#include <QLabel>

namespace OpenMix {

namespace {
constexpr int kColumns = 12; // wrap the channel tiles into rows of this many
}

ChannelStripPanel::ChannelStripPanel(Application* app, QWidget* parent)
    : QWidget(parent), m_app(app) {
    m_grid = new QGridLayout(this);
    m_grid->setContentsMargins(Theme::SpacingS, Theme::SpacingS, Theme::SpacingS, Theme::SpacingS);
    m_grid->setSpacing(Theme::SpacingXS);

    m_emptyHint = new QLabel(tr("No actor channels assigned. Set them up in Actor Setup."), this);
    m_emptyHint->setStyleSheet(QString("color: %1;").arg(Theme::Colors::TextTertiary));
    m_grid->addWidget(m_emptyHint, 0, 0);

    if (m_app && m_app->show() && m_app->show()->actorProfileLibrary())
        connect(m_app->show()->actorProfileLibrary(), &ActorProfileLibrary::changed, this,
                &ChannelStripPanel::rebuild);
    if (m_app && m_app->channelMonitor())
        connect(m_app->channelMonitor(), &ChannelMonitor::channelStateChanged, this,
                &ChannelStripPanel::onChannelStateChanged);

    rebuild();
}

void ChannelStripPanel::rebuild() {
    for (QLabel* tile : m_tiles)
        tile->deleteLater();
    m_tiles.clear();

    if (!m_app || !m_app->show() || !m_app->show()->actorProfileLibrary())
        return;

    const QList<Actor>& actors = m_app->show()->actorProfileLibrary()->actors();
    m_emptyHint->setVisible(actors.isEmpty());

    int i = 0;
    for (const Actor& actor : actors) {
        const int channel = actor.channel();
        if (channel <= 0)
            continue;
        auto* tile = new QLabel(this);
        tile->setAlignment(Qt::AlignCenter);
        tile->setFixedSize(84, 44);
        const QString name = actor.name().isEmpty() ? tr("ch %1").arg(channel) : actor.name();
        tile->setText(QString("<b>%1</b><br/><small>%2</small>").arg(channel).arg(name.toHtmlEscaped()));
        const int state = m_app->channelMonitor()
                              ? static_cast<int>(m_app->channelMonitor()->channelState(channel))
                              : 0;
        styleTile(tile, state);
        m_grid->addWidget(tile, i / kColumns, i % kColumns);
        m_tiles.insert(channel, tile);
        ++i;
    }
}

void ChannelStripPanel::onChannelStateChanged(int channel, int state) {
    if (QLabel* tile = m_tiles.value(channel))
        styleTile(tile, state);
}

void ChannelStripPanel::styleTile(QLabel* tile, int state) {
    // Normal / Silent / Clipping
    const char* border = Theme::Colors::AccentGreen;
    if (state == static_cast<int>(ChannelState::Silent))
        border = Theme::Colors::AccentBlue;
    else if (state == static_cast<int>(ChannelState::Clipping))
        border = Theme::Colors::AccentRed;
    tile->setStyleSheet(QString("QLabel { background: %1; border: 2px solid %2; border-radius: 4px; "
                                "color: %3; padding: 2px; }")
                            .arg(Theme::Colors::BgElevated)
                            .arg(border)
                            .arg(Theme::Colors::TextPrimary));
}

} // namespace OpenMix
