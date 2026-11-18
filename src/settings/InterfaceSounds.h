#pragma once
#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class QSoundEffect;

// Profile-owned UI feedback. Reads declarative Kodi resource packs only;
// never executes add-ons or writes Kodi configuration.
class InterfaceSounds final : public QObject
{
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
    InterfaceSounds(const QString& settingsPath,
                    const QStringList& addonRoots,
                    QObject* parent = nullptr,
                    const QString& optionsPath = ":/config/interface-sounds.json",
                    bool audioBackend = true);
    ~InterfaceSounds() override;
    bool
    enabled() const
    {
        return m_enabled;
    }
    int
    volume() const
    {
        return m_volume;
    }
    bool
    duringPlayback() const
    {
        return m_duringPlayback;
    }
    QString
    theme() const
    {
        return m_theme;
    }
    QString
    themeName() const;
    QVariantList
    themes() const
    {
        return m_themes;
    }
    QVariantList
    fields() const
    {
        return m_options.value("fields").toList();
    }
    QString
    errorString() const
    {
        return m_error;
    }
    bool
    previewing() const
    {
        return m_previewTimer.isActive();
    }
    bool
    suppressed() const
    {
        return m_suppressed;
    }
    bool
    playbackActive() const
    {
        return m_playbackActive;
    }
    bool
    muted() const
    {
        return m_muted;
    }
    void
    setSuppressed(bool value);
    void
    setPlaybackActive(bool value);
    void
    setMuted(bool value);
    Q_INVOKABLE bool
    setEnabled(bool value);
    Q_INVOKABLE bool
    setVolume(int value);
    Q_INVOKABLE bool
    setDuringPlayback(bool value);
    Q_INVOKABLE bool
    selectTheme(const QString& id);
    Q_INVOKABLE void
    refreshThemes();
    Q_INVOKABLE void
    playCommand(int command);
    Q_INVOKABLE void
    playEvent(const QString& event);
    Q_INVOKABLE bool
    preview(const QString& themeId);
    Q_INVOKABLE void
    stop();
    static QVariantMap
    readKodiTheme(const QString& directory, QString* error = nullptr, bool includeAudio = true);
    static bool
    validateThemeDirectory(const QString& directory, QString& root, QByteArray& addon, QString* error);
    static bool
    validateAddonIdentity(const QByteArray& addon, const QString& root, QString& id, QString& name,
                          QString* error);
    static bool
    findSoundResourceExtension(const QByteArray& addon, QString* error);
    static bool
    parseSoundMappings(const QString& root, QMap<QString, QString>& files, QString* error);
    static bool
    parseSoundFields(QXmlStreamReader& xml, QMap<QString, QString>& fields, QString* error);
    static bool
    processSoundAction(const QString& group, const QMap<QString, QString>& fields,
                       QMap<QString, QString>& files, QString* error);
    static bool
    validateAndLoadSounds(const QString& root, const QMap<QString, QString>& files, QVariantMap& sounds,
                          bool includeAudio, QString* error);
    static bool
    validOptions(const QVariantMap& options);
    static bool
    validateDefaults(const QVariantMap& defaults);
    static bool
    validateTones(const QVariantMap& tones);
    static bool
    validateEvents(const QVariantList& events, const QVariantMap& tones);
    static bool
    validateEventEntry(const QVariantMap& entry, const QVariantMap& tones, QSet<QString>& ids);
    static bool
    validateEventAliases(const QVariantList& list);
    static bool
    validateFields(const QVariantList& fields);
    static bool
    validWave(const QByteArray& bytes);
    static bool
    validWaveFormat(const QByteArray& bytes, qsizetype& p, quint32& rate, quint16& frame);
    static bool
    validWaveData(const QByteArray& bytes, qsizetype& p, quint32 dataSize, quint16 frame, bool& dataFound);
    static bool
    validWaveFinal(bool dataFound, quint32 dataSize, quint32 rate, quint16 frame);
    static quint16
    u16(const QByteArray& bytes, qsizetype p);
    static quint32
    u32(const QByteArray& bytes, qsizetype p);
  signals:
    void
    changed();
    // Emitted only after policy, file and rate checks; not an audibility claim.
    void
    cueRequested(const QString& event, const QUrl& source, qreal volume);

  private:
    bool
    save(bool enabled, int volume, bool duringPlayback, const QString& theme);
    bool
    prepare(const QString& id);
    bool
    prepareThemeSounds(const QString& id, QMap<QString, QByteArray>& sounds) const;
    bool
    prepareEventSounds(const QString& id, const QMap<QString, QByteArray>& sounds,
                       QMap<QString, QUrl>& sources, QMap<QString, int>& durations, QSet<QString>& cacheFiles);
    bool
    createSoundEffects(const QMap<QString, QUrl>& sources);
    void
    request(const QString& event, bool preview);
    void
    silence();
    bool
    quiet() const;
    QString
    effectiveTheme() const;
    QString m_path, m_theme = "resource.uisounds.kodi", m_prepared, m_error, m_failedTheme;
    QStringList m_roots;
    QVariantMap m_options;
    QVariantList m_themes;
    QMap<QString, QString> m_themePaths;
    QMap<QString, QUrl> m_sources;
    QMap<QString, int> m_durations;
    QSet<QString> m_cacheFiles;
    QMap<QString, QSoundEffect*> m_effects;
    QSoundEffect* m_playing = nullptr;
    QString m_pendingEvent, m_lastEvent;
    QTemporaryDir m_cache;
    QTimer m_previewTimer;
    QElapsedTimer m_lastCue;
    int m_volume = 20, m_previewIndex = 0;
    bool m_enabled = true, m_duringPlayback = false, m_suppressed = false, m_playbackActive = false, m_muted = false,
         m_audioBackend = true;
};
