#pragma once

#include <QStyledItemDelegate>

namespace OpenMix {

class LogItemDelegate : public QStyledItemDelegate {
    Q_OBJECT

  public:
    explicit LogItemDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

  private:
    struct LevelStyle {
        QColor textColor;
        QColor badgeColor;
        QColor badgeTextColor;
    };

    LevelStyle styleForLevel(int level) const;
    QString badgeText(int level) const;
    QString sourceText(int source) const;
};

} // namespace OpenMix
