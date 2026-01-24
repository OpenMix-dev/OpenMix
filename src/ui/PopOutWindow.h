#pragma once

#include <QDialog>
#include <QPointer>

class QVBoxLayout;
class QHBoxLayout;
class QLabel;

namespace OpenMix {

class PopOutWindow : public QDialog {
    Q_OBJECT

  public:
    explicit PopOutWindow(const QString& settingsKey, const QString& title,
                          QWidget* parent = nullptr);

    ~PopOutWindow() override;
    void setContentWidget(QWidget* content);
    QWidget* contentWidget() const { return m_content; }
    void showAndRestore();
    void setMinimumContentSize(int width, int height);
    QString settingsKey() const { return m_settingsKey; }
    void setWindowTitle(const QString& title);

  signals:
    void closed();
    void visibilityChanged(bool visible);

  protected:
    void closeEvent(QCloseEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    void setupUi(const QString& title);
    void saveGeometry();
    void loadGeometry();

    QString m_settingsKey;
    QWidget* m_content = nullptr;
    QVBoxLayout* m_mainLayout = nullptr;
    QWidget* m_titleBar = nullptr;
    QLabel* m_titleLabel = nullptr;
    QWidget* m_contentContainer = nullptr;

    bool m_geometryLoaded = false;
    static constexpr int DEFAULT_WIDTH = 400;
    static constexpr int DEFAULT_HEIGHT = 500;
};

} // namespace OpenMix
