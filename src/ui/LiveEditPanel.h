#pragma once

#include "core/LiveEditSession.h"
#include <QMap>
#include <QWidget>

class QLabel;
class QPushButton;
class QCheckBox;
class QScrollArea;
class QVBoxLayout;
class QComboBox;

namespace OpenMix {

class Application;
class ParameterEditWidget;

class LiveEditPanel : public QWidget {
    Q_OBJECT

  public:
    explicit LiveEditPanel(Application* app, QWidget* parent = nullptr);

    void refresh();

  public slots:
    void onModeChanged(LiveEditMode mode);
    void onSessionStarted(const QString& cueId);
    void onSessionEnded();
    void onParameterEdited(const QString& path, const QVariant& oldValue, const QVariant& newValue);
    void onParameterReverted(const QString& path);

  private slots:
    void onCommitClicked();
    void onCancelClicked();
    void onRevertAllClicked();
    void onPreviewToggled(bool checked);
    void onCommitTargetChanged(int index);
    void onParameterValueChanged(const QString& path, const QVariant& value);
    void onParameterRevertRequested(const QString& path);

  private:
    void setupUi();
    void connectSignals();
    void updateUiState();
    void updateStatusLabel();
    void clearParameterWidgets();
    void populateParameterWidgets();
    ParameterEditWidget* findOrCreateWidget(const QString& path);
    QString formatParameterName(const QString& path) const;

    Application* m_app;

    // header
    QLabel* m_statusLabel;
    QLabel* m_cueLabel;

    // controls
    QCheckBox* m_previewCheck;
    QComboBox* m_commitTargetCombo;
    QPushButton* m_commitButton;
    QPushButton* m_cancelButton;
    QPushButton* m_revertAllButton;

    // parameter edit area
    QScrollArea* m_scrollArea;
    QWidget* m_scrollWidget;
    QVBoxLayout* m_paramLayout;
    QMap<QString, ParameterEditWidget*> m_paramWidgets;
};

} // namespace OpenMix
