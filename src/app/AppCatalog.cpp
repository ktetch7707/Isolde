#include "app/AppCatalog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QVersionNumber>
#include <QRegularExpression>
#include <algorithm>

AppCatalog::AppCatalog(AppRegistry *registry, QObject *parent) : QObject(parent), m_registry(registry) {}

static bool isSafeWebUrl(const QUrl &url)
{
    return url.isValid() && (url.scheme() == QStringLiteral("https") || url.scheme() == QStringLiteral("qrc"));
}

static bool isSafeWebOrigin(const QString &text)
{
    const QUrl url(text.trimmed());
    return isSafeWebUrl(url) && (url.scheme() == QStringLiteral("qrc")
        || (!url.host().isEmpty() && url.path().isEmpty() && url.query().isEmpty() && url.fragment().isEmpty()));
}

static QStringList readStringArray(const QJsonArray &array)
{
    QStringList result;
    for (const auto &value : array) {
        const auto text = value.toString().trimmed();
        if (!text.isEmpty() && !result.contains(text))
            result.append(text);
    }
    return result;
}

static bool isValidActionHandlers(const QJsonObject &appObject, const QStringList &handledActions)
{
    if (handledActions.isEmpty())
        return true;
    if (appObject.value(QStringLiteral("type")).toString() != QStringLiteral("desktop"))
        return true;
    const auto handlersValue = appObject.value(QStringLiteral("actionHandlers"));
    if (!handlersValue.isObject())
        return false;
    const auto handlers = handlersValue.toObject();
    for (const auto &actionType : handledActions) {
        const auto handlerValue = handlers.value(actionType);
        if (!handlerValue.isObject())
            return false;
        const auto handler = handlerValue.toObject();
        if (handler.value(QStringLiteral("appendTarget")).toBool(false))
            continue;
        const auto argumentsValue = handler.value(QStringLiteral("arguments"));
        if (!argumentsValue.isArray() || argumentsValue.toArray().isEmpty())
            return false;
        for (const auto &argument : argumentsValue.toArray()) {
            if (!argument.isString() || argument.toString().trimmed().isEmpty())
                return false;
        }
    }
    return true;
}

QVariantList AppCatalog::entries() const
{
    QVariantList result;
    for (const auto &entry : m_entries)
        result.append(QVariantMap{{"id", entry.app.id}, {"name", entry.app.name}, {"category", entry.category},
                                  {"version", entry.version}, {"description", entry.description},
                                  {"publisher", entry.publisher}, {"provenance", entry.provenance}, {"capabilities", entry.capabilities},
                                  {"setupRequired", entry.setupRequired}, {"setupPrompt", entry.setupPrompt},
                                  {"state", packageState(entry.app.id)},
                                  {"failureReason", m_failureReasons.value(entry.app.id)},
                                  {"presentationMode", entry.app.presentationMode},
                                  {"profileIsolation", entry.app.type == "android" ? "private-android" : entry.app.type == "desktop" ? "private-native" : (entry.app.type == "web" ? "private-web" : "")},
                                  {"networkRequirement", entry.app.networkRequirement},
                                  {"handledActions", entry.app.handledActions},
                                  {"accent", entry.app.accent.name()}, {"symbol", entry.app.symbol},
                                  {"installed", isInstalled(entry.app.id)}});
    return result;
}

QString AppCatalog::errorString() const { return m_errorString; }
bool AppCatalog::isInstalled(const QString &id) const { return m_installedIds.contains(id); }
QString AppCatalog::packageState(const QString &id) const
{
    if (!isInstalled(id))
        return QStringLiteral("available");
    const auto state = m_packageStates.value(id, QStringLiteral("ready"));
    if (state == QStringLiteral("needs-setup") || state == QStringLiteral("ready") || state == QStringLiteral("failed"))
        return state;
    return QStringLiteral("ready");
}

const AppCatalog::Entry *AppCatalog::findEntry(const QString &id) const
{
    for (const auto &entry : m_entries)
        if (entry.app.id == id) return &entry;
    return nullptr;
}

