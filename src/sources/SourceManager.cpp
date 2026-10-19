#include "sources/SourceManager.h"
#include "home/ComponentSchema.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <algorithm>

namespace {

QString stableSourceId(const QString &path)
{
    const auto digest = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex().left(12);
    return QStringLiteral("source-") + QString::fromLatin1(digest);
}

QString classifyPath(const QFileInfo &info)
{
    const auto name = info.fileName().toLower();
    const QDir directory(info.absoluteFilePath());
    if (info.isDir() && (directory.exists(QStringLiteral("VIDEO_TS"))
                         || directory.exists(QStringLiteral("BDMV"))
                         || name.contains(QStringLiteral("dvd"))
                         || name.contains(QStringLiteral("bluray"))
                         || name.contains(QStringLiteral("blu-ray"))))
        return QStringLiteral("movie-disc");
    if (info.isDir() && (directory.exists(QStringLiteral("steamapps"))
                         || directory.exists(QStringLiteral("PS3_GAME"))
                         || directory.exists(QStringLiteral("PS4UPDATE.PUP"))
                         || name.contains(QStringLiteral("game"))
                         || name.contains(QStringLiteral("ps5"))
                         || name.contains(QStringLiteral("xbox"))))
        return QStringLiteral("game-disc");
    if (info.isDir())
        return QStringLiteral("media-folder");
    return QStringLiteral("file-source");
}

QStringList defaultProbeRoots()
{
    const auto user = qEnvironmentVariable("USER");
    QStringList roots;
    if (!user.isEmpty()) {
        roots << QStringLiteral("/media/") + user
              << QStringLiteral("/run/media/") + user;
    }
    roots << QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    return roots;
}

QString actionTypeForPath(const QFileInfo &info)
{
    if (info.isDir())
        return QStringLiteral("open-folder");
    return QStringLiteral("open-file");
}

QString symbolForPath(const QFileInfo &info)
{
    const auto classification = classifyPath(info);
    if (classification == QStringLiteral("movie-disc"))
        return QStringLiteral("◉");
    if (classification == QStringLiteral("game-disc"))
        return QStringLiteral("◆");
    if (info.isDir())
        return QStringLiteral("▣");
    return QStringLiteral("▤");
}

QVariantMap sourceTileForPath(const QFileInfo &info, const QString &openWith)
{
    const auto path = info.absoluteFilePath();
    const auto actionType = actionTypeForPath(info);
    const auto classification = classifyPath(info);
    const auto name = info.fileName().isEmpty() ? path : info.fileName();
    const auto presentationMode = classification == QStringLiteral("game-disc")
        ? QStringLiteral("disc-game")
        : classification == QStringLiteral("movie-disc") ? QStringLiteral("disc-media") : QStringLiteral("media");
    const QVariantMap action{{QStringLiteral("type"), actionType},
                             {QStringLiteral("target"), path},
                             {QStringLiteral("openWith"), openWith}};
    return ComponentSchema::component(QVariantMap{{QStringLiteral("kind"), QStringLiteral("action")},
                       {QStringLiteral("contextual"), true},
                       {QStringLiteral("id"), stableSourceId(path)},
                       {QStringLiteral("name"), name},
                       {QStringLiteral("section"), QStringLiteral("Detected Sources")},
                       {QStringLiteral("sourceType"), classification},
                       {QStringLiteral("target"), path},
                       {QStringLiteral("openWith"), openWith},
                       {QStringLiteral("symbol"), symbolForPath(info)},
                       {QStringLiteral("accent"), classification == QStringLiteral("game-disc") ? QStringLiteral("#8B5CF6") : QStringLiteral("#06B6D4")},
                       {QStringLiteral("presentationMode"), presentationMode},
                       {QStringLiteral("networkRequirement"), QStringLiteral("none")},
                       {QStringLiteral("size"), QStringLiteral("standard")},
                       {QStringLiteral("actionType"), actionType},
                       {QStringLiteral("action"), action}}, true);
}

} // namespace

SourceManager::SourceManager(AppRegistry *registry, QObject *parent) : QObject(parent), m_registry(registry)
{
    loadState();
    m_probeRoots = defaultProbeRoots();
    m_refreshTimer.setInterval(5000);
    m_refreshTimer.setSingleShot(false);
    connect(&m_refreshTimer, &QTimer::timeout, this, &SourceManager::refresh);
    m_refreshTimer.start();
    refresh();
}

bool SourceManager::automaticSourcesEnabled() const { return m_automaticSourcesEnabled; }

