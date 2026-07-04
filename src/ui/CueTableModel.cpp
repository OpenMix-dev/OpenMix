#include "CueTableModel.h"
#include "core/Actor.h"
#include "core/ActorProfileLibrary.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/DCAMapping.h"
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
        connect(m_cueList, &CueList::cueAboutToBeAdded, this,
                &CueTableModel::onCueAboutToBeAdded);
        connect(m_cueList, &CueList::cueAdded, this, &CueTableModel::onCueAdded);
        connect(m_cueList, &CueList::cueAboutToBeRemoved, this,
                &CueTableModel::onCueAboutToBeRemoved);
        connect(m_cueList, &CueList::cueRemoved, this, &CueTableModel::onCueRemoved);
        connect(m_cueList, &CueList::cueUpdated, this, &CueTableModel::onCueUpdated);
        connect(m_cueList, &CueList::cueAboutToBeMoved, this,
                &CueTableModel::onCueAboutToBeMoved);
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
    return ColCount + m_dcaCount * DcaSubCols;
}

void CueTableModel::setDcaCount(int count) {
    count = qBound(0, count, 64);
    if (count == m_dcaCount)
        return;
    beginResetModel();
    m_dcaCount = count;
    endResetModel();
}

int CueTableModel::dcaSubColumn(int col) const {
    if (col < ColCount || col >= ColCount + m_dcaCount * DcaSubCols)
        return -1;
    return (col - ColCount) % DcaSubCols;
}

