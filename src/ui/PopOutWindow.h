#pragma once

#include <QPointer>
#include <QWidget>

class QVBoxLayout;

namespace OpenMix {

// Floating tool window for pop-out panels. Deliberately a plain QWidget, not a
// QDialog: dialog default-button handling turns Enter in any field into a
// button click, and Escape into a hidden window.
class PopOutWindow : public QWidget {
    Q_OBJECT

  public:
    explicit PopOutWindow(const QString& settingsKey, const QString& title,
                          QWidget* parent = nullptr);

    ~PopOutWindow() override;
    void setContentWidget(QWidget* content);
    QWidget* contentWidget() const { return m_content; }
    void showAndRestore();
    // visible to the user, not just mapped (a minimized window stays isVisible())
    bool isEffectivelyVisible() const { return isVisible() && !isMinimized(); }
    void setMinimumContentSize(int width, int height);
    QString settingsKey() const { return m_settingsKey; }
    void setWindowTitle(const QString& title);

  signals:
    void closed();
    void visibilityChanged(bool visible);

  protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
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
    QWidget* m_contentContainer = nullptr;

    bool m_geometryLoaded = false;
    static constexpr int DEFAULT_WIDTH = 400;
    static constexpr int DEFAULT_HEIGHT = 500;
};

} // namespace OpenMix
