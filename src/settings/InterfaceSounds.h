#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QTimer>
#include <QUrl>
#include <QSet>

class QSoundEffect;

// Profile-owned UI feedback. Reads declarative Kodi resource packs only;
// never executes add-ons or writes Kodi configuration.
class InterfaceSounds final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(int volume READ volume NOTIFY changed)
    Q_PROPERTY(bool duringPlayback READ duringPlayback NOTIFY changed)
    Q_PROPERTY(QString theme READ theme NOTIFY changed)
    Q_PROPERTY(QString themeName READ themeName NOTIFY changed)
    Q_PROPERTY(QVariantList themes READ themes NOTIFY changed)
    Q_PROPERTY(QVariantList fields READ fields CONSTANT)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)
    Q_PROPERTY(bool previewing READ previewing NOTIFY changed)
    Q_PROPERTY(bool suppressed READ suppressed WRITE setSuppressed NOTIFY changed)
    Q_PROPERTY(bool playbackActive READ playbackActive WRITE setPlaybackActive NOTIFY changed)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY changed)
public:
    InterfaceSounds(const QString &settingsPath, const QStringList &addonRoots,
                    QObject *parent = nullptr, const QString &optionsPath = ":/config/interface-sounds.json",
                    bool audioBackend = true);
    ~InterfaceSounds() override;
    bool enabled() const { return m_enabled; }
    int volume() const { return m_volume; }
    bool duringPlayback() const { return m_duringPlayback; }
    QString theme() const { return m_theme; }
    QString themeName() const;
    QVariantList themes() const { return m_themes; }
    QVariantList fields() const { return m_options.value("fields").toList(); }
    QString errorString() const { return m_error; }
    bool previewing() const { return m_previewTimer.isActive(); }
    bool suppressed() const { return m_suppressed; }
    bool playbackActive() const { return m_playbackActive; }
    bool muted() const { return m_muted; }
    void setSuppressed(bool value);
    void setPlaybackActive(bool value);
    void setMuted(bool value);
    Q_INVOKABLE bool setEnabled(bool value);
    Q_INVOKABLE bool setVolume(int value);
    Q_INVOKABLE bool setDuringPlayback(bool value);
    Q_INVOKABLE bool selectTheme(const QString &id);
    Q_INVOKABLE void refreshThemes();
    Q_INVOKABLE void playCommand(int command);
    Q_INVOKABLE void playEvent(const QString &event);
    Q_INVOKABLE bool preview(const QString &themeId);
    Q_INVOKABLE void stop();
    static QVariantMap readKodiTheme(const QString &directory, QString *error = nullptr, bool includeAudio = true);
    static bool validOptions(const QVariantMap &options);
    static bool validWave(const QByteArray &bytes);
signals:
    void changed();
    // Emitted only after policy, file and rate checks; not an audibility claim.
    void cueRequested(const QString &event, const QUrl &source, qreal volume);
private:
    bool save(bool enabled, int volume, bool duringPlayback, const QString &theme);
    bool prepare(const QString &id);
    void request(const QString &event, bool preview);
    void silence();
    bool quiet() const;
    QString effectiveTheme() const;
    QString m_path, m_theme = "resource.uisounds.kodi", m_prepared, m_error, m_failedTheme;
    QStringList m_roots;
    QVariantMap m_options;
    QVariantList m_themes;
    QMap<QString,QString> m_themePaths;
    QMap<QString,QUrl> m_sources;
    QMap<QString,int> m_durations;
    QSet<QString> m_cacheFiles;
    QMap<QString,QSoundEffect *> m_effects;
    QSoundEffect *m_playing = nullptr;
    QString m_pendingEvent, m_lastEvent;
    QTemporaryDir m_cache;
    QTimer m_previewTimer;
    QElapsedTimer m_lastCue;
    int m_volume=20, m_previewIndex=0;
    bool m_enabled=true, m_duringPlayback=false, m_suppressed=false,
        m_playbackActive=false, m_muted=false, m_audioBackend=true;
};
