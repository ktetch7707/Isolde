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
#include <QCryptographicHash>
#include <QNetworkRequest>
#include <algorithm>

AppCatalog::AppCatalog(AppRegistry *registry, QObject *parent) : QObject(parent), m_registry(registry) {
    connect(&m_network, &QNetworkAccessManager::finished, this, [this](QNetworkReply *reply){if(reply==m_downloadReply)onDownloadFinished();});
}
AppCatalog::~AppCatalog() {
    if(m_downloadReply) { auto *reply=m_downloadReply;m_downloadReply=nullptr;reply->disconnect(this);reply->abort();reply->deleteLater(); }
    if(m_androidProcess.state()!=QProcess::NotRunning){m_androidProcess.terminate();if(!m_androidProcess.waitForFinished(20000)){m_androidProcess.kill();m_androidProcess.waitForFinished(3000);}}
}

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
                                  {"presentationMode", entry.app.presentationMode}, {"type", entry.app.type},
                                  {"storeType", entry.app.type == "android" ? "android" : (entry.app.type == "web" || entry.app.presentationMode == "web-kiosk") ? "web" : "native"},
                                  {"profileIsolation", entry.app.type == "android" ? "private-android" : entry.app.type == "desktop" ? "private-native" : (entry.app.type == "web" ? "private-web" : "")},
                                  {"networkRequirement", entry.app.networkRequirement},
                                  {"handledActions", entry.app.handledActions},
                                  {"accent", entry.app.accent.name()}, {"symbol", entry.app.symbol},
                                  {"installed", isInstalled(entry.app.id)}});
    return result;
}

void AppCatalog::setStoreProvider(StoreProvider *provider)
{
    if (m_provider == provider) return;
    if(m_downloadReply)m_downloadReply->abort();
    if(provider && provider->providerId()!="local" && provider->providerId()!="fdroid"){setError(tr("Unknown Store source."));return;}
    if(m_provider){m_provider->cancel();m_provider->disconnect(this);}
    m_providerResults.clear();
    m_provider = provider;
    if (m_provider) connect(m_provider, &StoreProvider::resultsChanged, this, [this]() {
        if (m_provider && (!m_provider->isOnline() || m_onlineStoreSearch)) {
            const auto results=m_provider->results();
            if(results.size()>200){setError(tr("The Store returned more than 200 results. Narrow your search."));return;}
            m_providerResults = results;
            emit providerResultsChanged();
            emit entriesChanged();
        }
    });
    if(m_provider)connect(m_provider,&StoreProvider::providerError,this,[this](const QString &error){setError(error.left(512));});
    emit providerResultsChanged();
    emit entriesChanged();
}

void AppCatalog::setOnlineStoreSearch(bool enabled)
{
    if (m_onlineStoreSearch == enabled) return;
    m_onlineStoreSearch = enabled;
    if (enabled && m_provider) {
        m_provider->requestSearch(QString());
    } else {
        if(m_provider)m_provider->cancel();
        m_providerResults.clear();
        if(m_downloadReply)m_downloadReply->abort();
    }
    emit providerResultsChanged();
    emit onlineStoreSearchChanged();
    emit entriesChanged();
}

void AppCatalog::searchStore(const QString &query)
{
    if(query.size()>128){setError(tr("Use a search of at most 128 characters."));return;}
    if(m_provider && (!m_provider->isOnline() || m_onlineStoreSearch))m_provider->requestSearch(query.trimmed());
}

QVariantList AppCatalog::allEntries() const
{
    if (!m_onlineStoreSearch) return entries();
    return mergedEntries();
}