bool AppCatalog::load(const QString &catalogPath, const QString &statePath, const QString &installRoot)
{
    QString source = catalogPath;
    if (source.isEmpty()) {
        source = QDir(QCoreApplication::applicationDirPath()).filePath("config/catalog.json");
        if (!QFile::exists(source)) source = QStringLiteral(":/config/catalog.json");
    }
    m_statePath = statePath.isEmpty()
        ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)).filePath("installed-apps.json")
        : statePath;
    m_installRoot = installRoot.isEmpty()
        ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("packages")
        : installRoot;

    QFile file(source);
    if (!file.open(QIODevice::ReadOnly)) { setError(tr("Unable to open app catalog: %1").arg(source)); return false; }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || document.object().value("schemaVersion").toInt() != 1
        || !document.object().value("packages").isArray()) {
        setError(tr("Invalid app catalog: %1").arg(parseError.errorString())); return false;
    }

    QVector<Entry> loaded;
    QSet<QString> ids;
    const QDir catalogDirectory(QFileInfo(source).absolutePath());
    for (const auto &value : document.object().value("packages").toArray()) {
        const auto manifestPath = catalogDirectory.filePath(value.toString());
        QFile manifestFile(manifestPath);
        if (!manifestFile.open(QIODevice::ReadOnly)) {
            setError(tr("Unable to open package manifest: %1").arg(manifestPath)); return false;
        }
        const auto manifestData = manifestFile.readAll();
        QJsonParseError manifestError;
        const auto manifestDocument = QJsonDocument::fromJson(manifestData, &manifestError);
        if (manifestError.error != QJsonParseError::NoError || !manifestDocument.isObject()) {
            setError(tr("Invalid package manifest: %1").arg(manifestPath)); return false;
        }
        const auto object = manifestDocument.object();
        const auto appObject = object.value("app").toObject();
        Entry entry;
        entry.packageId = object.value("packageId").toString().trimmed();
        entry.app.id = appObject.value("id").toString().trimmed();
        entry.app.name = object.value("name").toString().trimmed();
        entry.app.type = appObject.value("type").toString().trimmed();
        entry.app.presentationMode = appObject.value("presentationMode").toString(QStringLiteral("native-tv")).trimmed();
        entry.app.networkRequirement = appObject.value("networkRequirement").toString(
            entry.app.type == QStringLiteral("web") ? QStringLiteral("internet") : QStringLiteral("none")).trimmed();
        entry.app.handledActions = readStringArray(appObject.value(QStringLiteral("handledActions")).toArray());
        entry.app.section = appObject.value("section").toString().trimmed();
        entry.app.symbol = appObject.value("symbol").toString().trimmed();
        entry.app.accent = QColor(appObject.value("accent").toString());
        entry.app.launchConfiguration = appObject.toVariantMap();
        entry.category = object.value("category").toString().trimmed();
        entry.version = object.value("version").toString().trimmed();
        entry.minIsoldeVersion = object.value("minIsoldeVersion").toString().trimmed();
        entry.description = object.value("description").toString().trimmed();
        entry.publisher = object.value("publisher").toString().trimmed();
        entry.provenance = object.value("provenance").toString().trimmed();
        const auto setupObject = object.value(QStringLiteral("setup")).toObject();
        entry.setupRequired = setupObject.value(QStringLiteral("required")).toBool(false);
        entry.setupPrompt = setupObject.value(QStringLiteral("prompt")).toString().trimmed();
        for (const auto &capability : object.value("capabilities").toArray())
            entry.capabilities.append(capability.toString());
        entry.manifestData = manifestData;
        static const QRegularExpression safeId(QStringLiteral("^[a-z0-9][a-z0-9.-]*$"));
        static const QRegularExpression safeDesktopId(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));
        static const QRegularExpression safeBusName(QStringLiteral("^[A-Za-z_][A-Za-z0-9_.-]*$"));
        static const QSet<QString> presentationModes{QStringLiteral("native-tv"), QStringLiteral("web-kiosk"),
                                                     QStringLiteral("media"), QStringLiteral("capture"),
                                                     QStringLiteral("game-hub"), QStringLiteral("game"),
                                                     QStringLiteral("disc-media"), QStringLiteral("disc-game"),
                                                     QStringLiteral("utility-desktop"), QStringLiteral("desktop-bridge")};
        static const QSet<QString> networkRequirements{QStringLiteral("none"), QStringLiteral("local-network"),
                                                       QStringLiteral("internet")};
        static const QSet<QString> handledActionTypes{QStringLiteral("open-folder"), QStringLiteral("open-file"),
                                                      QStringLiteral("open-url"), QStringLiteral("open-disc")};
        const auto desktopId = appObject.value("desktopId").toString();
        const auto busName = appObject.value("mprisBusName").toString();
        const auto windowClass = appObject.value("windowClass").toString();
        const auto containment = appObject.value("containment").toString();
        const auto url = QUrl(appObject.value(QStringLiteral("url")).toString());
        bool validOrigins = true;
        for (const auto &origin : appObject.value(QStringLiteral("allowedOrigins")).toArray())
            validOrigins = validOrigins && isSafeWebOrigin(origin.toString());
        const bool validDesktop = entry.app.type != QStringLiteral("desktop")
            || (safeDesktopId.match(desktopId).hasMatch() && (busName.isEmpty() || safeBusName.match(busName).hasMatch())
                && (windowClass.isEmpty() || safeBusName.match(windowClass).hasMatch())
                && (containment.isEmpty() || containment == QStringLiteral("fullscreen")));
        const bool validWeb = entry.app.type != QStringLiteral("web") || (isSafeWebUrl(url) && validOrigins);
        bool validHandledActions = true;
        for (const auto &handledAction : entry.app.handledActions)
            validHandledActions = validHandledActions && handledActionTypes.contains(handledAction);
        const bool validActionHandlers = isValidActionHandlers(appObject, entry.app.handledActions);
        const bool compatible = QVersionNumber::fromString(QCoreApplication::applicationVersion())
            >= QVersionNumber::fromString(entry.minIsoldeVersion);
        if (object.value("schemaVersion").toInt() != 1 || !safeId.match(entry.packageId).hasMatch()
            || entry.packageId != entry.app.id || !entry.app.isValid() || ids.contains(entry.app.id)
            || !entry.app.accent.isValid() || entry.category.isEmpty() || entry.publisher.isEmpty()
            || !presentationModes.contains(entry.app.presentationMode)
            || !networkRequirements.contains(entry.app.networkRequirement)
            || !validHandledActions || !validActionHandlers
            || QVersionNumber::fromString(entry.version).isNull() || !compatible || entry.description.isEmpty()
            || (entry.setupRequired && entry.setupPrompt.isEmpty())
            || !validDesktop || !validWeb) {
            setError(tr("Catalog entries require unique IDs and complete, valid metadata.")); return false;
        }
        ids.insert(entry.app.id);
        loaded.append(entry);
    }
    m_entries = std::move(loaded);
    m_installedIds.clear();
    m_packageStates.clear();
    m_failureReasons.clear();
    QDir installDirectory(m_installRoot);
    for (const auto &name : installDirectory.entryList({QStringLiteral(".staging-*")}, QDir::Dirs | QDir::NoDotAndDotDot))
        QDir(installDirectory.filePath(name)).removeRecursively();

    QFile state(m_statePath);
    if (state.exists() && state.open(QIODevice::ReadOnly)) {
        const auto stateObject = QJsonDocument::fromJson(state.readAll()).object();
        const auto installed = stateObject.value("installed").toArray();
        const auto packageStates = stateObject.value(QStringLiteral("packageStates")).toObject();
        const auto failureReasons = stateObject.value(QStringLiteral("failureReasons")).toObject();
        for (const auto &value : installed) {
            if (const auto *entry = findEntry(value.toString())) {
                const auto installedManifest = QDir(m_installRoot).filePath(
                    entry->packageId + "/" + entry->version + "/manifest.json");
                if (QFile::exists(installedManifest)) {
                    m_installedIds.insert(entry->app.id);
                    m_registry->addApp(entry->app);
                    auto restoredState = packageStates.value(entry->app.id).toString(
                        entry->setupRequired ? QStringLiteral("needs-setup") : QStringLiteral("ready"));
                    if (restoredState != QStringLiteral("needs-setup") && restoredState != QStringLiteral("ready")
                        && restoredState != QStringLiteral("failed"))
                        restoredState = QStringLiteral("ready");
                    m_packageStates.insert(entry->app.id, restoredState);
                    const auto reason = failureReasons.value(entry->app.id).toString().trimmed();
                    if (!reason.isEmpty())
                        m_failureReasons.insert(entry->app.id, reason);
                }
            }
        }
    }
    setError({});
    emit entriesChanged();
    return true;
}

