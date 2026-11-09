#pragma once

#include <QByteArray>
#include <QString>

// Incremental, bounded parsing of the trusted helper's line protocol.
class SteamSessionOutput {
public:
    explicit SteamSessionOutput(const QString &identity)
        : m_marker("STEAM_GAME_PROCESS_OBSERVED " + identity.toLatin1()) {}

    bool consume(const QByteArray &bytes) {
        bool observed = false;
        for (const char c : bytes) {
            if (c == '\n') {
                if (!m_discard && m_line == m_marker) observed = true;
                m_line.clear();
                m_discard = false;
            } else if (!m_discard) {
                if (m_line.size() >= 256) { m_line.clear(); m_discard = true; }
                else m_line.append(c);
            }
        }
        return observed;
    }

private:
    QByteArray m_marker;
    QByteArray m_line;
    bool m_discard = false;
};
