#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QTimer>
#include <QVariantList>
#include <QJsonObject>

class KodiMedia final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList movies READ movies NOTIFY changed)
    Q_PROPERTY(QVariantList items READ movies NOTIFY changed)
    Q_PROPERTY(QString mediaKind READ mediaKind NOTIFY changed)
    Q_PROPERTY(QString browseTitle READ browseTitle NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE selectIndex NOTIFY changed)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY changed)
    Q_PROPERTY(int pageStart READ pageStart NOTIFY changed)
    Q_PROPERTY(int totalMovies READ totalMovies NOTIFY changed)
    Q_PROPERTY(bool hasNextPage READ hasNextPage NOTIFY changed)
    Q_PROPERTY(bool playbackActive READ playbackActive NOTIFY changed)
    Q_PROPERTY(QString playbackStatus READ playbackStatus NOTIFY changed)
    Q_PROPERTY(QString viewKind READ viewKind NOTIFY changed)
    Q_PROPERTY(QString viewValue READ viewValue NOTIFY changed)
    Q_PROPERTY(QVariantList collections READ collections NOTIFY changed)
    Q_PROPERTY(QString collectionStatus READ collectionStatus NOTIFY changed)
public:
    explicit KodiMedia(const QString &profile, QObject *parent = nullptr, const QString &testDirectory = {});
    ~KodiMedia() override;
    QString status() const { return m_status; }
    QVariantList movies() const { return m_movies; }
    QVariantList items() const { return m_movies; }
    QString mediaKind() const { return m_mediaKind; }
    QString browseTitle() const;
    int selectedIndex() const { return m_selectedIndex; }
    bool canGoBack() const { return !m_history.isEmpty(); }
    Q_INVOKABLE void selectIndex(int index);
    Q_INVOKABLE void openLibrary(const QString &kind);
    Q_INVOKABLE void openView(const QString &kind, const QString &view, const QString &value = {});
    Q_INVOKABLE void openShowResult(const QString &query, int tvShowId, int pageStart = 0);
    Q_INVOKABLE void playMedia(const QString &kind, int id, bool resume = true);
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE bool goBack();
    Q_INVOKABLE void playSelected(bool resume = true);
    bool busy() const { return m_busy; }
    bool connected() const { return m_connected; }
    QString scanStatus() const { return m_scanStatus; }
    int pageStart() const { return m_pageStart; }
    int totalMovies() const { return m_totalMovies; }
    bool hasNextPage() const { return m_connected && m_pageStart + m_pageCount < m_totalMovies; }
    Q_INVOKABLE void nextPage();
    Q_INVOKABLE void previousPage();
    bool playbackActive() const { return m_playbackActive; }
    QString playbackStatus() const { return m_playbackStatus; }
    QString viewKind() const { return m_viewKind; }
    QString viewValue() const { return m_viewValue; }
    QVariantList collections() const;
    QString collectionStatus() const { return m_collectionStatus; }
    Q_INVOKABLE void setView(const QString &kind, const QString &value = {});
    Q_INVOKABLE void saveCollection(const QString &name);
    Q_INVOKABLE void createManualMovieCollection(const QString &name, int movieId);
    Q_INVOKABLE void addMovieToCollection(const QString &collectionId, int movieId);
    Q_INVOKABLE void removeMovieFromCollection(const QString &collectionId, int movieId);
    Q_INVOKABLE void createManualCollection(const QString &name, const QString &mediaType, int itemId);
    Q_INVOKABLE void addManualCollectionMember(const QString &collectionId, const QString &mediaType, int itemId);
    Q_INVOKABLE void removeManualCollectionMember(const QString &collectionId, int itemId);
    Q_INVOKABLE void renameCollection(const QString &id, const QString &name);
    Q_INVOKABLE void removeCollection(const QString &id);
    Q_INVOKABLE void openCollection(const QString &id);
    Q_INVOKABLE void configureShelf(const QString &id, bool pinned, int count = 6);
    Q_INVOKABLE void moveCollection(const QString &id, int offset);
    Q_INVOKABLE void playCollectionMovie(const QString &id, int movieId);
    Q_INVOKABLE void openCollectionShow(const QString &id, int tvShowId);
    Q_INVOKABLE void playMovie(int movieId, bool resume = true);
    Q_INVOKABLE void playbackControl(const QString &action);
    void presentationFailed();
    void invalidate(const QString &reason);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void scanMovies();
    void quitIfIdle();
signals:
    void changed();
    void collectionsChanged();
    void quitReply(bool accepted);
    void playbackStarted();
    void playbackReturned(bool home);
private:
    void request(const QString &method, const QJsonObject &params = {});
    void consume();
    void fail(const QString &message);
    void releaseSocket();
    void loadPage(int start);
    void loadManualPage(int start);
    void loadNextManualMovie();
    void beginRead(int start);
    void playLibraryItem(const QString &kind, int id, bool resume, bool requireListed = true);
    bool canNavigate() const;
    struct BrowseState { QString kind, view, value, showTitle; int show, season, start, selected; };
    BrowseState browseState() const;
    void restoreBrowse(const BrowseState &state);
    bool storeCollections(const QVariantList &entries);
    QString m_profile, m_viewKind = "all", m_viewValue, m_collectionStatus;
    QString m_directory, m_status = "Open Kodi with the Isolde bridge enabled to browse this profile's library.";
    QLocalSocket m_socket;
    QTimer m_timeout;
    QByteArray m_received, m_request;
    QVariantList m_movies;
    QVariantList m_recentResults;
    bool m_recentEpisodes = false;
    int m_descriptor = -1, m_id = 0, m_stage = 0;
    quint64 m_generation = 0;
    bool m_busy = false, m_connected = false;
    QString m_scanStatus;
    bool m_observedScan = false;
    int m_scanPolls = 0;
    int m_pageStart = 0, m_requestedStart = 0, m_totalMovies = 0, m_pageCount = 0;
    bool m_playbackActive = false, m_playbackVisible = false;
    QString m_playbackStatus;
    QString m_pendingControl;
    QString m_presentationError;
    QTimer m_playbackPoll;
    int m_pendingMovie = 0, m_pendingShow = 0;
    QString m_pendingDirectKind;
    int m_pendingDirectId = 0;
    bool m_pendingDirectResume = true;
    QVariantList m_manualMembers;
    QVariantList m_manualResults;
    int m_manualNextIndex = 0;
    int m_manualCurrentId = 0, m_manualMissingCount = 0;
    QString m_manualType = "movie";
    QString m_mediaKind = "movies", m_showTitle;
    int m_showId = -1, m_season = -1, m_selectedIndex = 0;
    QList<BrowseState> m_history;
};
