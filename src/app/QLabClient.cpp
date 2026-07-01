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
    m_connected = false; // re-authenticate against the new target
    rebuildAddress();
}

void QLabClient::setPasscode(const QString& passcode) {
    if (m_passcode == passcode)
        return;
    m_passcode = passcode;
    m_connected = false; // resend /connect with the new passcode
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

void QLabClient::ensureConnected() {
    if (m_connected || m_passcode.isEmpty() || !m_address)
        return;
    lo_send(m_address, (prefix() + QStringLiteral("/connect")).toUtf8().constData(), "s",
            m_passcode.toUtf8().constData());
    m_connected = true;
}

void QLabClient::send(const QString& address) {
    if (!m_enabled || !m_address)
        return;
    ensureConnected();
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

void QLabClient::back() {
    if (m_suppressBack)
        return;
    send(prefix() + QStringLiteral("/playhead/previous"));
}

void QLabClient::loadFromSettings() {
    QSettings settings;
    settings.beginGroup("QLab");
    m_host = settings.value("host", m_host).toString();
    m_port = settings.value("port", m_port).toInt();
    m_enabled = settings.value("enabled", false).toBool();
    m_preRollMs = settings.value("preRollMs", 0).toInt();
    m_workspaceId = settings.value("workspaceId").toString();
    m_passcode = settings.value("passcode").toString();
    m_suppressBack = settings.value("suppressBack", false).toBool();
    m_connected = false;
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
    settings.setValue("passcode", m_passcode);
    settings.setValue("suppressBack", m_suppressBack);
    settings.endGroup();
}

} // namespace OpenMix
