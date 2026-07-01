#pragma once

#include <QIcon>
#include <QMap>
#include <QWidget>

class QGridLayout;

namespace OpenMix {

class BubbleButton;

class BubbleBar : public QWidget {
    Q_OBJECT

  public:
    explicit BubbleBar(QWidget* parent = nullptr);
    BubbleButton* addButton(const QString& id, const QString& icon, const QString& tooltip);
    BubbleButton* addButton(const QString& id, const QIcon& icon, const QString& tooltip);
    BubbleButton* button(const QString& id) const;
    void removeButton(const QString& id);
    void setButtonActive(const QString& id, bool active);
    void setButtonBadge(const QString& id, const QString& badge);
    QStringList buttonIds() const;

  signals:
    void buttonClicked(const QString& id, bool checked);

  protected:
    void resizeEvent(QResizeEvent* event) override;

  private:
    void setupUi();
    void updatePosition();

    QGridLayout* m_layout = nullptr;
    QMap<QString, BubbleButton*> m_buttons;
};

} // namespace OpenMix
