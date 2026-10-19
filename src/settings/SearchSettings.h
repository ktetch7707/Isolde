#pragma once

#include <QObject>

/**
 * @brief Universal-search Web ON/OFF preference (unified plan, Section 13).
 *
 * Web OFF is the enforced default and a network boundary: search must only use
 * titles/metadata already available from the profile's Kodi library and issue no
 * external search/metadata/provider requests. The toggle is remembered between
 * sessions via the shared QSettings "search" group. The Web ON path is a future
 * slice gated on a verified discovery provider; enabling it today only flips the
 * persisted flag and is reported honestly by the UI.
 */
class SearchSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool webEnabled READ webEnabled WRITE setWebEnabled NOTIFY changed)

public:
    explicit SearchSettings(QObject *parent = nullptr);

    [[nodiscard]] bool webEnabled() const { return m_webEnabled; }
    void setWebEnabled(bool enabled);
    Q_INVOKABLE void toggleWebEnabled();

signals:
    void changed();

private:
    bool m_webEnabled = false;
};