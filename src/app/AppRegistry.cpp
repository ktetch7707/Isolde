#include "app/AppArtwork.h"
#include "app/AppRegistry.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QImageReader>
#include <QImage>
#include <QPixmap>
#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <utility>

AppRegistry::AppRegistry(QObject* parent) : QObject(parent) {}

static bool
validIconName(const AppDefinition& app)
{
    const auto value = app.launchConfiguration.value("icon");
    static const QRegularExpression name("^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$");
    return !value.isValid() || (value.metaType().id() == QMetaType::QString && name.match(value.toString()).hasMatch());
}

static QJsonObject
iconRole(const AppDefinition& app)
{
    static const auto roles = []()
    {
        QFile file(":/config/app-icons/roles.json");
        if (!file.open(QIODevice::ReadOnly))
            return QJsonObject();
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        return root["schemaVersion"].toInt() == 1 ? root["icons"].toObject() : QJsonObject();
    }();
    return roles[app.launchConfiguration.value("icon", app.id).toString()].toObject();
}
static QString
cacheAppIcon(const QByteArray& bytes, const QString& profile, const QRegularExpression& desktopName);

QString
AppRegistry::appIcon(const AppDefinition& app, int role)
{
    if (!qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
        return {};
    const auto packaged = ":/config/app-icons/" + app.launchConfiguration.value("icon", app.id).toString() + ".svg";
    if (app.type != "web" && app.type != "desktop" && app.type != "android" && !QFile::exists(packaged))
        return {};
    static const QRegularExpression desktopName("^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$");
    const auto name = resolveAppIconName(app, desktopName);
    QIcon icon;
    if (QFile::exists(packaged))
    {
        if (!resolvePackagedIcon(app, role, icon))
            return {};
    }
    else if (QDir::isAbsolutePath(name))
    {
        if (!resolveAbsolutePathIcon(name, icon))
            return {};
    }
    else
    {
        if (!desktopName.match(name).hasMatch())
            return {};
        if (!resolveThemeIcon(name, QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation), icon))
            return {};
    }
    QByteArray bytes;
    if (!finalizeIconImage(icon, packaged, role, bytes))
        return {};
    auto profile = QCoreApplication::instance()->property("isoldeProfile").toString();
    if (profile.isEmpty())
        profile = "default";
    const auto path = cacheAppIcon(bytes, profile, desktopName);
    if (path.isEmpty())
        return {};
    return QUrl::fromLocalFile(path).toString();
}


// Read the desktop file's icon without trusting or executing its launch command.
QString
AppRegistry::resolveAppIconName(const AppDefinition& app, const QRegularExpression& desktopName)
{
    auto name = app.launchConfiguration.value("icon", app.id).toString();
    const auto desktopId = app.launchConfiguration.value("desktopId").toString();
    QStringList candidates{desktopId};
    if (app.id != desktopId)
        candidates.append(app.id);
    const auto roots = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (auto candidate : candidates)
    {
        if (!desktopName.match(candidate).hasMatch())
            continue;
        if (!candidate.endsWith(".desktop"))
            candidate += ".desktop";
        for (const auto& root : roots)
        {
            const auto path = QDir(root).filePath("applications/" + candidate);
            if (!safeFile(path))
                continue;
            QSettings settings(path, QSettings::IniFormat);
            const auto supplied = settings.value("Desktop Entry/Icon").toString().trimmed();
            static const QSet<QString> generic{"application-x-executable", "application-default-icon",
                                               "package-x-generic", "system-run"};
            if (!supplied.isEmpty() && !generic.contains(supplied))
                return supplied;
        }
    }
    return name;
}

// Verify the reviewed foreground bytes before applying their approved ink mask.
bool
AppRegistry::resolvePackagedIcon(const AppDefinition& app, int role, QIcon& icon)
{
    const auto packaged = ":/config/app-icons/" + app.launchConfiguration.value("icon", app.id).toString() + ".svg";
    if (!role)
    {
        icon = QIcon(packaged);
        return true;
    }
    const auto metadata = iconRole(app);
    const auto relative = metadata["foreground"].toString();
    if (!QRegularExpression("^foreground/[a-z0-9][a-z0-9-]{0,127}\\.svg$").match(relative).hasMatch())
    {
        if (role == 2)
            return false;
        icon = QIcon(packaged);
        return true;
    }
    QFile file(":/config/app-icons/" + relative);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    auto svg = file.read(64 * 1024 + 1);
    svg.replace("\r\n", "\n");
    if (svg.size() > 64 * 1024 ||
        QString::fromLatin1(QCryptographicHash::hash(svg, QCryptographicHash::Sha256).toHex()) !=
            metadata["foregroundSha256"].toString())
        return false;
    QBuffer buffer(&svg);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "svg");
    reader.setScaledSize(QSize(512, 512));
    auto foreground = reader.read();
    if (foreground.isNull())
        return false;
    if (role == 2)
    {
        const QColor ink(metadata["inkColour"].toString());
        QList<QColor> excluded;
        if (!ink.isValid() || metadata["inkExclude"].toArray().size() > 4)
            return false;
        for (const auto& value : metadata["inkExclude"].toArray())
        {
            const QColor colour(value.toString());
            if (!colour.isValid())
                return false;
            excluded.append(colour);
        }
        foreground = AppArtwork::inkMask(foreground, ink, excluded);
    }
    icon = QIcon(QPixmap::fromImage(foreground));
    return true;
}