QVariantList AppCatalog::mergedEntries() const
{
    QVariantList result = entries();
    for (const auto &value : m_providerResults) {
        const auto map = value.toMap();
        const QString id = QStringLiteral("provider:") + map.value(QStringLiteral("id")).toString();
        const auto installedId="fdroid-"+QString::fromLatin1(QCryptographicHash::hash(map.value("packageName").toString().toUtf8(),QCryptographicHash::Sha256).toHex().left(24));
        if (isInstalled(installedId)) continue;
        QVariantMap merged = map;
        merged[QStringLiteral("id")] = id;
        merged[QStringLiteral("storeType")] = map.value(QStringLiteral("storeType"),map.value(QStringLiteral("type"))).toString();
        merged[QStringLiteral("installed")] = false;
        merged[QStringLiteral("state")] = QStringLiteral("available");
        result.append(merged);
    }
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
    if(busy()){setError(tr("Finish the current app operation before reloading the catalog."));return false;}
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
    m_legacyInstallRoot.clear();
    QString stateReadPath=m_statePath;
    const auto configuredProfile=qApp->property("isoldeProfile").toString();
    const auto profile=configuredProfile.isEmpty()?QString("default"):configuredProfile;
    if(statePath.isEmpty() && installRoot.isEmpty()) {
        static const QRegularExpression uuid("^[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}$");
        if(profile!="default" && !uuid.match(profile).hasMatch()){setError(tr("Invalid app profile."));return false;}
        m_legacyInstallRoot=m_installRoot;
        m_statePath=QFileInfo(m_statePath).absolutePath()+"/profiles/"+profile+"/installed-apps.json";
        m_installRoot+="/profiles/"+profile;
        if(QFile::exists(m_statePath))stateReadPath=m_statePath;
    }

    QFile file(source);
    if (!file.open(QIODevice::ReadOnly)) { setError(tr("Unable to open app catalog: %1").arg(source)); return false; }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || document.object().value("schemaVersion").toInt() != 1
        || !document.object().value("packages").isArray()) {
        setError(tr("Invalid app catalog: %1").arg(parseError.errorString())); return false;
    }

    const auto options=document.object().value("store").toObject();
    if(!options.isEmpty()) {
        const QSet<QString> allowed{"platforms","columns","spacing","cardHeight","detailRatio","webBackend","webDesktopId","webCategory","appSection","appAccent"};
        for(const auto &key:options.keys())if(!allowed.contains(key)){setError(tr("Unknown Store presentation field."));return false;}
        for(const auto &key:QStringList{"columns","spacing","cardHeight"})
            if(!options.value(key).isDouble() || options.value(key).toDouble()!=options.value(key).toInt()){setError(tr("Store sizes must use whole numbers."));return false;}
        if(options.contains("detailRatio") && (!options.value("detailRatio").isDouble() || options.value("detailRatio").toDouble()<0.25 || options.value("detailRatio").toDouble()>0.5)){setError(tr("Invalid Store details size."));return false;}
        const auto platforms=options.value("platforms").toArray();
        if(platforms.size()!=2 || options.value("columns").toInt()<1 || options.value("columns").toInt()>4
            || options.value("spacing").toInt()<8 || options.value("spacing").toInt()>48
            || options.value("cardHeight").toInt()<100 || options.value("cardHeight").toInt()>220) {setError(tr("Invalid Store presentation."));return false;}
        QSet<QString> types;
        if(options.contains("webBackend") && (options.value("webBackend")!="chromium" && options.value("webBackend")!="embedded")){setError(tr("Invalid web app backend."));return false;}
        if(options.contains("webDesktopId") && !QRegularExpression("^[A-Za-z0-9][A-Za-z0-9._-]{0,79}$").match(options.value("webDesktopId").toString()).hasMatch()){setError(tr("Invalid web browser identity."));return false;}
        if(options.contains("appAccent") && !QColor(options.value("appAccent").toString()).isValid()){setError(tr("Invalid Store app color."));return false;}
        for(const auto &key:QStringList{"webCategory","appSection"})if(options.contains(key) && (options.value(key).toString().isEmpty() || options.value(key).toString().size()>80)){setError(tr("Invalid Store app label."));return false;}
        for(const auto &item:platforms){const auto p=item.toObject();const auto type=p.value("type").toString();
            for(const auto &key:p.keys())if(key!="type" && key!="label"){setError(tr("Unknown Store platform field."));return false;}
            if((type!="web" && type!="android") || types.contains(type) || p.value("label").toString().isEmpty() || p.value("label").toString().size()>40){setError(tr("Invalid Store platforms."));return false;}types.insert(type);}
    }
    m_storeOptions=options.toVariantMap();
    QVector<Entry> loaded;
    QSet<QString> ids;
    const QDir catalogDirectory(QFileInfo(source).absolutePath());
    for (const auto &value : document.object().value("packages").toArray()) {
        static const QRegularExpression manifestName("^packages/[a-z0-9][a-z0-9.-]{0,79}/manifest\\.json$");
        if(!value.isString() || !manifestName.match(value.toString()).hasMatch()){setError(tr("Unsafe package manifest path."));return false;}
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
            || entry.version.size()>80 || QVersionNumber::fromString(entry.version).isNull() || !QRegularExpression("^[0-9]+(?:\\.[0-9]+){0,3}(?:-[A-Za-z0-9.-]+)?$").match(entry.version).hasMatch() || !compatible || entry.description.isEmpty()
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
    m_customPackages.clear();
    QDir installDirectory(m_installRoot);
    for (const auto &name : installDirectory.entryList({QStringLiteral(".staging-*")}, QDir::Dirs | QDir::NoDotAndDotDot))
        QDir(installDirectory.filePath(name)).removeRecursively();

    QFile state(stateReadPath);
    if (state.exists() && state.open(QIODevice::ReadOnly)) {
        const auto stateObject = QJsonDocument::fromJson(state.readAll()).object();
        if(stateReadPath==m_statePath){
            const auto custom=stateObject.value("customPackages").toObject();
            if(custom.size()>256){setError(tr("Too many custom apps in this profile."));return false;}
            for(auto it=custom.begin();it!=custom.end();++it){
                if(!QRegularExpression("^(web|fdroid)-[a-f0-9]{24}$").match(it.key()).hasMatch() || !QRegularExpression("^[0-9]{1,18}(?:\\.[0-9]{1,18}){0,3}$").match(it.value().toString()).hasMatch()){setError(tr("Invalid custom app identity."));return false;}
                const auto path=QDir(m_installRoot).filePath(it.key()+"/"+it.value().toString()+"/manifest.json");QFile customFile(path);
                const auto info=QFileInfo(path),base=QFileInfo(m_installRoot);
                if(info.isSymLink() || base.isSymLink() || !info.canonicalFilePath().startsWith(base.canonicalFilePath()+"/") || info.size()>65536 || !customFile.open(QIODevice::ReadOnly)){setError(tr("Unable to load this profile's custom app."));return false;}
                Entry entry;if(!customEntry(QJsonDocument::fromJson(customFile.readAll()).object(),&entry) || entry.app.id!=it.key() || entry.version!=it.value().toString() || findEntry(entry.app.id))return false;
                m_entries.append(entry);m_customPackages.insert(entry.app.id,entry.version);
            }
        }
        const auto installed = stateObject.value("installed").toArray();
        const auto packageStates = stateObject.value(QStringLiteral("packageStates")).toObject();
        const auto failureReasons = stateObject.value(QStringLiteral("failureReasons")).toObject();
        for (const auto &value : installed) {
            if (const auto *entry = findEntry(value.toString())) {
                const auto installedManifest = QDir(m_installRoot).filePath(
                    entry->packageId + "/" + entry->version + "/manifest.json");
                const auto legacyManifest=QDir(m_legacyInstallRoot).filePath(entry->packageId+"/"+entry->version+"/manifest.json");
                if (QFile::exists(installedManifest) || (!m_legacyInstallRoot.isEmpty() && QFile::exists(legacyManifest))) {
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
    if (id.startsWith("provider:")) {
        for (const auto &value : m_providerResults) {
            const auto result=value.toMap();
            if ("provider:"+result.value("id").toString()==id) {
                const auto localId="fdroid-"+QString::fromLatin1(QCryptographicHash::hash(result.value("packageName").toString().toUtf8(),QCryptographicHash::Sha256).toHex().left(24));
                if(findEntry(localId))return install(localId);
                return downloadAndInstall(result.value("storeId").toString());
            }
        }
        setError(tr("Search again; this Store result is no longer available.")); return false;
    }
    const auto *entry = findEntry(id);
    if (!entry || isInstalled(id)) return false;
    if(busy() && !m_committing)return false;
    if(entry->app.type=="android" && !m_committing)return androidOperation(*entry,"install");
    if(entry->custom)m_customPackages.insert(id,entry->version);
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
        if(entry->custom)m_customPackages.remove(id);
        return false;
    }
    emit entriesChanged();
    emit packageInstalled(id);
    return true;
}

bool AppCatalog::customEntry(const QJsonObject &data,Entry *entry)
{
    if (data.value("kind")=="fdroid") {
        const QSet<QString> fields{"schemaVersion","kind","id","name","version","packageName","sha256","provenance"};
        for(const auto &key:data.keys()) if(!fields.contains(key)){setError(tr("Unknown Android package field."));return false;}
        const auto package=data.value("packageName").toString(),name=data.value("name").toString().trimmed();
        const auto id="fdroid-"+QString::fromLatin1(QCryptographicHash::hash(package.toUtf8(),QCryptographicHash::Sha256).toHex().left(24));
        if(data.value("schemaVersion").toInt()!=1 || data.value("id")!=id
           || !QRegularExpression("^[A-Za-z][A-Za-z0-9_]*(?:\\.[A-Za-z0-9_]+)+$").match(package).hasMatch()
           || !QRegularExpression("^[0-9]{1,18}$").match(data.value("version").toString()).hasMatch()
           || !QRegularExpression("^[a-f0-9]{64}$").match(data.value("sha256").toString()).hasMatch()
           || name.isEmpty() || name.size()>80 || name.contains(QRegularExpression("[\\x00-\\x1f<>]"))
           || data.value("provenance").toString().isEmpty() || data.value("provenance").toString().size()>512){setError(tr("Invalid verified Android package metadata."));return false;}
        entry->app.id=id;entry->app.name=name;entry->app.type="android";entry->app.presentationMode="desktop-bridge";
        entry->app.networkRequirement="none";entry->app.section=m_storeOptions.value("appSection").toString();
        entry->app.accent=QColor(m_storeOptions.value("appAccent").toString());entry->app.symbol=name.left(1);
        entry->app.launchConfiguration={{"runtimeApp",id},{"storeVersion",data.value("version").toString()}};
        entry->packageId=id;entry->version=data.value("version").toString();entry->custom=true;
        entry->publisher=package;entry->provenance=data.value("provenance").toString();entry->manifestData=QJsonDocument(data).toJson();
        if(!entry->app.isValid()){setError(tr("Invalid Android app presentation."));return false;}
        return true;
    }
    const QSet<QString> fields{"schemaVersion","kind","id","name","url","backend","version"};
    for(const auto &key:data.keys())if(!fields.contains(key)){setError(tr("Unknown custom app field."));return false;}
    const auto name=data.value("name").toString().trimmed(),address=data.value("url").toString(),backend=data.value("backend").toString();
    const QUrl url(address,QUrl::StrictMode);
    if(data.value("schemaVersion").toInt()!=1 || data.value("kind")!="web" || data.value("version")!="1.0.0"
       || name.isEmpty() || name.size()>80 || name.contains(QRegularExpression("[\\x00-\\x1f<>]"))
       || address.size()>2048 || !url.isValid() || url.scheme()!="https" || url.host().isEmpty() || !url.userInfo().isEmpty()
       || (backend!="chromium" && backend!="embedded")){setError(tr("Use a name of up to 80 characters and a valid HTTPS address without embedded sign-in details."));return false;}
    const auto canonical=url.toString(QUrl::FullyEncoded);
    const auto id="web-"+QString::fromLatin1(QCryptographicHash::hash(canonical.toUtf8(),QCryptographicHash::Sha256).toHex().left(24));
    if(data.value("id")!=id){setError(tr("The web app identity does not match its address."));return false;}
    QUrl origin;origin.setScheme(url.scheme());origin.setHost(url.host());origin.setPort(url.port());
    entry->app.id=id;entry->app.name=name;entry->app.type=backend=="chromium"?"desktop":"web";entry->app.presentationMode="web-kiosk";
    entry->app.networkRequirement="internet";entry->app.section=m_storeOptions.value("appSection").toString();
    entry->app.accent=QColor(m_storeOptions.value("appAccent").toString());entry->app.symbol=name.left(1);
    entry->app.launchConfiguration={{"url",canonical},{"allowedOrigins",QStringList{origin.toString()}}};
    if(backend=="chromium"){
        const auto browser=m_storeOptions.value("webDesktopId").toString();
        if(!QRegularExpression("^[A-Za-z0-9][A-Za-z0-9._-]{0,79}$").match(browser).hasMatch()){setError(tr("Choose a browser in the Store configuration."));return false;}
        entry->app.handledActions={"open-url"};
        entry->app.launchConfiguration.insert("desktopId",browser);
        entry->app.launchConfiguration.insert("actionType","open-url");entry->app.launchConfiguration.insert("actionTarget",canonical);
        entry->app.launchConfiguration.insert("actionHandlers",QVariantMap{{"open-url",QVariantMap{{"arguments",QVariantList{"--app={target}"}}}}});
    }
    if(!entry->app.accent.isValid() || entry->app.section.isEmpty()){setError(tr("Configure the Store app appearance before adding a website."));return false;}
    entry->packageId=id;entry->version="1.0.0";entry->description=canonical;entry->publisher=url.host();entry->provenance=tr("Website chosen by you");
    entry->category=m_storeOptions.value("webCategory").toString();entry->custom=true;entry->manifestData=QJsonDocument(data).toJson();
    return true;
}

bool AppCatalog::installCustom(const QJsonObject &data)
{
    if(busy()){setError(tr("Finish the current app operation first."));return false;}
    Entry entry;if(!customEntry(data,&entry))return false;
    if(const auto *existing=findEntry(entry.app.id))return install(existing->app.id);
    if(m_customPackages.size()>=256){setError(tr("This profile already has 256 custom apps."));return false;}
    m_entries.append(entry);
    if(install(entry.app.id))return true;
    m_customPackages.remove(entry.app.id);m_entries.removeLast();return false;
}

QString AppCatalog::addWebApp(const QString &name,const QString &address,const QString &backend)
{
    const QUrl url(address.trimmed(),QUrl::StrictMode);const auto canonical=url.toString(QUrl::FullyEncoded);
    const auto id="web-"+QString::fromLatin1(QCryptographicHash::hash(canonical.toUtf8(),QCryptographicHash::Sha256).toHex().left(24));
    if(isInstalled(id))return id;
    return installCustom({{"schemaVersion",1},{"kind","web"},{"id",id},{"name",name},{"url",canonical},{"backend",backend},{"version","1.0.0"}})?id:QString();
}

bool AppCatalog::downloadAndInstall(const QString &storeId)
{
    if (!m_onlineStoreSearch || !m_provider || !m_provider->isOnline() || busy()) {
        setError(tr("Online store search is not enabled."));
        emit operationFailed(m_errorString);
        return false;
    }
    const auto results = m_providerResults;
    const auto it = std::find_if(results.begin(), results.end(), [&](const auto &v) {
        const auto map = v.toMap();
        return map.value(QStringLiteral("storeId")) == storeId;
    });
    if (it == results.end()) {
        setError(tr("App not found in store results."));
        emit operationFailed(m_errorString);
        return false;
    }
    const auto result = (*it).toMap();
    const QUrl artifactUrl = m_provider->artifactUrl(storeId);
    if (!artifactUrl.isValid() || artifactUrl.scheme()!="https" || artifactUrl.host().isEmpty() || !artifactUrl.userInfo().isEmpty()) {
        setError(tr("Store provides no download URL."));
        emit operationFailed(m_errorString);
        return false;
    }
    m_downloadProfile=qApp->property("isoldeProfile").toString();
    m_downloadStoreId = storeId;
    m_downloadExpectedHash = m_provider->artifactHash(storeId);
    if(m_downloadExpectedHash.size()!=32){setError(tr("The Store source did not supply a verified SHA-256."));return false;}
    if(m_provider->providerId()!="fdroid" || result.value("type")!="android"
       || !result.value("metadataVerified").toBool() || !result.value("compatible").toBool()) {
        setError(tr("Verified repository metadata and a compatible Android package are required."));return false;
    }
    m_downloadResult = result;
    QNetworkRequest request(artifactUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", QByteArray("Isolde/")+QCoreApplication::applicationVersion().toUtf8());
    request.setTransferTimeout(120000);
    m_downloadReply = m_network.get(request);
    connect(m_downloadReply,&QNetworkReply::readyRead,this,&AppCatalog::onDownloadReadyRead);
    connect(m_downloadReply,&QNetworkReply::downloadProgress,this,[this](qint64 received,qint64 total){
        if(received>128*1024*1024 || total>128*1024*1024){m_downloadReply->abort();return;}
        m_downloadProgress=total>0?tr("%1%").arg(qRound(100.0*received/total)):tr("%1 bytes").arg(received);emit downloadProgressChanged();
    });
    m_downloadData.clear();
    emit entriesChanged();
    m_downloadProgress.clear();
    emit downloadProgressChanged();
    return true;
}

bool AppCatalog::stageProviderArtifact(const QString &storeId, const QVariantMap &result)
{
    Q_UNUSED(storeId);
    const auto package=result.value("packageName").toString();
    const auto id="fdroid-"+QString::fromLatin1(QCryptographicHash::hash(package.toUtf8(),QCryptographicHash::Sha256).toHex().left(24));
    QJsonObject manifest{{"schemaVersion",1},{"kind","fdroid"},{"id",id},{"name",result.value("name").toString()},
        {"packageName",package},{"version",result.value("versionCode").toString()},
        {"sha256",QString::fromLatin1(m_downloadExpectedHash.toHex())},{"provenance",result.value("provenance").toString()}};
    Entry entry;if(!customEntry(manifest,&entry))return false;
    if(findEntry(id) || m_customPackages.size()>=256){setError(tr("App already registered or profile app limit reached."));return false;}
    const auto directory=QDir(m_installRoot).filePath(id+"/"+entry.version);
    const auto staging=QDir(m_installRoot).filePath(".staging-"+id);
    QDir().mkpath(m_installRoot);
    if(QFileInfo(m_installRoot).canonicalFilePath()!=QDir::cleanPath(QFileInfo(m_installRoot).absoluteFilePath())
       || QFileInfo(QDir(m_installRoot).filePath(id)).isSymLink() || QFileInfo(directory).exists() || QFileInfo(staging).exists()
       || !QDir().mkpath(staging)){setError(tr("Unable to stage Android package in profile storage."));return false;}
    const auto write=[&](const QString &name,const QByteArray &bytes){QSaveFile file(QDir(staging).filePath(name));
        return file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size() && file.commit();};
    if(!write("app.apk",m_downloadData) || !write("manifest.json",entry.manifestData)
       || !QDir().mkpath(QFileInfo(directory).absolutePath()) || !QDir().rename(staging,directory)){
        QDir(staging).removeRecursively();setError(tr("Unable to save verified Android package."));return false;}
    m_entries.append(entry);
    if(!androidOperation(entry,"install")){m_entries.removeLast();QDir(directory).removeRecursively();return false;}
    return true;
}

void AppCatalog::onDownloadFinished()
{
    if(!m_downloadReply)return;
    if (m_downloadReply->error() != QNetworkReply::NoError) {
        const QString error = tr("Download failed: %1").arg(m_downloadReply->errorString());
        qWarning() << "Download error" << m_downloadReply->error() << error;
        setError(error);
        emit operationFailed(error);
        m_downloadProgress = QString();
        emit downloadProgressChanged();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
        m_downloadData.clear();emit entriesChanged();
        return;
    }
    if(m_downloadProfile!=qApp->property("isoldeProfile").toString()){
        auto *reply=m_downloadReply;m_downloadReply=nullptr;reply->deleteLater();m_downloadData.clear();
        setError(tr("The active profile changed. Retry installation in the selected profile."));emit entriesChanged();return;
    }
    onDownloadReadyRead();
    if(m_downloadReply->error()!=QNetworkReply::NoError){onDownloadFinished();return;}
    const QByteArray sha = m_downloadExpectedHash;
    if (!sha.isEmpty()) {
        QByteArray actual = QCryptographicHash::hash(m_downloadData, QCryptographicHash::Sha256);
        if (actual != sha) {
            setError(tr("Artifact hash mismatch. Aborting."));
            emit operationFailed(m_errorString);
            m_downloadProgress = QString();
            emit downloadProgressChanged();
            m_downloadReply->deleteLater();
            m_downloadReply = nullptr;
            m_downloadData.clear();emit entriesChanged();
            return;
        }
    }
    auto *reply=m_downloadReply;m_downloadReply=nullptr;reply->deleteLater();
    stageProviderArtifact(m_downloadStoreId, m_downloadResult);
    m_downloadData.clear();emit entriesChanged();
    m_downloadProgress = QString();
    emit downloadProgressChanged();
}

void AppCatalog::onDownloadReadyRead()
{
    if(!m_downloadReply)return;
    if(m_downloadData.size()+m_downloadReply->bytesAvailable()>128*1024*1024){m_downloadReply->abort();return;}
    m_downloadData += m_downloadReply->readAll();
}

void AppCatalog::onDownloadError(QNetworkReply::NetworkError /*code*/)
{
    // Error handling is done in onDownloadFinished via the error code check.
}

bool AppCatalog::uninstall(const QString &id)
{
    const auto *entry = findEntry(id);
    if (!entry || !isInstalled(id)) return false;
    if(busy() && !m_committing)return false;
    if(entry->app.type=="android" && !m_committing)return androidOperation(*entry,"uninstall");
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
    if(entry->custom){
        m_customPackages.remove(id);
        for(qsizetype i=0;i<m_entries.size();++i)if(m_entries[i].app.id==id){m_entries.removeAt(i);break;}
    }
    emit entriesChanged();
    emit packageUninstalled(id);
    return true;
}

bool AppCatalog::androidOperation(const Entry &entry, const QString &operation)
{
    QString helper;
    const auto base=QCoreApplication::applicationDirPath();
    for(const auto &path:{base+"/../share/isolde/android-session.py",base+"/share/isolde/android-session.py"})
        if(QFileInfo(path).isFile()){helper=path;break;}
    if(helper.isEmpty()){setError(tr("Android support is missing. Run the Isolde installer with Android support."));return false;}
    auto profile=qApp->property("isoldeProfile").toString();if(profile.isEmpty())profile="default";
    m_operationId=entry.app.id;m_operation=operation;m_operationEvents.clear();setError({});
    m_androidProcess.disconnect(this);
    connect(&m_androidProcess,&QProcess::readyReadStandardOutput,this,[this]{
        m_operationEvents=(m_operationEvents+m_androidProcess.readAllStandardOutput()).right(65536);
    });
    connect(&m_androidProcess,&QProcess::readyReadStandardError,this,[this]{m_androidProcess.readAllStandardError();});
    connect(&m_androidProcess,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart){m_operationId.clear();setError(tr("Android could not start. Check installer setup and retry."));emit entriesChanged();}
    });
    connect(&m_androidProcess,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,profile](int code,QProcess::ExitStatus status){
        const auto id=m_operationId,operation=m_operation;
        bool verified=false;QString message;
        for(const auto &line:m_operationEvents.split('\n')){const auto event=QJsonDocument::fromJson(line).object();
            if(event.value("event")=="completed" && event.value("operation")==operation
                && event.value(operation=="install"?"packagePresent":"packageAbsent").toBool())verified=true;
            if(event.value("event")=="error")message=event.value("message").toString().left(512);
        }
        auto active=qApp->property("isoldeProfile").toString();if(active.isEmpty())active="default";
        if(active!=profile){m_operationId.clear();setError(tr("The active profile changed; app activation was not saved."));emit entriesChanged();return;}
        if(!code && status==QProcess::NormalExit && verified){
            m_committing=true;
            const bool committed=operation=="install"?install(id):(!isInstalled(id) || uninstall(id));
            m_committing=false;
            if(!committed && operation=="install") {
                // Undo the runtime install when activation could not be saved.
                const auto *entry=findEntry(id);
                if(entry && entry->custom){const Entry rollback=*entry;m_operationId.clear();androidOperation(rollback,"uninstall");return;}
            }
        }
        else setError(message.isEmpty()?tr("Android could not finish. Your app list is unchanged; retry the operation."):message);
        m_operationId.clear();emit entriesChanged();
    });
    m_androidProcess.setProgram("/usr/bin/python3");
    QStringList arguments{helper,"--profile",profile,"--app",entry.app.launchConfiguration.value("runtimeApp").toString(),"--operation",operation};
    if(entry.custom && entry.app.type=="android")arguments << "--store-version" << entry.version;
    m_androidProcess.setArguments(arguments);
    m_androidProcess.start();emit entriesChanged();return true;
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
    QJsonObject customPackages;
    for (const auto &id : ids) {
        packageStates.insert(id, packageState(id));
        if(m_customPackages.contains(id))customPackages.insert(id,m_customPackages.value(id));
        const auto reason = m_failureReasons.value(id).trimmed();
        if (!reason.isEmpty())
            failureReasons.insert(id, reason);
    }
    QSaveFile file(m_statePath);
    const auto data = QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"installed", installed},
                                                {"packageStates", packageStates},
                                                {"failureReasons", failureReasons},{"customPackages",customPackages}}).toJson();
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
