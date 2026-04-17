#include "LogItemDelegate.h"
#include "LogListModel.h"
#include "core/AppLogger.h"
#include <QApplication>
#include <QDateTime>
#include <QPainter>

namespace OpenMix {

LogItemDelegate::LogItemDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void LogItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const {
    painter->save();

    // draw background
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, QColor(59, 130, 246, 102)); // rgba(59, 130, 246, 0.4)
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(option.rect, QColor(34, 37, 42)); // #22252a
    }

    QDateTime timestamp = index.data(LogListModel::TimestampRole).toDateTime();
    int level = index.data(LogListModel::LevelRole).toInt();
    int source = index.data(LogListModel::SourceRole).toInt();
    QString message = index.data(LogListModel::MessageRole).toString();

    LevelStyle style = styleForLevel(level);

    int x = option.rect.x() + 12;
    int y = option.rect.y();
    int height = option.rect.height();

    // monospace font for timestamp
    QFont monoFont("JetBrains Mono", 10);
    monoFont.setStyleHint(QFont::Monospace);
    if (!QFontInfo(monoFont).fixedPitch()) {
        monoFont = QFont("Consolas", 10);
        monoFont.setStyleHint(QFont::Monospace);
    }

    // timestamp
    painter->setFont(monoFont);
    painter->setPen(QColor(107, 114, 128)); // #6b7280 - muted
    QString timeStr = timestamp.toString("HH:mm:ss.zzz");
    QRect timeRect(x, y, 90, height);
    painter->drawText(timeRect, Qt::AlignLeft | Qt::AlignVCenter, timeStr);
    x += 98;

    // level badge
    QString badge = badgeText(level);
    QFont badgeFont = option.font;
    badgeFont.setPointSize(9);
    badgeFont.setBold(true);
    painter->setFont(badgeFont);
    QFontMetrics badgeFm(badgeFont);
    int badgeWidth = badgeFm.horizontalAdvance(badge) + 12;
    int badgeHeight = 18;
    int badgeY = y + (height - badgeHeight) / 2;

    // draw badge background
    QRect badgeRect(x, badgeY, badgeWidth, badgeHeight);
    painter->setPen(Qt::NoPen);
    painter->setBrush(style.badgeColor);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->drawRoundedRect(badgeRect, 4, 4);

    // draw badge text
    painter->setPen(style.badgeTextColor);
    painter->drawText(badgeRect, Qt::AlignCenter, badge);
    x += badgeWidth + 8;

    // source label
    QString srcStr = sourceText(source);
    QFont srcFont = option.font;
    srcFont.setPointSize(10);
    painter->setFont(srcFont);
    painter->setPen(QColor(156, 163, 175)); // #9ca3af - secondary
    QRect srcRect(x, y, 80, height);
    painter->drawText(srcRect, Qt::AlignLeft | Qt::AlignVCenter, srcStr);
    x += 88;

    // message
    QFont msgFont = option.font;
    msgFont.setPointSize(11);
    painter->setFont(msgFont);
    painter->setPen(style.textColor);
    int messageWidth = option.rect.right() - x - 12;
    QRect msgRect(x, y, messageWidth, height);
    QString elidedMsg = painter->fontMetrics().elidedText(message, Qt::ElideRight, messageWidth);
    painter->drawText(msgRect, Qt::AlignLeft | Qt::AlignVCenter, elidedMsg);

    painter->restore();
}

QSize LogItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                [[maybe_unused]] const QModelIndex& index) const {
    return QSize(option.rect.width(), 36);
}

LogItemDelegate::LevelStyle LogItemDelegate::styleForLevel(int level) const {
    LevelStyle style;

    switch (static_cast<LogLevel>(level)) {
    case LogLevel::Debug:
        style.textColor = QColor(156, 163, 175);  // #9ca3af
        style.badgeColor = QColor(107, 114, 128); // #6b7280
        style.badgeTextColor = QColor(255, 255, 255);
        break;
    case LogLevel::Info:
        style.textColor = QColor(240, 241, 243); // #f0f1f3
        style.badgeColor = QColor(59, 130, 246); // #3b82f6
        style.badgeTextColor = QColor(255, 255, 255);
        break;
    case LogLevel::Warning:
        style.textColor = QColor(245, 158, 11);  // #f59e0b
        style.badgeColor = QColor(245, 158, 11); // #f59e0b
        style.badgeTextColor = QColor(0, 0, 0);
        break;
    case LogLevel::Error:
        style.textColor = QColor(239, 68, 68);  // #ef4444
        style.badgeColor = QColor(239, 68, 68); // #ef4444
        style.badgeTextColor = QColor(255, 255, 255);
        break;
    case LogLevel::Critical:
        style.textColor = QColor(255, 255, 255);
        style.badgeColor = QColor(185, 28, 28); // #b91c1c - darker red
        style.badgeTextColor = QColor(255, 255, 255);
        break;
    }

    return style;
}

QString LogItemDelegate::badgeText(int level) const {
    switch (static_cast<LogLevel>(level)) {
    case LogLevel::Debug:
        return "DBG";
    case LogLevel::Info:
        return "INF";
    case LogLevel::Warning:
        return "WRN";
    case LogLevel::Error:
        return "ERR";
    case LogLevel::Critical:
        return "CRT";
    }
    return "???";
}

QString LogItemDelegate::sourceText(int source) const {
    switch (static_cast<LogSource>(source)) {
    case LogSource::Connection:
        return "Connection";
    case LogSource::Protocol:
        return "Protocol";
    case LogSource::Playback:
        return "Playback";
    case LogSource::UI:
        return "UI";
    case LogSource::System:
        return "System";
    case LogSource::MIDI:
        return "MIDI";
    case LogSource::Discovery:
        return "Discovery";
    }
    return "Unknown";
}

} // namespace OpenMix
