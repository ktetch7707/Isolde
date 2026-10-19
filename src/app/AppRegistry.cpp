#include "app/AppRegistry.h"

#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUrl>
#include <utility>

AppRegistry::AppRegistry(QObject *parent) : QObject(parent) {}

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

QVariantList AppRegistry::apps() const
{
    QVariantList result;
    result.reserve(m_apps.size());
    for (const auto &app : m_apps) {
        result.append(QVariantMap{{"id", app.id}, {"name", app.name}, {"type", app.type},
                                  {"presentationMode", app.presentationMode},
                                  {"networkRequirement", app.networkRequirement},
                                  {"handledActions", app.handledActions},
                                  {"section", app.section}, {"symbol", app.symbol},
                                  {"accent", app.accent.name()}});
    }
    return result;
}

QString AppRegistry::errorString() const { return m_errorString; }

const AppDefinition *AppRegistry::find(const QString &id) const
{
    for (const auto &app : m_apps)
        if (app.id == id)
            return &app;
    return nullptr;
}

bool AppRegistry::addApp(const AppDefinition &app)
{
    if (!app.isValid() || find(app.id))
        return false;
    m_apps.append(app);
    emit appsChanged();
    return true;
}

bool AppRegistry::removeApp(const QString &id)
{
    for (qsizetype i = 0; i < m_apps.size(); ++i) {
        if (m_apps.at(i).id == id) {
            m_apps.removeAt(i);
            emit appsChanged();
            return true;
        }
    }
    return false;
}

bool AppRegistry::load(const QString &path)
{
    QString resolvedPath = path;
    if (resolvedPath.isEmpty()) {
        resolvedPath = QDir(QCoreApplication::applicationDirPath()).filePath("config/apps.json");
        if (!QFile::exists(resolvedPath))
            resolvedPath = QStringLiteral(":/config/apps.json");
    }
    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(tr("Unable to open app configuration: %1").arg(resolvedPath));
        return false;
    }
    return loadFromData(file.readAll());
}

bool AppRegistry::loadFromData(const QByteArray &data)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        setError(tr("Invalid app configuration: %1").arg(parseError.errorString()));
        return false;
    }

    QVector<AppDefinition> loaded;
    QSet<QString> ids;
    for (const auto &value : document.array()) {
        if (!value.isObject()) {
            setError(tr("Every app entry must be an object."));
            return false;
        }
        const auto object = value.toObject();
        AppDefinition app;
        app.id = object.value("id").toString().trimmed();
        app.name = object.value("name").toString().trimmed();
        app.type = object.value("type").toString().trimmed();
        app.presentationMode = object.value("presentationMode").toString(QStringLiteral("native-tv")).trimmed();
        app.networkRequirement = object.value("networkRequirement").toString(
            app.type == QStringLiteral("web") ? QStringLiteral("internet") : QStringLiteral("none")).trimmed();
        app.handledActions = readStringArray(object.value(QStringLiteral("handledActions")).toArray());
        app.section = object.value("section").toString().trimmed();
        app.symbol = object.value("symbol").toString().trimmed();
        app.accent = QColor(object.value("accent").toString("#64748B"));
        app.launchConfiguration = object.toVariantMap();
        static const QSet<QString> presentationModes{QStringLiteral("native-tv"), QStringLiteral("web-kiosk"),
                                                     QStringLiteral("media"), QStringLiteral("capture"),
                                                     QStringLiteral("game-hub"), QStringLiteral("game"),
                                                     QStringLiteral("disc-media"), QStringLiteral("disc-game"),
                                                     QStringLiteral("utility-desktop"), QStringLiteral("desktop-bridge")};
        static const QSet<QString> networkRequirements{QStringLiteral("none"), QStringLiteral("local-network"),
                                                       QStringLiteral("internet")};
        static const QSet<QString> handledActionTypes{QStringLiteral("open-folder"), QStringLiteral("open-file"),
                                                      QStringLiteral("open-url"), QStringLiteral("open-disc")};
        const auto url = QUrl(object.value(QStringLiteral("url")).toString());
        bool validOrigins = true;
        for (const auto &origin : object.value(QStringLiteral("allowedOrigins")).toArray())
            validOrigins = validOrigins && isSafeWebOrigin(origin.toString());
        const bool validWeb = app.type != QStringLiteral("web") || (isSafeWebUrl(url) && validOrigins);
        if (!app.isValid() || ids.contains(app.id) || !app.accent.isValid()
            || !presentationModes.contains(app.presentationMode) || !validWeb) {
            setError(tr("App entries require unique IDs, names, types, valid accent colors, known presentation/network modes, and safe web URLs/origins."));
            return false;
        }
        if (!networkRequirements.contains(app.networkRequirement)) {
            setError(tr("App entries require a known network requirement."));
            return false;
        }
        for (const auto &handledAction : app.handledActions) {
            if (!handledActionTypes.contains(handledAction)) {
                setError(tr("App entries require known handled action types."));
                return false;
            }
        }
        ids.insert(app.id);
        loaded.append(app);
    }
    if (loaded.isEmpty()) {
        setError(tr("App configuration contains no apps."));
        return false;
    }

    m_apps = std::move(loaded);
    setError({});
    emit appsChanged();
    return true;
}

void AppRegistry::setError(const QString &message)
{
    if (m_errorString == message)
        return;
    m_errorString = message;
    emit errorStringChanged();
}
