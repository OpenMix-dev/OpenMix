#pragma once

#include "core/AppLogger.h"
#include <QAbstractListModel>
#include <QRegularExpression>

namespace OpenMix {

class LogListModel : public QAbstractListModel {
    Q_OBJECT

  public:
    enum Roles {
        TimestampRole = Qt::UserRole + 1,
        LevelRole,
        SourceRole,
        MessageRole,
        MetadataRole,
        EntryRole
    };

    explicit LogListModel(AppLogger* logger, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // filtering
    void setLevelFilter(LogLevel minLevel);
    LogLevel levelFilter() const { return m_levelFilter; }

    void setSourceFilter(LogSource source, bool enabled);
    bool sourceFilterEnabled() const { return m_sourceFilterEnabled; }
    LogSource sourceFilter() const { return m_sourceFilter; }

    void setSearchText(const QString& text);
    QString searchText() const { return m_searchText; }

    // get entry at index (filtered)
    LogEntry entryAt(int row) const;

    // refresh from logger
    void refresh();

  public slots:
    void onEntryAdded(const LogEntry& entry);
    void onEntriesBatchAdded(const QVector<LogEntry>& entries);
    void onLogCleared();

  private:
    void rebuildFilteredEntries();
    bool matchesFilter(const LogEntry& entry) const;

    AppLogger* m_logger;

    QVector<LogEntry> m_filteredEntries;

    LogLevel m_levelFilter = LogLevel::Debug;
    LogSource m_sourceFilter = LogSource::System;
    bool m_sourceFilterEnabled = false;
    QString m_searchText;
    QRegularExpression m_searchRegex;
};

} // namespace OpenMix
