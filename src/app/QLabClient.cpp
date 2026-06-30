#include "QLabClient.h"

#include <QByteArray>
#include <QSettings>
#include <QTimer>

namespace OpenMix {

QLabClient::QLabClient(QObject* parent) : QObject(parent) { rebuildAddress(); }

QLabClient::~QLabClient() {
    if (m_address)
        lo_address_free(m_address);
}

void QLabClient::setTarget(const QString& host, int port) {
    m_host = host;
    m_port = port > 0 ? port : QLAB_DEFAULT_PORT;
    rebuildAddress();
}

void QLabClient::rebuildAddress() {
    if (m_address) {
        lo_address_free(m_address);
        m_address = nullptr;
    }
    m_address =
        lo_address_new(m_host.toUtf8().constData(), QByteArray::number(m_port).constData());
}

QString QLabClient::prefix() const {
    return m_workspaceId.isEmpty() ? QString() : QStringLiteral("/workspace/") + m_workspaceId;
}

void QLabClient::send(const QString& address) {
    if (!m_enabled || !m_address)
        return;
    lo_send(m_address, address.toUtf8().constData(), "");
    emit sent(address);
}

void QLabClient::triggerCue(const QString& cueId) {
    if (!m_enabled || cueId.isEmpty())
        return;

    const QString address = prefix() + QStringLiteral("/cue/") + cueId + QStringLiteral("/start");
    if (m_preRollMs > 0) {
        QTimer::singleShot(m_preRollMs, this, [this, address]() { send(address); });
    } else {
        send(address);
    }
}

void QLabClient::go() { send(prefix() + QStringLiteral("/go")); }

void QLabClient::loadFromSettings() {
    QSettings settings;
    settings.beginGroup("QLab");
    m_host = settings.value("host", m_host).toString();
    m_port = settings.value("port", m_port).toInt();
    m_enabled = settings.value("enabled", false).toBool();
    m_preRollMs = settings.value("preRollMs", 0).toInt();
    m_workspaceId = settings.value("workspaceId").toString();
    settings.endGroup();
    rebuildAddress();
}

void QLabClient::saveToSettings() {
    QSettings settings;
    settings.beginGroup("QLab");
    settings.setValue("host", m_host);
    settings.setValue("port", m_port);
    settings.setValue("enabled", m_enabled);
    settings.setValue("preRollMs", m_preRollMs);
    settings.setValue("workspaceId", m_workspaceId);
    settings.endGroup();
}

} // namespace OpenMix