int CueTableModel::dcaOfColumn(int col) const {
    if (col < ColCount || col >= ColCount + m_dcaCount * DcaSubCols)
        return -1;
    return (col - ColCount) / DcaSubCols + 1; // 1-based
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
        // per-DCA triplet columns: [assignment | fx | pos]
        const int sub = dcaSubColumn(col);
        if (sub >= 0) {
            const int dca = dcaOfColumn(col);
            const DCAOverride ov = cue.dcaOverride(dca);
            if (sub == 0) { // assignment: the cue's DCA label / mute override
                if (ov.label.has_value()) {
                    // editors get the raw typed label; display resolves it to
                    // the actor in that role
                    if (role == Qt::EditRole)
                        return *ov.label;
                    const Actor* actor =
                        m_actorLibrary ? m_actorLibrary->resolveActor(*ov.label) : nullptr;
                    if (actor) {
                        // prefer the role the label matched, else the primary
                        const QString secondary =
                            actor->secondaryName(actor->matchedRole(*ov.label));
                        return secondary.isEmpty()
                                   ? actor->displayName()
                                   : tr("%1 (%2)").arg(actor->displayName(), secondary);
                    }
                    return *ov.label;
                }
                if (ov.mute.has_value())
                    return *ov.mute ? tr("mute") : tr("on");
                return QString();
            }
            // fx / pos: summarise the cue's per-channel FX / position over the
            // channels assigned to this DCA (via the show's DCA mapping)
            if (m_dcaMapping) {
                const QList<int> channels = m_dcaMapping->channelsForDCA(dca);
                if (sub == 1) { // fx
                    const QMap<int, bool> fx = cue.channelFX();
                    int n = 0;
                    for (int ch : channels)
                        if (fx.value(ch, false))
                            ++n;
                    return n > 0 ? QString::number(n) : QString();
                }
                if (sub == 2) { // pos
                    const QMap<int, QString> pos = cue.channelPositions();
                    int n = 0;
                    for (int ch : channels)
                        if (!pos.value(ch).isEmpty())
                            ++n;
                    return n > 0 ? QString::number(n) : QString();
                }
            }
            return QString();
        }

        switch (col) {
        case ColNumber: {
            const QString num = QString::number(cue.number(), 'f', 1);
            // Marker only on the display text, never the edit text.
            if (role == Qt::DisplayRole) {
                if (row == m_currentIndex)
                    return QString("▶ ") + num;
                if (row == m_standbyIndex)
                    return QString("→ ") + num;
            }
            return num;
        }
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
        case ColColor:
            return QVariant(); // shown as a ● dot via the decoration role
        case ColScene: {
            QStringList parts;
            for (int s : cue.scenes())
                parts << QString::number(s);
            return parts.join(", ");
        }
        case ColSnip: {
            QStringList parts;
            for (int s : cue.snippets())
                parts << QString::number(s);
            return parts.join(", ");
        }
        case ColExternal:
            return cue.qLabCue();
        case ColFx: {
            QStringList parts;
            const QMap<int, bool> mutes = cue.fxMutes();
            for (auto it = mutes.begin(); it != mutes.end(); ++it) {
                if (it.value())
                    parts << QString::number(it.key());
            }
            return parts.isEmpty() ? QString() : tr("mute %1").arg(parts.join(", "));
        }
        case ColFade:
            return cue.fadeTime() > 0.0 ? tr("%1s").arg(cue.fadeTime(), 0, 'f', 1)
                                        : tr("instant");
        }
    }

    // color swatch shown in the color column
    if (role == Qt::DecorationRole && col == ColColor) {
        const QString hex = cue.color();
        if (!hex.isEmpty()) {
            QColor c(hex);
            if (c.isValid())
                return c;
        }
        return QVariant();
    }

    if (role == Qt::BackgroundRole) {
        // green = running now, amber = standing by next. Distinct so an
        // operator reads the list at a glance under stage lighting.
        if (row == m_currentIndex) {
            return QBrush(Theme::withAlpha(Theme::Colors::AccentGreen, 220));
        }
        if (row == m_standbyIndex) {
            return QBrush(Theme::withAlpha(Theme::Colors::AccentAmber, 150));
        }
    }

    if (role == Qt::ForegroundRole) {
        if (row == m_currentIndex || row == m_standbyIndex) {
            return QBrush(Theme::color(Theme::Colors::TextPrimary));
        }
    }

    if (role == Qt::FontRole) {
        if (row == m_currentIndex || row == m_standbyIndex) {
            QFont font;
            font.setBold(true);
            return font;
        }
    }

    if (role == Qt::ToolTipRole) {
        // DCA assignment cells: overrides + resolved members, like the cue
        // editor's assign info
        if (dcaSubColumn(col) == 0) {
            const int dca = dcaOfColumn(col);
            const DCAOverride ov = cue.dcaOverride(dca);
            QStringList lines;
            if (ov.label.has_value())
                lines << tr("Label: %1").arg(*ov.label);
            if (ov.mute.has_value())
                lines << (*ov.mute ? tr("Mutes on fire") : tr("Unmutes on fire"));

            QList<int> channels;
            if (cue.hasCustomDCAMapping())
                channels = cue.dcaChannelMapping().value(dca);
            else if (m_dcaMapping)
                channels = m_dcaMapping->channelsForDCA(dca);
            if (!channels.isEmpty()) {
                const QString dcaLabel = ov.label.value_or(QString());
                QStringList members;
                for (int ch : channels) {
                    const Actor* actor =
                        m_actorLibrary ? m_actorLibrary->actorForChannel(ch) : nullptr;
                    if (actor) {
                        const QString secondary =
                            actor->secondaryName(actor->matchedRole(dcaLabel));
                        members << (secondary.isEmpty()
                                        ? tr("Ch %1 %2").arg(ch).arg(actor->displayName())
                                        : tr("Ch %1 %2 (%3)")
                                              .arg(ch)
                                              .arg(actor->displayName(), secondary));
                    } else {
                        members << tr("Ch %1").arg(ch);
                    }
                }
                lines << tr("Members: %1").arg(members.join(", "));
            }
            lines << tr("Type a role or actor name to auto-assign their channel to this DCA");
            return lines.join("\n");
        }

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

    // per-DCA triplet headers
    const int sub = dcaSubColumn(section);
    if (sub >= 0) {
        switch (sub) {
        case 0:
            return tr("DCA %1").arg(dcaOfColumn(section));
        case 1:
            return tr("fx");
        case 2:
            return tr("pos");
        }
    }

    switch (section) {
    case ColNumber:
        return tr("Cue");
    case ColName:
        return tr("Text");
    case ColType:
        return tr("Type");
    case ColGroup:
        return tr("Group");
    case ColTags:
        return tr("Tags");
    case ColNotes:
        return tr("Notes");
    case ColColor:
        return QString::fromUtf8("\xE2\x97\x8F"); // ●
    case ColScene:
        return tr("Scene");
    case ColSnip:
        return tr("Snip");
    case ColExternal:
        return tr("QLab");
    case ColFx:
        return tr("FX");
    case ColFade:
        return tr("Fade");
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

    // enable editing for primary cue props and DCA assignment cells
    int col = index.column();
    if (col == ColNumber || col == ColName || col == ColType || col == ColGroup || col == ColTags ||
        col == ColNotes || dcaSubColumn(col) == 0) {
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

    // DCA assignment cells: set the label override (preserving any mute) and
    // resolve a typed role/actor name to a channel assignment — the same
    // commit path the assign dialog used
    if (dcaSubColumn(col) == 0) {
        const int dca = dcaOfColumn(col);
        DCAOverride ov = cue.dcaOverride(dca);
        oldValue = ov.label.value_or(QString());
        const QString text = value.toString().trimmed();
        ov.label = text.isEmpty() ? std::nullopt : std::optional<QString>(text);
        cue.setDCAOverride(dca, ov);
        if (m_actorLibrary && !text.isEmpty()) {
            if (const Actor* actor = m_actorLibrary->resolveActor(text))
                cue.assignChannelToDCAMapping(actor->channel(), dca, m_dcaMapping);
        }
        emit cueEditRequested(row, col, oldValue, value);
        m_cueList->updateCue(row, cue);
        return true;
    }

    switch (col) {
    case ColNumber: {
        oldValue = cue.number();
        double newNumber = value.toDouble();
        cue.setNumber(newNumber);
        break;
    }
    case ColName:
        oldValue = cue.name();
        cue.setName(value.toString());
        break;
    case ColType: {
        oldValue = cueTypeToString(cue.type());
        CueType newType = stringToCueType(value.toString());
        cue.setType(newType);
        break;
    }
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
                                    [[maybe_unused]] int column, [[maybe_unused]] const QModelIndex& parent) const {

    if (!data->hasFormat(s_mimeType))
        return false;
    if (action != Qt::MoveAction)
        return false;
    if (row < 0)
        return false;

    return true;
}

bool CueTableModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                                 [[maybe_unused]] int column, [[maybe_unused]] const QModelIndex& parent) {

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
        emit dataChanged(this->index(oldIndex, 0), this->index(oldIndex, columnCount() - 1));
    }
    if (index >= 0 && index < rowCount()) {
        emit dataChanged(this->index(index, 0), this->index(index, columnCount() - 1));
    }
}

void CueTableModel::setStandbyCueIndex(int index) {
    int oldIndex = m_standbyIndex;
    m_standbyIndex = index;

    // refresh old & new rows
    if (oldIndex >= 0 && oldIndex < rowCount()) {
        emit dataChanged(this->index(oldIndex, 0), this->index(oldIndex, columnCount() - 1));
    }
    if (index >= 0 && index < rowCount()) {
        emit dataChanged(this->index(index, 0), this->index(index, columnCount() - 1));
    }
}

const Cue* CueTableModel::cueAt(int row) const {
    if (!m_cueList || row < 0 || row >= m_cueList->count()) {
        return nullptr;
    }
    return &m_cueList->at(row);
}

void CueTableModel::onCueAboutToBeAdded(int index) {
    beginInsertRows(QModelIndex(), index, index);
}

void CueTableModel::onCueAdded(int index) {
    endInsertRows();

    // shift highlights for rows that moved down by the insertion
    if (m_currentIndex >= index)
        ++m_currentIndex;
    if (m_standbyIndex >= index)
        ++m_standbyIndex;
}

void CueTableModel::onCueAboutToBeRemoved(int index) {
    // must bracket the removal before the row is erased from the list
    beginRemoveRows(QModelIndex(), index, index);
}

void CueTableModel::onCueRemoved(int index) {
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
    emit dataChanged(this->index(index, 0), this->index(index, columnCount() - 1));
}

void CueTableModel::onCueAboutToBeMoved(int from, int to) {
    // beginMoveRows must bracket the move before the list is reordered.
    // destinationChild is the row before the moved rows: moving down → to + 1,
    // moving up → to
    const int destRow = (from < to) ? to + 1 : to;
    beginMoveRows(QModelIndex(), from, from, QModelIndex(), destRow);
}

void CueTableModel::onCueMoved(int from, int to) {
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