bool
AppRegistry::resolveAbsolutePathIcon(const QString& name, QIcon& icon)
{
    if (!safeFile(name))
        return false;
    QImageReader reader(name);
    const auto size = reader.size();
    if (!reader.canRead() || size.width() < 1 || size.height() < 1 || size.width() > 4096 || size.height() > 4096)
        return false;
    icon = QIcon(name);
    return true;
}

bool
AppRegistry::resolveThemeIcon(const QString& name, const QStringList& roots, QIcon& icon)
{
    icon = QIcon::fromTheme(name);
    if (!icon.isNull())
        return true;
    for (const auto& root : roots)
    {
        for (const auto& folder : QStringList{"icons/hicolor/256x256/apps/",
                                              "icons/hicolor/128x128/apps/",
                                              "icons/hicolor/64x64/apps/",
                                              "icons/hicolor/48x48/apps/",
                                              "icons/hicolor/scalable/apps/",
                                              "pixmaps/"})
        {
            for (const auto& extension : QStringList{".png", ".svg", ".xpm"})
            {
                const auto path = QDir(root).filePath(folder + name + extension);
                if (safeFile(path))
                {
                    icon = QIcon(path);
                    break;
                }
            }
            if (!icon.isNull())
                break;
        }
        if (!icon.isNull())
            break;
    }
    return true;
}

bool
AppRegistry::finalizeIconImage(const QIcon& icon, const QString& packaged, int role, QByteArray& bytes)
{
    if (icon.isNull())
        return false;
    auto image = icon.pixmap(512, 512).toImage();
    if (image.isNull())
        return false;
    if (role == 2 && !QFile::exists(packaged))
        return false; // Unreviewed rasters have no approved monochrome mask.
    if (role == 1 && !QFile::exists(packaged))
    {
        const auto cleaned = AppArtwork::cleanBackground(image);
        if (cleaned.accepted)
            image = cleaned.image;
    }
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG") || bytes.size() > 1024 * 1024)
        return false;
    return true;
}

bool
AppRegistry::safeFile(const QString& path)
{
    const QFileInfo info(path);
    if (!info.isFile() || info.size() < 1 || info.size() > 1024 * 1024)
        return false;
    const auto canonical = info.canonicalFilePath();
    const auto roots = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const auto& root : roots)
    {
        const auto base = QFileInfo(root).canonicalFilePath();
        if (!base.isEmpty() && canonical.startsWith(base + "/"))
            return true;
    }
    return false;
}

static QString
cacheAppIcon(const QByteArray& bytes, const QString& profile, const QRegularExpression& desktopName)
{
    if (!desktopName.match(profile).hasMatch())
        return {};
    const auto directory =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/profiles/" + profile + "/app-icons";
    if (!QDir().mkpath(directory))
        return {};
    const auto path = QDir(directory).filePath(
        QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) + ".png");
    if (QFileInfo(path).isSymLink())
        return {};
    QFile existing(path);
    if (!existing.open(QIODevice::ReadOnly) || existing.read(1024 * 1024 + 1) != bytes)
    {
        existing.close();
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            return {};
    }
    return path;
}

static QStringList
readStringArray(const QJsonArray& array)
{
    QStringList result;
    for (const auto& value : array)
    {
        const auto text = value.toString().trimmed();
        if (!text.isEmpty() && !result.contains(text))
            result.append(text);
    }
    return result;
}

static bool
isSafeWebUrl(const QUrl& url)
{
    return url.isValid() && (url.scheme() == QStringLiteral("https") || url.scheme() == QStringLiteral("qrc"));
}

static bool
isSafeWebOrigin(const QString& text)
{
    const QUrl url(text.trimmed());
    return isSafeWebUrl(url) &&
           (url.scheme() == QStringLiteral("qrc") ||
            (!url.host().isEmpty() && url.path().isEmpty() && url.query().isEmpty() && url.fragment().isEmpty()));
}