void SourceManager::setAutomaticSourcesEnabled(bool enabled)
{
    if (m_automaticSourcesEnabled == enabled)
        return;
    m_automaticSourcesEnabled = enabled;
    saveState();
    refresh();
}

QVariantList SourceManager::items() const
{
    return m_automaticSourcesEnabled ? m_sources : QVariantList{};
}

QVariantList SourceManager::sections() const
{
    const auto sourceItems = items();
    if (sourceItems.isEmpty())
        return {};
    return QVariantList{QVariantMap{{QStringLiteral("id"), QStringLiteral("detected-sources")},
                                    {QStringLiteral("name"), QStringLiteral("Detected Sources")},
                                    {QStringLiteral("items"), sourceItems}}};
}

QVariantList SourceManager::sources() const
{
    return m_sources;
}

int SourceManager::visibleSourceCount() const
{
    return items().size();
}

void SourceManager::refresh()
{
    const auto next = scanSources();
    if (next == m_sources)
        return;
    m_sources = next;
    emit sourcesChanged();
}

bool SourceManager::hideSource(const QString &id)
{
    if (id.trimmed().isEmpty())
        return false;
    m_hiddenSources.insert(id);
    saveState();
    refresh();
    return true;
}

bool SourceManager::markPermanent(const QString &id)
{
    if (id.trimmed().isEmpty())
        return false;
    m_permanentSources.insert(id);
    saveState();
    refresh();
    return true;
}

bool SourceManager::forgetSourceDecision(const QString &id)
{
    if (id.trimmed().isEmpty())
        return false;
    const bool hiddenChanged = m_hiddenSources.remove(id);
    const bool permanentChanged = m_permanentSources.remove(id);
    const bool changed = hiddenChanged || permanentChanged;
    if (!changed)
        return true;
    saveState();
    refresh();
    return true;
}

QVariantMap SourceManager::sourceAction(const QString &id) const
{
    for (const auto &value : m_sources) {
        const auto source = value.toMap();
        if (source.value(QStringLiteral("id")).toString() == id)
            return source.value(QStringLiteral("action")).toMap();
    }
    return {};
}

void SourceManager::setProbeRootsForTests(const QStringList &roots)
{
    m_probeRoots = roots;
    refresh();
}

QVariantList SourceManager::scanSources() const
{
    QVariantList result;
    for (const auto &root : m_probeRoots) {
        const QDir directory(root);
        if (!directory.exists())
            continue;
        const auto entries = directory.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
        for (const auto &entry : entries) {
            const auto actionType = actionTypeForPath(entry);
            const auto handler = defaultHandlerForAction(actionType);
            if (handler.isEmpty())
                continue;
            const auto tile = sourceTileForPath(entry, handler);
            const auto id = tile.value(QStringLiteral("id")).toString();
            if (!m_hiddenSources.contains(id) && !m_permanentSources.contains(id))
                result.append(tile);
        }
    }
    return result;
}

QString SourceManager::defaultHandlerForAction(const QString &actionType) const
{
    if (!m_registry)
        return {};
    for (const auto &appValue : m_registry->apps()) {
        const auto app = appValue.toMap();
        if (app.value(QStringLiteral("handledActions")).toStringList().contains(actionType))
            return app.value(QStringLiteral("id")).toString();
    }
    return {};
}

void SourceManager::loadState()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("sources"));
    m_automaticSourcesEnabled = settings.value(QStringLiteral("automaticSourcesEnabled"), true).toBool();
    const auto hidden = settings.value(QStringLiteral("hiddenSources")).toStringList();
    const auto permanent = settings.value(QStringLiteral("permanentSources")).toStringList();
    m_hiddenSources = QSet<QString>(hidden.begin(), hidden.end());
    m_permanentSources = QSet<QString>(permanent.begin(), permanent.end());
    settings.endGroup();
}

void SourceManager::saveState() const
{
    auto hidden = m_hiddenSources.values();
    auto permanent = m_permanentSources.values();
    std::sort(hidden.begin(), hidden.end());
    std::sort(permanent.begin(), permanent.end());
    QSettings settings;
    settings.beginGroup(QStringLiteral("sources"));
    settings.setValue(QStringLiteral("automaticSourcesEnabled"), m_automaticSourcesEnabled);
    settings.setValue(QStringLiteral("hiddenSources"), hidden);
    settings.setValue(QStringLiteral("permanentSources"), permanent);
    settings.endGroup();
}
