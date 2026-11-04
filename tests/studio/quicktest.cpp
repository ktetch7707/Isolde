#include "home/ComponentSchema.h"
#include "home/HomeLayout.h"
#include "home/StudioSession.h"
#include "input/InputManager.h"
#include "settings/DisplaySettings.h"
#include <QtQuickTest>
#include <QQmlContext>
#include <QQmlEngine>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QSettings>
#include <QCoreApplication>
#include <QImage>
#include <memory>
#include <chrono>
#include <QSaveFile>
#include <QDir>

// No service, network client or normal-profile path is supplied to the editor.
// Each scenario owns a new saved document under this process's temporary root.
class StudioTestSetup : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString artifacts READ artifacts CONSTANT)
public:
    StudioTestSetup() : home(&registry) {
        if(!temp.isValid())return;
        // Exercise the real global display binding without reading or writing
        // the guest's normal display preferences.
        QCoreApplication::setOrganizationName("IsoldeStudioTests");
        QCoreApplication::setApplicationName("StudioInput");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
        display=std::make_unique<DisplaySettings>();
    }
    QString artifacts() const { return qEnvironmentVariable("ISOLDE_TEST_ARTIFACTS"); }
    Q_INVOKABLE double monotonicMilliseconds() const {
        return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    Q_INVOKABLE int activeStudioClients() const {
        int count=0;for(auto *session:home.findChildren<StudioSession *>())if(session->property("active").toBool())++count;return count;
    }
    Q_INVOKABLE bool writePerformance(const QString &tag,const QVariantMap &receipt) const {
        if(!QStringList{"720","1080"}.contains(tag) || artifacts().isEmpty())return false;
        QSaveFile output(QDir(artifacts()).filePath("studio-performance-"+tag+".json"));
        const auto bytes=QJsonDocument::fromVariant(receipt).toJson();
        return output.open(QIODevice::WriteOnly) && output.write(bytes)==bytes.size() && output.commit();
    }

    Q_INVOKABLE QString referenceFixture() const {
        const auto directory = temp.filePath(QString("reference-input-%1").arg(sequence));
        if (!QDir().mkpath(directory)) return {};
        const auto path = directory + "/reference.png";
        QImage image(160, 90, QImage::Format_RGB32);
        image.fill(QColor("#6688aa"));
        return image.save(path) ? path : QString();
    }
    Q_INVOKABLE QString artworkFixture() const {
        const auto path=temp.filePath("artwork-input");QDir().mkpath(path);
        QFile file(path+"/animation.gif");if(!file.open(QIODevice::WriteOnly))return {};
        auto bytes=QByteArray::fromHex("47494638396101000100800000ff00000000ff");
        for(int i=0;i<2;++i){bytes+=QByteArray::fromHex("21f90400640000002c0000000001000100000202");bytes+=QByteArray::fromHex(i?"4c0100":"440100");}
        bytes+=char(0x3b);file.write(bytes);return file.fileName();
    }
    Q_INVOKABLE bool reset(bool full = false, bool scrolling = false, bool flow = false, bool arrangement = false, int scaleCount = 0) {
        if (!temp.isValid() || scaleCount < 0 || scaleCount > 1024) return false;
        const int columns = scaleCount ? 16 : 4;
        display->reset();
        saved = temp.filePath(QString("scenario-%1.json").arg(++sequence));
        if (!home.load(QStringLiteral(STUDIO_DEFAULTS), saved)) return false;
        auto doc = QJsonDocument::fromJson(home.committedDocument()).object();
        doc["categories"] = QJsonArray{};
        doc["composition"] = QJsonObject{{"schemaVersion", 1}, {"enabled", true},
            {"verticalScrolling", scrolling || scaleCount > 160}, {"showClock", false},
            {"grid", QJsonObject{{"columns", columns}, {"rows", scaleCount ? 10 : 3}, {"mode", "screen"}}}};
        QJsonArray nodes;
        for (int i = 0; i < (scaleCount ? scaleCount : arrangement ? 3 : full ? 12 : 1); ++i) {
            const auto node = ComponentSchema::component({{"id", QString("cell-%1").arg(i)},
                {"kind", "widget"}, {"widgetType", "text"}, {"name", QString("Item %1").arg(i)},
                {"placement", QVariantMap{{"x", arrangement ? i : i % columns}, {"y", arrangement ? i : i / columns}, {"width", 1}, {"height", 1}}}}, true);
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
        engine->rootContext()->setContextProperty("displaySettings", display.get());
        engine->rootContext()->setContextProperty("isoldeVersion", "test");
    }
private:
    QTemporaryDir temp;
    AppRegistry registry;
    HomeLayout home;
    InputManager input;
    QString saved;
    int sequence = 0;
    std::unique_ptr<DisplaySettings> display;
};

QUICK_TEST_MAIN_WITH_SETUP(studio, StudioTestSetup)
#include "quicktest.moc"
