#pragma once
#include <QLocalSocket>
#include <QObject>
#include <QTimer>
#include <functional>

class KodiArtwork final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString source READ source NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
  public:
    explicit KodiArtwork(const QString& profile, QObject* parent = nullptr, const QString& testDirectory = {});
    ~KodiArtwork() override;
    QString
    source() const
    {
        return m_source;
    }
    QString
    status() const
    {
        return m_status;
    }
    Q_INVOKABLE void
    load(const QString& kind, int id);
    void loadBackdrop(const QString& kind, int id);
    Q_INVOKABLE void
    clear();
    void
    setManagedArtworkResolver(std::function<QString(const QString&)> resolver)
    {
        m_resolver = std::move(resolver);
    }
    Q_INVOKABLE bool
    setPoster(const QString& kind, int id, const QString& managedId);
    /** Loads fanart artwork for a media item.
     * @brief Loads fanart artwork.
     * @details Requests fanart from Kodi's art metadata via the bridge.
     * @param kind The media kind.
     * @param id The media item ID.
     */
    Q_INVOKABLE void
    loadFanart(const QString& kind, int id);
    /** Loads banner artwork for a media item.
     * @brief Loads banner artwork.
     * @details Requests banner from Kodi's art metadata via the bridge.
     * @param kind The media kind.
     * @param id The media item ID.
     */
    Q_INVOKABLE void
    loadBanner(const QString& kind, int id);
    /** Loads clearlogo artwork for a media item.
     * @brief Loads clearlogo artwork.
     * @details Requests clearlogo from Kodi's art metadata via the bridge.
     * @param kind The media kind.
     * @param id The media item ID.
     */
    Q_INVOKABLE void
    loadClearlogo(const QString& kind, int id);
    /** Loads cast portrait artwork for a media item.
     * @brief Loads cast portrait artwork.
     * @details Requests cast portrait from Kodi's art metadata via the bridge.
     * @param kind The media kind.
     * @param id The media item ID.
     */
    Q_INVOKABLE void
    loadCastPortrait(const QString& kind, int id);
  signals:
    void
    changed();

  private:
    void loadArtwork(const QString& kind, int id, bool backdrop);
    void loadArtwork(const QString& kind, int id, bool backdrop, const QString& role);
    void
    begin();
    void
    finish(const QString& status);
    void
    cancel();
    QLocalSocket m_socket;
    QTimer m_debounce, m_timeout;
    QString m_directory, m_kind, m_source, m_status;
    int m_item = -1, m_requestId = 0, m_descriptor = -1;
    bool m_active = false;
    bool m_setting = false;
    bool m_backdrop = false;
    QString m_role = "poster";
    std::function<QString(const QString&)> m_resolver;
    QByteArray m_poster;
    QByteArray m_received, m_request;
};