bool AppCatalog::install(const QString &id)
{
    const auto *entry = findEntry(id);
    if (!entry || isInstalled(id)) return false;
    const auto packageDirectory = QDir(m_installRoot).filePath(entry->packageId + "/" + entry->version);
    if (!QFile::exists(QDir(packageDirectory).filePath("manifest.json"))) {
        QDir().mkpath(m_installRoot);
        const auto staging = QDir(m_installRoot).filePath(".staging-" + entry->packageId + "-" + entry->version);
        QDir(staging).removeRecursively();
        if (!QDir().mkpath(staging)) { setError(tr("Unable to stage package %1.").arg(id)); return false; }
        QSaveFile manifest(QDir(staging).filePath("manifest.json"));
        if (!manifest.open(QIODevice::WriteOnly) || manifest.write(entry->manifestData) != entry->manifestData.size()
            || !manifest.commit()) {
            QDir(staging).removeRecursively(); setError(tr("Unable to write staged package %1.").arg(id)); return false;
        }
        QDir().mkpath(QFileInfo(packageDirectory).absolutePath());
        if (!QDir().rename(staging, packageDirectory)) {
            QDir(staging).removeRecursively(); setError(tr("Unable to activate package %1.").arg(id)); return false;
        }
    }
    if (!m_registry->addApp(entry->app)) return false;
    m_installedIds.insert(id);
    m_packageStates.insert(id, entry->setupRequired ? QStringLiteral("needs-setup") : QStringLiteral("ready"));
    m_failureReasons.remove(id);
    if (!saveState()) {
        m_installedIds.remove(id);
        m_packageStates.remove(id);
        m_failureReasons.remove(id);
        m_registry->removeApp(id);
        return false;
    }
    emit entriesChanged();
    emit packageInstalled(id);
    return true;
}

