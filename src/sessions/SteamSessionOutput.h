#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>

// Incremental, bounded parsing of the trusted helper's line protocol.
class SteamSessionOutput {
public:
    explicit SteamSessionOutput(const QString &identity)
        : m_identity(identity), m_marker("STEAM_GAME_PROCESS_OBSERVED " + identity.toLatin1()) {}

    bool consume(const QByteArray &bytes) {
        bool observed = false;
        for (const char c : bytes) {
            if (c == '\n') {
                if (!m_discard && m_line == m_marker) observed = true;
                if (!m_discard && m_line.startsWith("STEAM_PROVIDER_STATE "))
                    parseState(m_line.mid(21));
                m_line.clear();
                m_discard = false;
            } else if (!m_discard) {
                if (m_line.size() >= 2048) { m_line.clear(); m_discard = true; }
                else m_line.append(c);
            }
        }
        return observed;
    }

    QVariantMap takeState() { auto result=m_state;m_state.clear();return result; }

private:
    void parseState(const QByteArray &json) {
        QJsonParseError error;
        const auto document=QJsonDocument::fromJson(json,&error);
        if(error.error!=QJsonParseError::NoError || !document.isObject()
            || document.toJson(QJsonDocument::Compact)!=json)return;
        const auto obj=document.object();
        const QMap<QString,QStringList> allowed{
            {"phase",{"starting","process_observed","ended","canceled","failed"}},
            {"process",{"waiting","observed","gone"}},
            {"installation",{"unknown","verified","unavailable"}},
            {"client",{"unknown","available","unavailable"}},
            {"ownership",{"unknown","owned","not_owned"}},
            {"account",{"unknown","signed_in","signed_out"}},
            {"connection",{"unknown","online","offline"}},
            {"offline",{"unknown","ready","unavailable"}},
            {"exit",{"unknown","exited","crashed"}},
            {"local_save",{"unknown","flushed","failed"}},
            {"cloud_save",{"unknown","synced","pending","failed"}},
            {"session_end",{"unknown","normal","canceled","incomplete","recovery_required"}},
            {"error",{"none","bootstrap_failed","ownership_denied","auth_required","offline_unavailable","launch_failed","install_check_failed","client_unavailable"}}
        };
        if(obj.size()!=allowed.size()+2 || !obj.value("version").isDouble()
            || obj.value("version").toDouble()!=2 || obj.value("appId").toString()!=m_identity)return;
        for(auto it=allowed.cbegin();it!=allowed.cend();++it)
            if(!obj.value(it.key()).isString() || !it.value().contains(obj.value(it.key()).toString()))return;
        m_state=obj.toVariantMap();
    }

    QString m_identity;
    QByteArray m_marker;
    QByteArray m_line;
    QVariantMap m_state;
    bool m_discard = false;
};
