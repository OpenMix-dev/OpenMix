#include "CueItemDelegates.h"
#include "core/ActorProfileLibrary.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "theme/Theme.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QDoubleValidator>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPolygon>
#include <QTimer>

namespace OpenMix {

namespace {

// the app stylesheet's input padding and minimum height overflow a compact
// table row; strip the box model so inline cell editors fit the cell exactly.
// the explicit background keeps the editor opaque even when the cascaded app
// stylesheet is absent (fallback sheet) — the view repaints the cell text
// beneath the editor, and a translucent editor shows both texts superimposed
void fitEditorToCell(QWidget* editor) {
    editor->setStyleSheet(QStringLiteral("QLineEdit, QComboBox { border: none; "
                                         "border-radius: 0; padding: 0 4px; "
                                         "min-height: 0; background-color: %1; }")
                              .arg(QLatin1String(Theme::Colors::BgInput)));
}

// true when the view has an inline editor open on this index. The view keeps
// repainting the cell underneath its editor; painting glyphs there produces
// doubled text whenever the editor isn't fully opaque, so callers fill only
// the row background (the editor covers the full cell rect) and skip the rest.
bool skipPaintForOpenEditor(QPainter* painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) {
    const auto* view = qobject_cast<const QAbstractItemView*>(option.widget);
    if (!view || !view->indexWidget(index))
        return false;
    const QVariant bgData = index.data(Qt::BackgroundRole);
    if (bgData.isValid())
        painter->fillRect(option.rect, bgData.value<QBrush>());
    return true;
}

} // namespace

CueNumberDelegate::CueNumberDelegate(CueList* cueList, QObject* parent)
    : QStyledItemDelegate(parent), m_cueList(cueList) {}

void CueNumberDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const {
    if (skipPaintForOpenEditor(painter, option, index))
        return;

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

QWidget* CueNumberDelegate::createEditor(QWidget* parent, [[maybe_unused]] const QStyleOptionViewItem& option,
                                         const QModelIndex& index) const {

    m_currentEditIndex = index;

    QLineEdit* editor = new QLineEdit(parent);
    editor->setFrame(false);
    fitEditorToCell(editor);
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
        const auto existingIndex = m_cueList->indexOfNumber(newNumber);
        if (existingIndex && *existingIndex != index.row()) {
            // defer the modal: showing it here would pump the event loop during
            // Qt's editor-commit and can destroy the editor underneath us
            const double dup = newNumber;
            QTimer::singleShot(0, [dup]() {
                QMessageBox::warning(
                    nullptr, QObject::tr("Cue Number Conflict"),
                    QObject::tr("Cue %1 already exists. Please choose a different number.")
                        .arg(dup, 0, 'f', 1));
            });
            return;
        }
    }

    model->setData(index, newNumber, Qt::EditRole);
}

void CueNumberDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                             [[maybe_unused]] const QModelIndex& index) const {
    editor->setGeometry(option.rect);
}