bool AppCatalog::uninstall(const QString &id)
{
    const auto *entry = findEntry(id);
    if (!entry || !isInstalled(id)) return false;
    const auto previousState = m_packageStates.value(id);
    const auto previousFailureReason = m_failureReasons.value(id);
    m_installedIds.remove(id);
    m_packageStates.remove(id);
    m_failureReasons.remove(id);
    m_registry->removeApp(id);
    if (!saveState()) {
        m_installedIds.insert(id);
        if (!previousState.isEmpty())
            m_packageStates.insert(id, previousState);
        if (!previousFailureReason.isEmpty())
            m_failureReasons.insert(id, previousFailureReason);
        m_registry->addApp(entry->app);
        return false;
    }
    QDir(QDir(m_installRoot).filePath(entry->packageId)).removeRecursively();
    emit entriesChanged();
    emit packageUninstalled(id);
    return true;
}

bool AppCatalog::completeSetup(const QString &id)
{
    if (!isInstalled(id)) {
        setError(tr("Package is not installed: %1").arg(id));
        emit operationFailed(m_errorString);
        return false;
    }
    m_packageStates.insert(id, QStringLiteral("ready"));
    m_failureReasons.remove(id);
    if (!saveState())
        return false;
    emit entriesChanged();
    return true;
}

bool AppCatalog::markFailed(const QString &id, const QString &reason)
{
    if (!isInstalled(id))
        return false;
    m_packageStates.insert(id, QStringLiteral("failed"));
    const auto cleanReason = reason.trimmed();
    m_failureReasons.insert(id, cleanReason.isEmpty() ? tr("The app failed to start.") : cleanReason);
    if (!saveState())
        return false;
    emit entriesChanged();
    return true;
}

bool AppCatalog::saveState()
{
    QDir().mkpath(QFileInfo(m_statePath).absolutePath());
    auto ids = m_installedIds.values();
    std::sort(ids.begin(), ids.end());
    QJsonArray installed;
    for (const auto &id : ids) installed.append(id);
    QJsonObject packageStates;
    QJsonObject failureReasons;
    for (const auto &id : ids) {
        packageStates.insert(id, packageState(id));
        const auto reason = m_failureReasons.value(id).trimmed();
        if (!reason.isEmpty())
            failureReasons.insert(id, reason);
    }
    QSaveFile file(m_statePath);
    const auto data = QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"installed", installed},
                                                {"packageStates", packageStates},
                                                {"failureReasons", failureReasons}}).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        setError(tr("Unable to save app installation state."));
        emit operationFailed(m_errorString);
        return false;
    }
    setError({});
    return true;
}

void AppCatalog::setError(const QString &message)
{
    if (m_errorString == message) return;
    m_errorString = message;
    emit errorStringChanged();
}
