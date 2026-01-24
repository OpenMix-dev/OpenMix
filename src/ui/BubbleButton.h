#pragma once

#include <QIcon>
#include <QPushButton>

namespace OpenMix {

class BubbleButton : public QPushButton {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)

  public:
    explicit BubbleButton(const QString& icon, const QString& tooltip, QWidget* parent = nullptr);
    explicit BubbleButton(const QIcon& icon, const QString& tooltip, QWidget* parent = nullptr);
    void setBadge(const QString& text);
    QString badge() const { return m_badgeText; }
    void setActive(bool active);
    bool isActive() const { return m_active; }
    void setIcon(const QString& icon);
    void setButtonIcon(const QIcon& icon);

  signals:
    void activeChanged(bool active);

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    QString m_badgeText;
    QString m_iconText;
    bool m_active = false;
};

} // namespace OpenMix
