#include "ScsClient.h"

#include <QByteArray>
#include <QSettings>

namespace OpenMix {

ScsClient::ScsClient(QObject* parent) : QObject(parent) { rebuildAddress(); }

ScsClient::~ScsClient() {
    if (m_address)
        lo_address_free(m_address);
}

void ScsClient::setTarget(const QString& host, int port) {
    m_host = host;
    m_port = port > 0 ? port : SCS_DEFAULT_PORT;
    rebuildAddress();
}

void ScsClient::rebuildAddress() {
    if (m_address) {
        lo_address_free(m_address);
        m_address = nullptr;
    }
    m_address =
        lo_address_new(m_host.toUtf8().constData(), QByteArray::number(m_port).constData());
}

void ScsClient::send(const QString& address) {
    if (!m_enabled || !m_address)
        return;
    // Every control message carries a trailing string arg (control password,
    // empty by default).
    lo_send(m_address, address.toUtf8().constData(), "s", m_password.toUtf8().constData());
    emit sent(address);
}

void ScsClient::go() { send(QStringLiteral("/ctrl/go")); }

void ScsClient::stop() { send(QStringLiteral("/ctrl/stopall")); }

void ScsClient::fade() { send(QStringLiteral("/ctrl/fadeall")); }

void ScsClient::pauseResume() { send(QStringLiteral("/ctrl/pauseresumeall")); }

void ScsClient::fireCue(const QString& cueId) {
    if (!m_enabled || !m_address)
        return;
    // /cue/go carries the cue identifier followed by the control password.
    lo_send(m_address, "/cue/go", "ss", cueId.toUtf8().constData(),
            m_password.toUtf8().constData());
    emit sent(QStringLiteral("/cue/go"));
}

void ScsClient::loadFromSettings() {
    QSettings settings;
    settings.beginGroup("SCS");
    m_host = settings.value("host", m_host).toString();
    m_port = settings.value("port", m_port).toInt();
    m_enabled = settings.value("enabled", false).toBool();
    m_password = settings.value("password", m_password).toString();
    settings.endGroup();
    rebuildAddress();
}

void ScsClient::saveToSettings() {
    QSettings settings;
    settings.beginGroup("SCS");
    settings.setValue("host", m_host);
    settings.setValue("port", m_port);
    settings.setValue("enabled", m_enabled);
    settings.setValue("password", m_password);
    settings.endGroup();
}

} // namespace OpenMix