QVariantList
AppRegistry::apps() const
{
    QVariantList result;
    result.reserve(m_apps.size());
    for (const auto& app : m_apps)
    {
        result.append(QVariantMap{{"id", app.id},
                                  {"name", app.name},
                                  {"type", app.type},
                                  {"presentationMode", app.presentationMode},
                                  {"networkRequirement", app.networkRequirement},
                                  {"handledActions", app.handledActions},
                                  {"section", app.section},
                                  {"symbol", app.symbol},
                                  {"accent", app.accent.name()},
                                  {"logoSource", app.iconSource},
                                  {"cutoutLogoSource", app.cutoutIconSource},
                                  {"inkLogoSource", app.inkIconSource},
                                  {"iconOpticalScale", app.iconOpticalScale},
                                  {"iconContrastBackdrop", app.iconContrastBackdrop}});
    }
    return result;
}

QString
AppRegistry::errorString() const
{
    return m_errorString;
}

const AppDefinition*
AppRegistry::find(const QString& id) const
{
    for (const auto& app : m_apps)
        if (app.id == id)
            return &app;
    for (const auto& app : m_apps)
        if (app.legacyIds().contains(id))
            return &app;
    return nullptr;
}

bool
AppRegistry::addApp(const AppDefinition& app)
{
    if (!app.isValid() || find(app.id) || !validIconName(app))
        return false;
    for (const auto& alias : app.legacyIds())
        if (find(alias))
            return false;
    auto resolved = app;
    resolved.iconSource = appIcon(app);
    resolved.cutoutIconSource = appIcon(app, 1);
    resolved.inkIconSource = appIcon(app, 2);
    resolved.iconContrastBackdrop = iconRole(app)["contrastBackdrop"].toString();
    resolved.iconOpticalScale = qBound(.5, iconRole(app)["opticalScale"].toDouble(1), 1.4);
    m_apps.append(resolved);
    emit appsChanged();
    return true;
}

bool
AppRegistry::replaceApp(const AppDefinition& app)
{
    if (!app.isValid() || !validIconName(app))
        return false;
    for (auto& existing : m_apps)
        if (existing.id == app.id)
        {
            if (existing.type != app.type || existing.legacyIds() != app.legacyIds())
                return false;
            auto resolved = app;
            resolved.iconSource = appIcon(app);
            resolved.cutoutIconSource = appIcon(app, 1);
            resolved.inkIconSource = appIcon(app, 2);
            resolved.iconContrastBackdrop = iconRole(app)["contrastBackdrop"].toString();
            resolved.iconOpticalScale = qBound(.5, iconRole(app)["opticalScale"].toDouble(1), 1.4);
            existing = resolved;
            emit appsChanged();
            return true;
        }
    return false;
}

bool
AppRegistry::removeApp(const QString& id)
{
    for (qsizetype i = 0; i < m_apps.size(); ++i)
    {
        if (m_apps.at(i).id == id)
        {
            m_apps.removeAt(i);
            emit appsChanged();
            return true;
        }
    }
    return false;
}

bool
AppRegistry::load(const QString& path)
{
    return loadFromFile(resolveConfigPath(path, "config/apps.json", ":/config/apps.json"));
}

QString
AppRegistry::resolveConfigPath(const QString& path, const QString& defaultName, const QString& resourceName)
{
    QString resolvedPath = path;
    if (resolvedPath.isEmpty())
    {
        resolvedPath = QDir(QCoreApplication::applicationDirPath()).filePath(defaultName);
        if (!QFile::exists(resolvedPath))
            resolvedPath = resourceName;
    }
    return resolvedPath;
}

bool
AppRegistry::loadFromFile(const QString& resolvedPath)
{
    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        setError(tr("Unable to open app configuration: %1").arg(resolvedPath));
        return false;
    }
    return loadFromData(file.readAll());
}

bool
AppRegistry::loadFromData(const QByteArray& data)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray())
    {
        setError(tr("Invalid app configuration: %1").arg(parseError.errorString()));
        return false;
    }

    QVector<AppDefinition> loaded;
    QSet<QString> ids;
    for (const auto& value : document.array())
    {
        if (!validateAppEntry(value, loaded, ids))
            return false;
    }
    if (loaded.isEmpty())
    {
        setError(tr("App configuration contains no apps."));
        return false;
    }

    m_apps = std::move(loaded);
    setError({});
    emit appsChanged();
    return true;
}

