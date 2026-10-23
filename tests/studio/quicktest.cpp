#include "home/ComponentSchema.h"
#include "home/HomeLayout.h"
#include "input/InputManager.h"
#include <QtQuickTest>
#include <QQmlContext>
#include <QQmlEngine>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

// No service, network client or normal-profile path is supplied to the editor.
// Each scenario owns a new saved document under this process's temporary root.
class StudioTestSetup : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString artifacts READ artifacts CONSTANT)
public:
    StudioTestSetup() : home(&registry) {}
    QString artifacts() const { return qEnvironmentVariable("ISOLDE_TEST_ARTIFACTS"); }
    Q_INVOKABLE bool reset(bool full = false, bool scrolling = false, bool flow = false, bool arrangement = false) {
        if (!temp.isValid()) return false;
        saved = temp.filePath(QString("scenario-%1.json").arg(++sequence));
        if (!home.load(QStringLiteral(STUDIO_DEFAULTS), saved)) return false;
        auto doc = QJsonDocument::fromJson(home.committedDocument()).object();
        doc["categories"] = QJsonArray{};
        doc["composition"] = QJsonObject{{"schemaVersion", 1}, {"enabled", true},
            {"verticalScrolling", scrolling}, {"showClock", false},
            {"grid", QJsonObject{{"columns", 4}, {"rows", 3}, {"mode", "screen"}}}};
        QJsonArray nodes;
        for (int i = 0; i < (arrangement ? 3 : full ? 12 : 1); ++i) {
            const auto node = ComponentSchema::component({{"id", QString("cell-%1").arg(i)},
                {"kind", "widget"}, {"widgetType", "text"}, {"name", QString("Item %1").arg(i)},
                {"placement", QVariantMap{{"x", arrangement ? i : i % 4}, {"y", arrangement ? i : i / 4}, {"width", 1}, {"height", 1}}}}, true);
            if (node.isEmpty()) return false;
            nodes.append(QJsonObject::fromVariantMap(node));
        }
        if (flow) {
            doc["composition"] = QJsonObject{{"schemaVersion", 1}, {"enabled", false}};
            for (int i = 0; i < nodes.size(); ++i) {
                auto node = nodes[i].toObject(); node.remove("placement"); nodes[i] = node;
            }
        }
        doc["components"] = nodes;
        QFile file(saved);
        const auto bytes = QJsonDocument(doc).toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) return false;
        file.close();
        return home.load(QStringLiteral(STUDIO_DEFAULTS), saved);
    }
    Q_INVOKABLE QString disk() const {
        QFile file(saved);
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    }
public slots:
    void qmlEngineAvailable(QQmlEngine *engine) {
        engine->rootContext()->setContextProperty("studioFixture", this);
        engine->rootContext()->setContextProperty("homeLayout", &home);
        engine->rootContext()->setContextProperty("inputManager", &input);
        engine->rootContext()->setContextProperty("isoldeVersion", "test");
    }
private:
    QTemporaryDir temp;
    AppRegistry registry;
    HomeLayout home;
    InputManager input;
    QString saved;
    int sequence = 0;
};

QUICK_TEST_MAIN_WITH_SETUP(studio, StudioTestSetup)
#include "quicktest.moc"
