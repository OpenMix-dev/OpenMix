#include "CueTableModel.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "theme/Theme.h"
#include <QBrush>
#include <QDataStream>
#include <QFont>
#include <QIODevice>

namespace OpenMix {

const QString CueTableModel::s_mimeType = QStringLiteral("application/x-openmix-cue-index");

CueTableModel::CueTableModel(CueList* cueList, QObject* parent)
    : QAbstractTableModel(parent), m_cueList(cueList) {
    if (m_cueList) {
        connect(m_cueList, &CueList::cueAdded, this, &CueTableModel::onCueAdded);
        connect(m_cueList, &CueList::cueRemoved, this, &CueTableModel::onCueRemoved);
        connect(m_cueList, &CueList::cueUpdated, this, &CueTableModel::onCueUpdated);
        connect(m_cueList, &CueList::cueMoved, this, &CueTableModel::onCueMoved);
        connect(m_cueList, &CueList::listCleared, this, &CueTableModel::onListCleared);
        connect(m_cueList, &CueList::listLoaded, this, &CueTableModel::onListLoaded);
    }
}

int CueTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() || !m_cueList)
        return 0;
    return m_cueList->count();
}

int CueTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return ColCount;
}

QVariant CueTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !m_cueList)
        return QVariant();

    int row = index.row();
    int col = index.column();

    if (row < 0 || row >= m_cueList->count())
        return QVariant();

    const Cue& cue = m_cueList->at(row);

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (col) {
        case ColNumber:
            return QString::number(cue.number(), 'f', 1);
        case ColName:
            return cue.name();
        case ColType:
            return cueTypeToString(cue.type());
        case ColGroup:
            return cue.group();
        case ColTags:
            return cue.tags().join(", ");
        case ColNotes:
            return cue.notes();
        }
    }

    if (role == Qt::BackgroundRole) {
        if (row == m_currentIndex) {
            return QBrush(Theme::withAlpha(Theme::Colors::AccentGreen, 45));
        } else if (row == m_standbyIndex) {
            return QBrush(Theme::withAlpha(Theme::Colors::AccentAmber, 40));
        }
    }

    if (role == Qt::ForegroundRole) {
        if (row == m_currentIndex) {
            return QBrush(Theme::color(Theme::Colors::TextPrimary));
        }
    }

    if (role == Qt::FontRole) {
        if (row == m_currentIndex) {
            QFont font;
            font.setBold(true);
            return font;
        }
    }

    if (role == Qt::ToolTipRole) {
        QString tip = QString("Cue %1: %2").arg(cue.number(), 0, 'f', 1).arg(cue.name());
        if (!cue.notes().isEmpty()) {
            tip += "\n" + cue.notes();
        }
        if (cue.autoFollow()) {
            tip += QString("\nAuto-follow: %1s delay").arg(cue.autoFollowDelay());
        }
        return tip;
    }

    // store cue ID for drag-drop
    if (role == Qt::UserRole) {
        return cue.id();
    }

    return QVariant();
}

QVariant CueTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QVariant();
    }

    switch (section) {
    case ColNumber:
        return tr("Q#");
    case ColName:
        return tr("Name");
    case ColType:
        return tr("Type");
    case ColGroup:
        return tr("Group");
    case ColTags:
        return tr("Tags");
    case ColNotes:
        return tr("Notes");
    }
    return QVariant();
}

Qt::ItemFlags CueTableModel::flags(const QModelIndex& index) const {
    Qt::ItemFlags defaultFlags = QAbstractTableModel::flags(index);

    if (!index.isValid()) {
        return defaultFlags | Qt::ItemIsDropEnabled;
    }

    // all rows are draggable & drop targets
    Qt::ItemFlags flags = defaultFlags | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;

    // enable editing for certain columns
    int col = index.column();
    if (col == ColGroup || col == ColTags || col == ColNotes || col == ColName) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

bool CueTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole || !m_cueList) {
        return false;
    }

    int row = index.row();
    int col = index.column();

    if (row < 0 || row >= m_cueList->count())
        return false;

    Cue cue = m_cueList->at(row);
    QVariant oldValue;

    switch (col) {
    case ColName:
        oldValue = cue.name();
        cue.setName(value.toString());
        break;
    case ColGroup:
        oldValue = cue.group();
        cue.setGroup(value.toString());
        break;
    case ColTags: {
        oldValue = cue.tags().join(", ");
        QStringList tags = value.toString().split(",", Qt::SkipEmptyParts);
        for (QString& tag : tags) {
            tag = tag.trimmed();
        }
        cue.setTags(tags);
        break;
    }
    case ColNotes:
        oldValue = cue.notes();
        cue.setNotes(value.toString());
        break;
    default:
        return false;
    }

    // emit signal for undo system to intercept
    emit cueEditRequested(row, col, oldValue, value);

    // actually update the cue
    m_cueList->updateCue(row, cue);

    return true;
}

Qt::DropActions CueTableModel::supportedDropActions() const { return Qt::MoveAction; }

