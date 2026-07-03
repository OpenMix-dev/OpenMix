#include "PopOutWindow.h"
#include "WindowSizing.h"
#include "theme/Theme.h"

#include <QCloseEvent>
#include <QKeyEvent>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QSettings>
#include <QVBoxLayout>

namespace OpenMix {

PopOutWindow::PopOutWindow(const QString& settingsKey, const QString& title, QWidget* parent)
    : QWidget(parent,
              Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
      m_settingsKey(settingsKey) {
    setObjectName(Theme::ObjectNames::PopOutWindow);
    setAttribute(Qt::WA_DeleteOnClose, false);
    // plain QWidgets only paint stylesheet backgrounds when asked
    setAttribute(Qt::WA_StyledBackground, true);

    setupUi(title);
    loadGeometry();
    WindowSizing::widenOnShow(this);
}

PopOutWindow::~PopOutWindow() { saveGeometry(); }

void PopOutWindow::setupUi(const QString& title) {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    m_contentContainer = new QWidget(this);
    m_contentContainer->setObjectName("PopOutContent");
    QVBoxLayout* contentLayout = new QVBoxLayout(m_contentContainer);
    contentLayout->setContentsMargins(Theme::SpacingS, Theme::SpacingS, Theme::SpacingS,
                                      Theme::SpacingS);
    contentLayout->setSpacing(0);

    m_mainLayout->addWidget(m_contentContainer, 1);

    QWidget::setWindowTitle(title);
}

void PopOutWindow::setContentWidget(QWidget* content) {
    if (m_content) {
        m_contentContainer->layout()->removeWidget(m_content);
        m_content->setParent(nullptr);
    }

    m_content = content;

    if (m_content) {
        m_content->setParent(m_contentContainer);
        static_cast<QVBoxLayout*>(m_contentContainer->layout())->addWidget(m_content, 1);
    }
}

void PopOutWindow::showAndRestore() {
    if (!m_geometryLoaded) {
        loadGeometry();
    }
    if (isMinimized()) {
        setWindowState(windowState() & ~Qt::WindowMinimized);
    }
    show();
    raise();
    activateWindow();
}

void PopOutWindow::setMinimumContentSize(int width, int height) {
    setMinimumSize(width, height);
}

void PopOutWindow::setWindowTitle(const QString& title) {
    QWidget::setWindowTitle(title);
}

void PopOutWindow::closeEvent(QCloseEvent* event) {
    saveGeometry();
    emit closed();
    event->accept();
    hide();
}

void PopOutWindow::keyPressEvent(QKeyEvent* event) {
    // Escape intentionally closes the tool window; it routes through
    // closeEvent -> hide so visibility indicators stay in sync
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QWidget::keyPressEvent(event);
}

void PopOutWindow::moveEvent(QMoveEvent* event) { QWidget::moveEvent(event); }

void PopOutWindow::resizeEvent(QResizeEvent* event) { QWidget::resizeEvent(event); }

void PopOutWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    emit visibilityChanged(true);
}

void PopOutWindow::hideEvent(QHideEvent* event) {
    saveGeometry();
    QWidget::hideEvent(event);
    emit visibilityChanged(false);
}

void PopOutWindow::saveGeometry() {
    if (!isVisible() && !m_geometryLoaded) {
        return;
    }

    QSettings settings("OpenMix", "OpenMix");
    settings.beginGroup("PopOutWindows");
    settings.setValue(m_settingsKey + "/geometry", QWidget::saveGeometry());
    settings.endGroup();
}

void PopOutWindow::loadGeometry() {
    QSettings settings("OpenMix", "OpenMix");
    settings.beginGroup("PopOutWindows");

    QByteArray geometry = settings.value(m_settingsKey + "/geometry").toByteArray();
    settings.endGroup();

    if (!geometry.isEmpty()) {
        QWidget::restoreGeometry(geometry);
    } else {
        resize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        if (parentWidget()) {
            QPoint center = parentWidget()->geometry().center();
            move(center.x() - DEFAULT_WIDTH / 2, center.y() - DEFAULT_HEIGHT / 2);
        }
    }

    m_geometryLoaded = true;
}

} // namespace OpenMix
