#pragma once

#include "protocol/MixerProtocol.h"
#include <QTimer>
#include <QWidget>

namespace OpenMix {

class ConnectionStateWidget : public QWidget {
    Q_OBJECT

  public:
    explicit ConnectionStateWidget(QWidget* parent = nullptr);

    void setState(ConnectionState state);
    ConnectionState state() const { return m_state; }

    void setLatency(int ms);
    int latency() const { return m_latencyMs; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    void paintEvent(QPaintEvent* event) override;

  private slots:
    void onBlinkTimer();

  private:
    QColor stateColor() const;
    QString stateText() const;

    ConnectionState m_state = ConnectionState::Disconnected;
    int m_latencyMs = 0;
    QTimer m_blinkTimer;
    bool m_blinkState = false;
};

} // namespace OpenMix
