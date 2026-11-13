#include "KodiArtwork.h"
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QBuffer>
#include <QImageReader>
#include <QFile>
#include <QUrl>
#include <QImage>
#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <unistd.h>
#endif

KodiArtwork::KodiArtwork(const QString &profile, QObject *parent, const QString &testDirectory) : QObject(parent)
{
    m_directory = testDirectory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + "/native-profiles/" + profile + "/kodi/home/.kodi/userdata/isolde" : testDirectory;
    m_debounce.setSingleShot(true); m_debounce.setInterval(160);
    m_timeout.setSingleShot(true); m_timeout.setInterval(3000);
    m_socket.setReadBufferSize(1500001);
    connect(&m_debounce, &QTimer::timeout, this, &KodiArtwork::begin);
    connect(&m_timeout, &QTimer::timeout, this, [this] { finish("Artwork unavailable · retry by selecting again"); });
    connect(&m_socket, &QLocalSocket::connected, this, [this] { m_socket.write(m_request); });
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError error) {
        if (m_active && !(error == QLocalSocket::PeerClosedError && m_received.endsWith('\n'))) finish("Artwork unavailable");
    });
    connect(&m_socket, &QLocalSocket::readyRead, this, [this] {
        if (!m_active) return;
        m_received += m_socket.readAll();
        if (m_received.size() > 1500000) { finish("Artwork exceeds display limit"); return; }
        if (!m_received.endsWith('\n')) return;
        const auto reply = QJsonDocument::fromJson(m_received).object();
        const auto result = reply.value("result").toObject();
        const auto mime = result.value("mime").toString(), encoded = result.value("data").toString();
        if (reply.value("jsonrpc") != "2.0" || reply.value("id").toInt() != m_requestId || reply.contains("error")) {
            finish(m_setting?"Kodi could not confirm the poster change. Refresh before retrying.":"Artwork unavailable"); return;
        }
        if(m_setting){
            if(!result.value("updated").toBool()){finish("Kodi could not confirm the poster change.");return;}
            const auto source="data:image/png;base64,"+QString::fromLatin1(m_poster.toBase64());
            cancel();m_setting=false;m_poster.clear();m_source=source;m_status="Poster updated in Kodi";emit changed();return;
        }
        if (mime.isEmpty() && encoded.isEmpty()) { finish("No local artwork cached in Kodi"); return; }
        const auto raw = QByteArray::fromBase64(encoded.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
        QBuffer buffer; buffer.setData(raw); buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        const auto size = reader.size();
        if (!QStringList{"image/png", "image/jpeg"}.contains(mime) || raw.isEmpty() || raw.size() > 1024 * 1024
            || !reader.canRead() || size.width() < 1 || size.height() < 1 || size.width() > 4096 || size.height() > 4096) {
            finish("Artwork format unavailable"); return;
        }
        const auto source = "data:" + mime + ";base64," + encoded;
        cancel(); m_source = source; m_status.clear(); emit changed();
    });
}
KodiArtwork::~KodiArtwork() { cancel(); }
void KodiArtwork::cancel()
{
    m_active = false; m_debounce.stop(); m_timeout.stop(); m_socket.abort();
#ifdef Q_OS_LINUX
    if (m_descriptor >= 0) ::close(m_descriptor);
#endif
    m_descriptor = -1; m_received.clear();
}
void KodiArtwork::clear()
{
    cancel(); m_setting=false;m_poster.clear();m_item = -1; m_kind.clear(); m_source.clear(); m_status.clear(); emit changed();
}
bool KodiArtwork::setPoster(const QString &kind,int id,const QString &managedId)
{
    if(m_active && m_setting)return false;
    if(!m_resolver || !QStringList{"movie","tvshow"}.contains(kind) || id<1)return false;
    const auto source=m_resolver(managedId);const auto url=QUrl(source);
    if(source.isEmpty() || !url.isLocalFile())return false;
    QFile file(url.toLocalFile());
    if(!file.open(QIODevice::ReadOnly) || file.size()>8*1024*1024)return false;
    QImageReader reader(&file);const auto size=reader.size();
    if(!reader.canRead() || !size.isValid() || size.width()>4096 || size.height()>4096 || qint64(size.width())*size.height()>4*1024*1024)return false;
    const auto image=reader.read();QByteArray png;QBuffer output(&png);output.open(QIODevice::WriteOnly);
    if(image.isNull() || !image.save(&output,"PNG") || png.size()>1024*1024)return false;
    cancel();m_setting=true;m_kind=kind;m_item=id;m_poster=png;m_status="Saving the confirmed poster in Kodi…";emit changed();begin();return true;
}
void KodiArtwork::load(const QString &kind, int id)
{
    cancel(); m_setting=false;m_poster.clear();m_kind = kind; m_item = id; m_source.clear();
    if (id < 1 || !QStringList{"movie", "tvshow", "season", "episode", "album", "song", "channel"}.contains(kind)) { m_status = "No artwork selected"; emit changed(); return; }
    m_status = "Loading Kodi artwork…"; m_debounce.start(); emit changed();
}
void KodiArtwork::finish(const QString &status) { cancel(); m_source.clear(); m_status = status; emit changed(); }
void KodiArtwork::begin()
{
#ifdef Q_OS_LINUX
    m_descriptor = ::open(m_directory.toLocal8Bit().constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (m_descriptor < 0) { finish("Artwork unavailable"); return; }
    m_active = true;
    QJsonObject params{{"kind",m_kind},{"id",m_item}};
    if(m_setting)params.insert("data",QString::fromLatin1(m_poster.toBase64()));
    m_request = QJsonDocument(QJsonObject{{"jsonrpc", "2.0"}, {"id", ++m_requestId}, {"method", m_setting?"Isolde.SetPoster":"Isolde.GetArtwork"},
        {"params",params}}).toJson(QJsonDocument::Compact) + '\n';
    m_timeout.start();
    m_socket.connectToServer(QString("/proc/self/fd/%1/rpc.sock").arg(m_descriptor));
#else
    finish("Kodi artwork requires Linux");
#endif
}
