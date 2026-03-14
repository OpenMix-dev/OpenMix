#include "LogListModel.h"

namespace OpenMix {

LogListModel::LogListModel(AppLogger* logger, QObject* parent)
    : QAbstractListModel(parent), m_logger(logger) {
    if (m_logger) {
        connect(m_logger, &AppLogger::entryAdded, this, &LogListModel::onEntryAdded);
        connect(m_logger, &AppLogger::entriesBatchAdded, this, &LogListModel::onEntriesBatchAdded);
        connect(m_logger, &AppLogger::logCleared, this, &LogListModel::onLogCleared);

        rebuildFilteredEntries();
    }
}

int LogListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_filteredEntries.size();
}

QVariant LogListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredEntries.size()) {
        return {};
    }

    const LogEntry& entry = m_filteredEntries.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
    case MessageRole:
        return entry.message;
    case TimestampRole:
        return entry.timestamp;
    case LevelRole:
        return static_cast<int>(entry.level);
    case SourceRole:
        return static_cast<int>(entry.source);
    case MetadataRole:
        return entry.metadata;
    case EntryRole:
        return QVariant::fromValue(entry);
    }

    return {};
}

QHash<int, QByteArray> LogListModel::roleNames() const {
    QHash<int, QByteArray> names = QAbstractListModel::roleNames();
    names[TimestampRole] = "timestamp";
    names[LevelRole] = "level";
    names[SourceRole] = "source";
    names[MessageRole] = "message";
    names[MetadataRole] = "metadata";
    names[EntryRole] = "entry";
    return names;
}

void LogListModel::setLevelFilter(LogLevel minLevel) {
    if (m_levelFilter != minLevel) {
        m_levelFilter = minLevel;
        rebuildFilteredEntries();
    }
}

void LogListModel::setSourceFilter(LogSource source, bool enabled) {
    if (m_sourceFilter != source || m_sourceFilterEnabled != enabled) {
        m_sourceFilter = source;
        m_sourceFilterEnabled = enabled;
        rebuildFilteredEntries();
    }
}

void LogListModel::setSearchText(const QString& text) {
    if (m_searchText != text) {
        m_searchText = text;
        if (!text.isEmpty()) {
            m_searchRegex = QRegularExpression(QRegularExpression::escape(text),
                                               QRegularExpression::CaseInsensitiveOption);
        } else {
            m_searchRegex = QRegularExpression();
        }
        rebuildFilteredEntries();
    }
}

LogEntry LogListModel::entryAt(int row) const {
    if (row >= 0 && row < m_filteredEntries.size()) {
        return m_filteredEntries.at(row);
    }
    return {};
}

void LogListModel::refresh() { rebuildFilteredEntries(); }

void LogListModel::onEntryAdded(const LogEntry& entry) {
    if (matchesFilter(entry)) {
        int row = m_filteredEntries.size();
        beginInsertRows(QModelIndex(), row, row);
        m_filteredEntries.append(entry);
        endInsertRows();
    }
}

void LogListModel::onEntriesBatchAdded(const QVector<LogEntry>& entries) {
    QVector<LogEntry> matching;
    for (const LogEntry& entry : entries) {
        if (matchesFilter(entry)) {
            matching.append(entry);
        }
    }

    if (!matching.isEmpty()) {
        int startRow = m_filteredEntries.size();
        beginInsertRows(QModelIndex(), startRow, startRow + matching.size() - 1);
        m_filteredEntries.append(matching);
        endInsertRows();
    }
}

void LogListModel::onLogCleared() {
    beginResetModel();
    m_filteredEntries.clear();
    endResetModel();
}

void LogListModel::rebuildFilteredEntries() {
    beginResetModel();
    m_filteredEntries.clear();

    if (m_logger) {
        QVector<LogEntry> all = m_logger->allEntries();
        for (const LogEntry& entry : all) {
            if (matchesFilter(entry)) {
                m_filteredEntries.append(entry);
            }
        }
    }

    endResetModel();
}

bool LogListModel::matchesFilter(const LogEntry& entry) const {
    // level filter
    if (static_cast<int>(entry.level) < static_cast<int>(m_levelFilter)) {
        return false;
    }

    // source filter
    if (m_sourceFilterEnabled && entry.source != m_sourceFilter) {
        return false;
    }

    // search text filter
    if (!m_searchText.isEmpty()) {
        if (!entry.message.contains(m_searchRegex)) {
            return false;
        }
    }

    return true;
}

} // namespace OpenMix