bool
AppRegistry::validateAppEntry(const QJsonValue& value, QVector<AppDefinition>& loaded, QSet<QString>& ids)
{
    if (!value.isObject())
    {
        setError(tr("Every app entry must be an object."));
        return false;
    }
    const auto object = value.toObject();
    AppDefinition app;
    app.id = object.value("id").toString().trimmed();
    app.name = object.value("name").toString().trimmed();
    app.type = object.value("type").toString().trimmed();
    app.presentationMode = object.value("presentationMode").toString(QStringLiteral("native-tv")).trimmed();
    app.networkRequirement =
        object.value("networkRequirement")
            .toString(app.type == QStringLiteral("web") ? QStringLiteral("internet") : QStringLiteral("none"))
            .trimmed();
    app.handledActions = readStringArray(object.value(QStringLiteral("handledActions")).toArray());
    app.section = object.value("section").toString().trimmed();
    app.symbol = object.value("symbol").toString().trimmed();
    app.accent = QColor(object.value("accent").toString("#64748B"));
    app.launchConfiguration = object.toVariantMap();
    if (ids.contains(app.id) || !validIconName(app))
    {
        setError(tr("App IDs must be unique and icon names must be local."));
        return false;
    }
    if (!validateAppEntryFields(app, object))
        return false;
    if (!validateAppEntryOrigin(app, object))
        return false;
    app.iconSource = appIcon(app);
    app.cutoutIconSource = appIcon(app, 1);
    app.inkIconSource = appIcon(app, 2);
    app.iconContrastBackdrop = iconRole(app)["contrastBackdrop"].toString();
    app.iconOpticalScale = qBound(.5, iconRole(app)["opticalScale"].toDouble(1), 1.4);
    ids.insert(app.id);
    for (const auto& alias : app.legacyIds())
    {
        if (ids.contains(alias))
        {
            setError(tr("App compatibility IDs must be unique."));
            return false;
        }
        ids.insert(alias);
    }
    loaded.append(app);
    return true;
}

bool
AppRegistry::validateAppEntryFields(AppDefinition& app, const QJsonObject& object)
{
    static const QSet<QString> presentationModes{QStringLiteral("native-tv"),
                                                 QStringLiteral("web-kiosk"),
                                                 QStringLiteral("media"),
                                                 QStringLiteral("capture"),
                                                 QStringLiteral("game-hub"),
                                                 QStringLiteral("game"),
                                                 QStringLiteral("disc-media"),
                                                 QStringLiteral("disc-game"),
                                                 QStringLiteral("utility-desktop"),
                                                 QStringLiteral("desktop-bridge")};
    static const QSet<QString> networkRequirements{
        QStringLiteral("none"), QStringLiteral("local-network"), QStringLiteral("internet")};
    static const QSet<QString> handledActionTypes{QStringLiteral("open-folder"),
                                                  QStringLiteral("open-file"),
                                                  QStringLiteral("open-url"),
                                                  QStringLiteral("open-disc")};
    const auto url = QUrl(object.value(QStringLiteral("url")).toString());
    bool validOrigins = true;
    for (const auto& origin : object.value(QStringLiteral("allowedOrigins")).toArray())
        validOrigins = validOrigins && isSafeWebOrigin(origin.toString());
    const bool validWeb = app.type != QStringLiteral("web") || (isSafeWebUrl(url) && validOrigins);
    if (!app.isValid() || !app.accent.isValid() || !presentationModes.contains(app.presentationMode) ||
        !validWeb)
    {
        setError(
            tr("App entries require unique IDs, names, types, valid accent colors, known presentation/network "
               "modes, and safe web URLs/origins."));
        return false;
    }
    if (!networkRequirements.contains(app.networkRequirement))
    {
        setError(tr("App entries require a known network requirement."));
        return false;
    }
    for (const auto& handledAction : app.handledActions)
    {
        if (!handledActionTypes.contains(handledAction))
        {
            setError(tr("App entries require known handled action types."));
            return false;
        }
    }
    return true;
}

bool
AppRegistry::validateAppEntryOrigin(AppDefinition& app, const QJsonObject& object)
{
    const auto url = QUrl(object.value(QStringLiteral("url")).toString());
    bool validOrigins = true;
    for (const auto& origin : object.value(QStringLiteral("allowedOrigins")).toArray())
        validOrigins = validOrigins && isSafeWebOrigin(origin.toString());
    const bool validWeb = app.type != QStringLiteral("web") || (isSafeWebUrl(url) && validOrigins);
    if (!validWeb)
    {
        setError(
            tr("App entries require unique IDs, names, types, valid accent colors, known presentation/network "
               "modes, and safe web URLs/origins."));
        return false;
    }
    if (object.contains("allowedOrigins"))
    {
        for (const auto& origin : object.value(QStringLiteral("allowedOrigins")).toArray())
        {
            if (!isSafeWebOrigin(origin.toString()))
            {
                setError(tr("App entries require valid web origins."));
                return false;
            }
        }
    }
    return true;
}

void
AppRegistry::setError(const QString& message)
{
    if (m_errorString == message)
        return;
    m_errorString = message;
    emit errorStringChanged();
}