QStringList CueTableModel::mimeTypes() const { return QStringList() << s_mimeType; }

QMimeData* CueTableModel::mimeData(const QModelIndexList& indexes) const {
    if (indexes.isEmpty())
        return nullptr;

    QMimeData* mimeData = new QMimeData();
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    // get unique rows
    QSet<int> rows;
    for (const QModelIndex& index : indexes) {
        if (index.isValid()) {
            rows.insert(index.row());
        }
    }

    // serialize row indices
    stream << rows.size();
    for (int row : rows) {
        stream << row;
    }

    mimeData->setData(s_mimeType, encodedData);
    return mimeData;
}

bool CueTableModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                                    int column, const QModelIndex& parent) const {
    Q_UNUSED(column);
    Q_UNUSED(parent);

    if (!data->hasFormat(s_mimeType))
        return false;
    if (action != Qt::MoveAction)
        return false;
    if (row < 0)
        return false;

    return true;
}

bool CueTableModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                 const QModelIndex& parent) {
    Q_UNUSED(column);
    Q_UNUSED(parent);

    if (!canDropMimeData(data, action, row, column, parent))
        return false;
    if (!m_cueList)
        return false;

    QByteArray encodedData = data->data(s_mimeType);
    QDataStream stream(&encodedData, QIODevice::ReadOnly);

    int count;
    stream >> count;

    if (count != 1) {
        // only support single row drag for now
        return false;
    }

    int fromIndex;
    stream >> fromIndex;

    // adjust target row if dropping after the source
    int toIndex = row;
    if (fromIndex < toIndex) {
        toIndex--;
    }

    if (fromIndex == toIndex || fromIndex < 0 || toIndex < 0) {
        return false;
    }

    // emit signal for undo system
    emit cueReordered(fromIndex, toIndex);

    return true;
}

void CueTableModel::setCurrentCueIndex(int index) {
    int oldIndex = m_currentIndex;
    m_currentIndex = index;

    // refresh old & new rows
    if (oldIndex >= 0 && oldIndex < rowCount()) {
        emit dataChanged(this->index(oldIndex, 0), this->index(oldIndex, ColCount - 1));
    }
    if (index >= 0 && index < rowCount()) {
        emit dataChanged(this->index(index, 0), this->index(index, ColCount - 1));
    }
}

void CueTableModel::setStandbyCueIndex(int index) {
    int oldIndex = m_standbyIndex;
    m_standbyIndex = index;

    // refresh old & new rows
    if (oldIndex >= 0 && oldIndex < rowCount()) {
        emit dataChanged(this->index(oldIndex, 0), this->index(oldIndex, ColCount - 1));
    }
    if (index >= 0 && index < rowCount()) {
        emit dataChanged(this->index(index, 0), this->index(index, ColCount - 1));
    }
}

const Cue* CueTableModel::cueAt(int row) const {
    if (!m_cueList || row < 0 || row >= m_cueList->count()) {
        return nullptr;
    }
    return &m_cueList->at(row);
}

void CueTableModel::onCueAdded(int index) {
    beginInsertRows(QModelIndex(), index, index);
    endInsertRows();
}

void CueTableModel::onCueRemoved(int index) {
    beginRemoveRows(QModelIndex(), index, index);
    endRemoveRows();

    // adjust highlight indices
    if (m_currentIndex == index) {
        m_currentIndex = -1;
    } else if (m_currentIndex > index) {
        m_currentIndex--;
    }

    if (m_standbyIndex == index) {
        m_standbyIndex = -1;
    } else if (m_standbyIndex > index) {
        m_standbyIndex--;
    }
}

void CueTableModel::onCueUpdated(int index) {
    emit dataChanged(this->index(index, 0), this->index(index, ColCount - 1));
}

void CueTableModel::onCueMoved(int from, int to) {
    // use moveRows from Qt to update the view
    // CueList already moved the item, this just notifies the view
    // so selections/scroll position/other visual states stay in sync
    //
    // destinationChild is the row before the moved rows, so
    // moving down → to + 1, moving up → to
    int destRow = (from < to) ? to + 1 : to;

    beginMoveRows(QModelIndex(), from, from, QModelIndex(), destRow);
    endMoveRows();

    // adjust highlight indices
    if (m_currentIndex == from) {
        m_currentIndex = to;
    } else if (from < m_currentIndex && m_currentIndex <= to) {
        m_currentIndex--;
    } else if (to <= m_currentIndex && m_currentIndex < from) {
        m_currentIndex++;
    }

    if (m_standbyIndex == from) {
        m_standbyIndex = to;
    } else if (from < m_standbyIndex && m_standbyIndex <= to) {
        m_standbyIndex--;
    } else if (to <= m_standbyIndex && m_standbyIndex < from) {
        m_standbyIndex++;
    }
}

void CueTableModel::onListCleared() {
    beginResetModel();
    m_currentIndex = -1;
    m_standbyIndex = -1;
    endResetModel();
}

void CueTableModel::onListLoaded() {
    beginResetModel();
    endResetModel();
}

} // namespace OpenMix
