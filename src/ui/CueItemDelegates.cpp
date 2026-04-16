#include "CueItemDelegates.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "theme/Theme.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDoubleValidator>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QTimer>

namespace OpenMix {

CueNumberDelegate::CueNumberDelegate(CueList* cueList, QObject* parent)
    : QStyledItemDelegate(parent), m_cueList(cueList) {}

void CueNumberDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const {
    QStyleOptionViewItem opt = option;
    opt.state &= ~QStyle::State_HasFocus;

    QVariant bgData = index.data(Qt::BackgroundRole);
    if (bgData.isValid()) {
        opt.state &= ~QStyle::State_Selected;
        painter->fillRect(opt.rect, bgData.value<QBrush>());
        opt.backgroundBrush = bgData.value<QBrush>();
    }

    QStyledItemDelegate::paint(painter, opt, index);
}

QWidget* CueNumberDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                         const QModelIndex& index) const {
    Q_UNUSED(option);

    m_currentEditIndex = index;

    QLineEdit* editor = new QLineEdit(parent);
    editor->setFrame(false);
    editor->setFocusPolicy(Qt::StrongFocus);
    QDoubleValidator* validator = new QDoubleValidator(0.0, 9999.9, 1, editor);
    validator->setNotation(QDoubleValidator::StandardNotation);
    editor->setValidator(validator);
    editor->installEventFilter(const_cast<CueNumberDelegate*>(this));

    return editor;
}

void CueNumberDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    QString value = index.model()->data(index, Qt::EditRole).toString();
    QLineEdit* lineEdit = static_cast<QLineEdit*>(editor);
    lineEdit->setText(value);
    lineEdit->selectAll();
}

void CueNumberDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                     const QModelIndex& index) const {
    QLineEdit* lineEdit = static_cast<QLineEdit*>(editor);
    QString text = lineEdit->text().trimmed();

    if (text.isEmpty()) {
        return; // no empty input
    }

    bool ok;
    double newNumber = text.toDouble(&ok);
    if (!ok) {
        return;
    }

    // get current cue number for row
    QString currentNumberStr = index.model()->data(index, Qt::EditRole).toString();
    double currentNumber = currentNumberStr.toDouble();

    // check if number changed
    if (qFuzzyCompare(newNumber, currentNumber)) {
        return; // no change
    }

    // check for conflicts with other cues
    if (m_cueList) {
        int existingIndex = m_cueList->indexOfNumber(newNumber);
        if (existingIndex >= 0 && existingIndex != index.row()) {
            QMessageBox::warning(
                qobject_cast<QWidget*>(editor->parent()), QObject::tr("Cue Number Conflict"),
                QObject::tr("Cue %1 already exists. Please choose a different number.")
                    .arg(newNumber, 0, 'f', 1));
            return;
        }
    }

    model->setData(index, newNumber, Qt::EditRole);
}

void CueNumberDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                             const QModelIndex& index) const {
    Q_UNUSED(index);
    editor->setGeometry(option.rect);
}

bool CueNumberDelegate::eventFilter(QObject* object, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
            QWidget* editor = qobject_cast<QWidget*>(object);
            if (editor && m_currentEditIndex.isValid()) {
                bool forward = (keyEvent->key() == Qt::Key_Tab);
                emit tabNavigationRequested(m_currentEditIndex, forward);
                return true;
            }
        }
    }
    return QStyledItemDelegate::eventFilter(object, event);
}

CueTypeDelegate::CueTypeDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void CueTypeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const {
    QStyleOptionViewItem opt = option;
    opt.state &= ~QStyle::State_HasFocus;

    QVariant bgData = index.data(Qt::BackgroundRole);
    if (bgData.isValid()) {
        opt.state &= ~QStyle::State_Selected;
        painter->fillRect(opt.rect, bgData.value<QBrush>());
        opt.backgroundBrush = bgData.value<QBrush>();
    }

    QStyledItemDelegate::paint(painter, opt, index);
}

