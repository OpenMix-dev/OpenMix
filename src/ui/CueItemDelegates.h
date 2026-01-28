#pragma once

#include <QComboBox>
#include <QLineEdit>
#include <QStyledItemDelegate>

namespace OpenMix {

class CueList;

class CueNumberDelegate : public QStyledItemDelegate {
    Q_OBJECT

  public:
    explicit CueNumberDelegate(CueList* cueList, QObject* parent = nullptr);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;

    void setEditorData(QWidget* editor, const QModelIndex& index) const override;

    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override;

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const override;

    bool eventFilter(QObject* object, QEvent* event) override;

  signals:
    void tabNavigationRequested(const QModelIndex& fromIndex, bool forward) const;

  private:
    CueList* m_cueList;
    mutable QModelIndex m_currentEditIndex;
};

class CueTypeDelegate : public QStyledItemDelegate {
    Q_OBJECT

  public:
    explicit CueTypeDelegate(QObject* parent = nullptr);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;

    void setEditorData(QWidget* editor, const QModelIndex& index) const override;

    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override;

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const override;

    bool eventFilter(QObject* object, QEvent* event) override;

  signals:
    void tabNavigationRequested(const QModelIndex& fromIndex, bool forward) const;

  private:
    mutable QModelIndex m_currentEditIndex;
};

class CueTextDelegate : public QStyledItemDelegate {
    Q_OBJECT

  public:
    explicit CueTextDelegate(QObject* parent = nullptr);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;

    void setEditorData(QWidget* editor, const QModelIndex& index) const override;

    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override;

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const override;

    bool eventFilter(QObject* object, QEvent* event) override;

  signals:
    void tabNavigationRequested(const QModelIndex& fromIndex, bool forward) const;

  private:
    mutable QModelIndex m_currentEditIndex;
};

} // namespace OpenMix
