#include "PopOutWindow.h"
#include "theme/Theme.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QSettings>
#include <QVBoxLayout>

namespace OpenMix {

PopOutWindow::PopOutWindow(const QString& settingsKey, const QString& title, QWidget* parent)
    : QDialog(parent,
              Qt::Tool | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
      m_settingsKey(settingsKey) {
    setObjectName(Theme::ObjectNames::PopOutWindow);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setModal(false);

    setupUi(title);
    loadGeometry();
}

PopOutWindow::~PopOutWindow() { saveGeometry(); }

void PopOutWindow::setupUi(const QString& title) {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName("PopOutTitleBar");
    m_titleBar->setFixedHeight(36);

    QHBoxLayout* titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(12, 0, 8, 0);
    titleLayout->setSpacing(8);

    m_titleLabel = new QLabel(title, m_titleBar);
    m_titleLabel->setProperty("role", "header");
    titleLayout->addWidget(m_titleLabel);

    m_mainLayout->addWidget(m_titleBar);

    m_contentContainer = new QWidget(this);
    m_contentContainer->setObjectName("PopOutContent");
    QVBoxLayout* contentLayout = new QVBoxLayout(m_contentContainer);
    contentLayout->setContentsMargins(Theme::SpacingS, Theme::SpacingS, Theme::SpacingS,
                                      Theme::SpacingS);
    contentLayout->setSpacing(0);

    m_mainLayout->addWidget(m_contentContainer, 1);

    QDialog::setWindowTitle(title);
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
    show();
    raise();
    activateWindow();
}

void PopOutWindow::setMinimumContentSize(int width, int height) {
    setMinimumSize(width, height + m_titleBar->height());
}

void PopOutWindow::setWindowTitle(const QString& title) {
    QDialog::setWindowTitle(title);
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
}

void PopOutWindow::closeEvent(QCloseEvent* event) {
    saveGeometry();
    emit closed();
    event->accept();
    hide();
}

void PopOutWindow::moveEvent(QMoveEvent* event) { QDialog::moveEvent(event); }

void PopOutWindow::resizeEvent(QResizeEvent* event) { QDialog::resizeEvent(event); }

void PopOutWindow::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    emit visibilityChanged(true);
}

void PopOutWindow::hideEvent(QHideEvent* event) {
    saveGeometry();
    QDialog::hideEvent(event);
    emit visibilityChanged(false);
}

void PopOutWindow::saveGeometry() {
    if (!isVisible() && !m_geometryLoaded) {
        return;
    }

    QSettings settings("OpenMix", "OpenMix");
    settings.beginGroup("PopOutWindows");
    settings.setValue(m_settingsKey + "/geometry", QDialog::saveGeometry());
    settings.endGroup();
}

void PopOutWindow::loadGeometry() {
    QSettings settings("OpenMix", "OpenMix");
    settings.beginGroup("PopOutWindows");

    QByteArray geometry = settings.value(m_settingsKey + "/geometry").toByteArray();
    settings.endGroup();

    if (!geometry.isEmpty()) {
        QDialog::restoreGeometry(geometry);
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