QWidget* CueTypeDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const {
    Q_UNUSED(option);

    m_currentEditIndex = index;

    QComboBox* editor = new QComboBox(parent);
    editor->setFrame(false);
    editor->setFocusPolicy(Qt::StrongFocus);

    // add all cue types
    editor->addItem(cueTypeToString(CueType::Snapshot));
    editor->addItem(cueTypeToString(CueType::Stop));
    editor->addItem(cueTypeToString(CueType::GoTo));
    editor->addItem(cueTypeToString(CueType::Wait));
    editor->addItem(cueTypeToString(CueType::Macro));

    editor->installEventFilter(const_cast<CueTypeDelegate*>(this));

    connect(editor, QOverload<int>::of(&QComboBox::activated), editor, [editor](int) {
        editor->setProperty("_selectionMade", true);
        QTimer::singleShot(200, editor,
                           [editor]() { editor->setProperty("_selectionMade", false); });
    });

    return editor;
}

void CueTypeDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    QString value = index.model()->data(index, Qt::EditRole).toString();
    QComboBox* comboBox = static_cast<QComboBox*>(editor);

    int idx = comboBox->findText(value, Qt::MatchFixedString);
    if (idx >= 0) {
        comboBox->setCurrentIndex(idx);
    }
}

void CueTypeDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                   const QModelIndex& index) const {
    QComboBox* comboBox = static_cast<QComboBox*>(editor);
    model->setData(index, comboBox->currentText(), Qt::EditRole);
}

void CueTypeDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                           const QModelIndex& index) const {
    Q_UNUSED(index);
    editor->setGeometry(option.rect);
}

bool CueTypeDelegate::eventFilter(QObject* object, QEvent* event) {
    QComboBox* comboBox = qobject_cast<QComboBox*>(object);

    if (comboBox && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            bool selectionMade = comboBox->property("_selectionMade").toBool();
            if (!selectionMade && !comboBox->view()->isVisible()) {
                comboBox->showPopup();
                return true;
            }
        }

        if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
            if (m_currentEditIndex.isValid()) {
                bool forward = (keyEvent->key() == Qt::Key_Tab);
                // emit navigation signal - view will handle closing the editor
                emit tabNavigationRequested(m_currentEditIndex, forward);
                return true;
            }
        }
    }
    return QStyledItemDelegate::eventFilter(object, event);
}

CueTextDelegate::CueTextDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void CueTextDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const {
    QStyleOptionViewItem opt = option;
    opt.state &= ~QStyle::State_HasFocus;

    QVariant bgData = index.data(Qt::BackgroundRole);
    if (bgData.isValid()) {
        opt.state &= ~QStyle::State_Selected;
        painter->fillRect(opt.rect, bgData.value<QBrush>());
        opt.backgroundBrush = bgData.value<QBrush>();
    }

    QStyledItemDelegate::paint(painter, opt, index);
}

QWidget* CueTextDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const {
    Q_UNUSED(option);

    m_currentEditIndex = index;

    QLineEdit* editor = new QLineEdit(parent);
    editor->setFrame(false);
    editor->setFocusPolicy(Qt::StrongFocus);
    editor->installEventFilter(const_cast<CueTextDelegate*>(this));

    return editor;
}

void CueTextDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    QString value = index.model()->data(index, Qt::EditRole).toString();
    QLineEdit* lineEdit = static_cast<QLineEdit*>(editor);
    lineEdit->setText(value);
    lineEdit->selectAll();
}

void CueTextDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                   const QModelIndex& index) const {
    QLineEdit* lineEdit = static_cast<QLineEdit*>(editor);
    model->setData(index, lineEdit->text(), Qt::EditRole);
}

void CueTextDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                           const QModelIndex& index) const {
    Q_UNUSED(index);
    editor->setGeometry(option.rect);
}

bool CueTextDelegate::eventFilter(QObject* object, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
            QWidget* editor = qobject_cast<QWidget*>(object);
            if (editor && m_currentEditIndex.isValid()) {
                bool forward = (keyEvent->key() == Qt::Key_Tab);
                emit tabNavigationRequested(m_currentEditIndex, forward);
                return true;
            }
        }
    }
    return QStyledItemDelegate::eventFilter(object, event);
}

} // namespace OpenMix
