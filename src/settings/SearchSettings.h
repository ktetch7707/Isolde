#pragma once

#include <QObject>
#include <QString>

/**
 * @brief Universal-search Web ON/OFF preference (unified plan, Section 13).
 *
 * Web OFF is the enforced default and a network boundary: search must only use
 * titles/metadata already available from the profile's Kodi library and issue no
 * external search/metadata/provider requests. The toggle is remembered between
 * sessions via the shared QSettings "search" group. The Web ON path is a future
 * slice gated on a verified discovery provider; enabling it today only flips the
 * persisted flag and is reported honestly by the UI. Each runtime captures its
 * active profile; atomic sync failure retains the previous visible/stored value.
 */
class SearchSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool webEnabled READ webEnabled WRITE setWebEnabled NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)

public:
    explicit SearchSettings(QObject *parent = nullptr, const QString &settingsPath = {}, const QString &profile = {});

    [[nodiscard]] bool webEnabled() const { return m_webEnabled; }
    QString errorString() const { return m_error; }
    void setWebEnabled(bool enabled);
    Q_INVOKABLE void toggleWebEnabled();

signals:
    void changed();

private:
    bool m_webEnabled = false;
    QString m_settingsPath, m_group, m_error;
};
