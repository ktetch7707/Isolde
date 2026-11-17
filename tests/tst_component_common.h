#ifndef TST_COMPONENT_COMMON_H
#define TST_COMPONENT_COMMON_H

#include <QColor>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QUuid>
#include <QVariantList>
#include <QVariantMap>

// Shared test data (from original tst_components.cpp)
static const QByteArray apps = R"([
 {"id":"movies","name":"Movies","type":"mock","section":"Library","accent":"#556677","handledActions":["open-folder","open-file","open-disc"]},
 {"id":"other","name":"Other","type":"mock","section":"Apps","accent":"#445566","handledActions":["open-folder"]},
 {"id":"capture-test","name":"Capture test","type":"mock","presentationMode":"capture","section":"Inputs","accent":"#445566"},
 {"id":"settings","name":"Settings","type":"settings","section":"System","accent":"#556677"},
 {"id":"app-store","name":"Store","type":"store","section":"System","accent":"#556677"}
])";

static const QByteArray legacy =
    R"({"schemaVersion":1,"theme":{"backgroundStart":"#123456","backgroundEnd":"#010203","tileRoundness":0.73},
 "categories":[{"id":"personal","name":"Personal"}],"items":[
 {"id":"my-folder","kind":"action","name":"My films","categoryId":"personal","size":"wide","opacity":0.42,"showTitle":false,"customLogo":"artwork/my-folder.png","actions":[{"type":"open-folder","target":"/media/films","openWith":"gone"}]},
 {"id":"my-app","kind":"app","appId":"absent","size":"compact"},
 {"id":"clock","kind":"widget","widgetType":"clock"}]})";

static void
writeFile(const QString& path, const QByteArray& data)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(data);
}
static QByteArray
readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

#include <QCoreApplication>
#endif // TST_COMPONENT_COMMON_H