bool CueNumberDelegate::eventFilter(QObject* object, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
            if (auto* editor = qobject_cast<QWidget*>(object); editor && m_currentEditIndex.isValid()) {
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
    if (skipPaintForOpenEditor(painter, option, index))
        return;

    QStyleOptionViewItem opt = option;
    opt.state &= ~QStyle::State_HasFocus;

    QVariant bgData = index.data(Qt::BackgroundRole);
    if (bgData.isValid()) {
        opt.state &= ~QStyle::State_Selected;
        painter->fillRect(opt.rect, bgData.value<QBrush>());
        opt.backgroundBrush = bgData.value<QBrush>();
    }

    // distinct shape+color per cue type reads faster than the word alone
    const CueType type = stringToCueType(index.data(Qt::DisplayRole).toString());
    const int sz = 10;
    QRect box(opt.rect.left() + 8, opt.rect.center().y() - sz / 2, sz, sz);

    QColor c;
    switch (type) {
    case CueType::Snapshot: c = Theme::color(Theme::Colors::AccentGreen); break;
    case CueType::Stop:     c = Theme::color(Theme::Colors::AccentRed); break;
    case CueType::GoTo:     c = Theme::color(Theme::Colors::AccentBlue); break;
    case CueType::Wait:     c = Theme::color(Theme::Colors::AccentAmber); break;
    case CueType::Macro:    c = Theme::color(Theme::Colors::AccentBlueHover); break;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(c);
    switch (type) {
    case CueType::Snapshot:
        painter->drawEllipse(box);
        break;
    case CueType::Stop:
        painter->drawRect(box);
        break;
    case CueType::GoTo: {
        QPolygon tri;
        tri << QPoint(box.left(), box.top()) << QPoint(box.right(), box.center().y())
            << QPoint(box.left(), box.bottom());
        painter->drawPolygon(tri);
        break;
    }
    case CueType::Wait: {
        QPolygon dia;
        dia << QPoint(box.center().x(), box.top()) << QPoint(box.right(), box.center().y())
            << QPoint(box.center().x(), box.bottom()) << QPoint(box.left(), box.center().y());
        painter->drawPolygon(dia);
        break;
    }
    case CueType::Macro:
        for (int i = 0; i < 3; ++i)
            painter->drawRect(box.left(), box.top() + i * 4, sz, 2);
        break;
    }
    painter->restore();

    // indent the text past the marker
    opt.rect.setLeft(box.right() + 6);
    QStyledItemDelegate::paint(painter, opt, index);
}

QWidget* CueTypeDelegate::createEditor(QWidget* parent, [[maybe_unused]] const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const {

    m_currentEditIndex = index;

    QComboBox* editor = new QComboBox(parent);
    editor->setFrame(false);
    fitEditorToCell(editor);
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
                                           [[maybe_unused]] const QModelIndex& index) const {
    editor->setGeometry(option.rect);
}

bool CueTypeDelegate::eventFilter(QObject* object, QEvent* event) {
    if (auto* comboBox = qobject_cast<QComboBox*>(object); comboBox && event->type() == QEvent::KeyPress) {
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

DCAAssignDelegate::DCAAssignDelegate(ActorProfileLibrary* library, QObject* parent)
    : QStyledItemDelegate(parent), m_library(library) {}

void DCAAssignDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const {
    QStyleOptionViewItem opt = option;
    if (skipPaintForOpenEditor(painter, option, index))
        return;

    opt.state &= ~QStyle::State_HasFocus;

    QVariant bgData = index.data(Qt::BackgroundRole);
    if (bgData.isValid()) {
        opt.state &= ~QStyle::State_Selected;
        painter->fillRect(opt.rect, bgData.value<QBrush>());
        opt.backgroundBrush = bgData.value<QBrush>();
    }

    QStyledItemDelegate::paint(painter, opt, index);
}

QWidget* DCAAssignDelegate::createEditor(QWidget* parent,
                                         [[maybe_unused]] const QStyleOptionViewItem& option,
                                         const QModelIndex& index) const {
    m_currentEditIndex = index;

    QLineEdit* editor = new QLineEdit(parent);
    editor->setFrame(false);
    fitEditorToCell(editor);
    editor->setFocusPolicy(Qt::StrongFocus);
    editor->setPlaceholderText(tr("Role, actor, or label"));
    if (m_library) {
        // fresh completer per edit so the candidates are always current
        auto* completer = new QCompleter(m_library->completionCandidates(), editor);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        editor->setCompleter(completer);
    }
    editor->installEventFilter(const_cast<DCAAssignDelegate*>(this));

    return editor;
}

void DCAAssignDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    QString value = index.model()->data(index, Qt::EditRole).toString();
    QLineEdit* lineEdit = static_cast<QLineEdit*>(editor);
    lineEdit->setText(value);
    lineEdit->selectAll();
}

void DCAAssignDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                     const QModelIndex& index) const {
    QLineEdit* lineEdit = static_cast<QLineEdit*>(editor);
    model->setData(index, lineEdit->text(), Qt::EditRole);
}

void DCAAssignDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                             [[maybe_unused]] const QModelIndex& index) const {
    editor->setGeometry(option.rect);
}

bool DCAAssignDelegate::eventFilter(QObject* object, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
            if (auto* editor = qobject_cast<QWidget*>(object);
                editor && m_currentEditIndex.isValid()) {
                bool forward = (keyEvent->key() == Qt::Key_Tab);
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
    if (skipPaintForOpenEditor(painter, option, index))
        return;

    opt.state &= ~QStyle::State_HasFocus;

    QVariant bgData = index.data(Qt::BackgroundRole);
    if (bgData.isValid()) {
        opt.state &= ~QStyle::State_Selected;
        painter->fillRect(opt.rect, bgData.value<QBrush>());
        opt.backgroundBrush = bgData.value<QBrush>();
    }

    QStyledItemDelegate::paint(painter, opt, index);
}

QWidget* CueTextDelegate::createEditor(QWidget* parent, [[maybe_unused]] const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const {

    m_currentEditIndex = index;

    QLineEdit* editor = new QLineEdit(parent);
    editor->setFrame(false);
    fitEditorToCell(editor);
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
                                           [[maybe_unused]] const QModelIndex& index) const {
    editor->setGeometry(option.rect);
}

bool CueTextDelegate::eventFilter(QObject* object, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
            if (auto* editor = qobject_cast<QWidget*>(object); editor && m_currentEditIndex.isValid()) {
                bool forward = (keyEvent->key() == Qt::Key_Tab);
                emit tabNavigationRequested(m_currentEditIndex, forward);
                return true;
            }
        }
    }
    return QStyledItemDelegate::eventFilter(object, event);
}

} // namespace OpenMix
