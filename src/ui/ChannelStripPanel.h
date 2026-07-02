#pragma once

#include <QHash>
#include <QWidget>

class QGridLayout;
class QLabel;

namespace OpenMix {

class Application;

// A horizontally-wrapping strip of per-channel status tiles beneath the cue
// grid: one tile per assigned actor channel, showing the channel number and
// name, coloured by the channel monitor's silence/clip state.
class ChannelStripPanel : public QWidget {
    Q_OBJECT

  public:
    explicit ChannelStripPanel(Application* app, QWidget* parent = nullptr);

  private slots:
    void rebuild();
    void onChannelStateChanged(int channel, int state);

  private:
    void styleTile(QLabel* tile, int state);

    Application* m_app;
    QGridLayout* m_grid;
    QLabel* m_emptyHint = nullptr;
    QHash<int, QLabel*> m_tiles; // channel -> tile
};

} // namespace OpenMix
