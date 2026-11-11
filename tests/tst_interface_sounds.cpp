#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include "settings/InterfaceSounds.h"
#include "input/InputManager.h"

namespace {
QByteArray wave(int milliseconds=30) {
    const int samples=48*milliseconds;
    QByteArray result;QDataStream out(&result,QIODevice::WriteOnly);out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF",4);out<<quint32(36+samples*2);out.writeRawData("WAVEfmt ",8);
    out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(48000)<<quint32(96000)<<quint16(2)<<quint16(16);
    out.writeRawData("data",4);out<<quint32(samples*2);
    for(int i=0;i<samples;++i)out<<qint16(i%100);
    return result;
}
bool write(const QString &path,const QByteArray &bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size();
}
QByteArray read(const QString &path) {QFile file(path);return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray{};}
QString pack(const QString &root,const QString &name="Fixture") {
    const auto directory=root+"/resource.uisounds.fixture";
    write(directory+"/addon.xml",("<addon id=\"resource.uisounds.fixture\" name=\""+name+"\"><extension point=\"kodi.resource.uisounds\"/><extension point=\"xbmc.addon.metadata\"/></addon>").toUtf8());
    write(directory+"/resources/sounds.xml","<sounds><actions><action><name>down</name><file>click.wav</file></action><action><name>select</name><file>click.wav</file></action><action><name>previousmenu</name><file>click.wav</file></action><action><name>error</name><file>click.wav</file></action></actions></sounds>");
    write(directory+"/resources/click.wav",wave());return directory;
}
}

class InterfaceSoundTests : public QObject {
    Q_OBJECT
private slots:
    void optionsAndWaveBounds() {
        auto options=QJsonDocument::fromJson(read(INTERFACE_SOUND_DEFAULTS)).object().toVariantMap();
        QVERIFY(InterfaceSounds::validOptions(options));
        auto invalid=options;invalid["schemaVersion"]="1";QVERIFY(!InterfaceSounds::validOptions(invalid));
        invalid=options;auto events=invalid["events"].toList();events[1]=events[0];invalid["events"]=events;QVERIFY(!InterfaceSounds::validOptions(invalid));
        invalid=options;auto defaults=invalid["defaults"].toMap();defaults["volume"]=101;invalid["defaults"]=defaults;QVERIFY(!InterfaceSounds::validOptions(invalid));
        QVERIFY(InterfaceSounds::validWave(wave()));QVERIFY(!InterfaceSounds::validWave(wave(2001)));
        auto damaged=wave();damaged[4]=0;QVERIFY(!InterfaceSounds::validWave(damaged));
        damaged=wave();damaged[20]=3;QVERIFY(!InterfaceSounds::validWave(damaged));
        QVERIFY(!InterfaceSounds::validWave(QByteArray(600000,'x')));
    }
    void kodiMappingAndPathBoundaries() {
        QTemporaryDir dir;const auto root=pack(dir.path());QString error;
        const auto theme=InterfaceSounds::readKodiTheme(root,&error);
        QVERIFY2(!theme.isEmpty(),qPrintable(error));QCOMPARE(theme["id"].toString(),"resource.uisounds.fixture");
        QCOMPARE(theme["sounds"].toMap()["select"].toByteArray(),wave());
        const auto original=read(root+"/resources/sounds.xml");
        for(const auto &path:QList<QByteArray>{"../outside.wav","/tmp/outside.wav","https://example.invalid/audio.wav","sub/../../outside.wav","sub\\outside.wav"}) {
            auto unsafe=original;unsafe.replace("click.wav",path);QVERIFY(write(root+"/resources/sounds.xml",unsafe));
            QVERIFY(InterfaceSounds::readKodiTheme(root,&error).isEmpty());QVERIFY(!error.isEmpty());
        }
        QVERIFY(write(root+"/resources/sounds.xml","<!DOCTYPE sounds [<!ENTITY x SYSTEM 'file:///etc/passwd'>]><sounds>&x;</sounds>"));
        QVERIFY(InterfaceSounds::readKodiTheme(root).isEmpty());
        QVERIFY(write(root+"/resources/sounds.xml",original));
        QVERIFY(QFile::remove(root+"/resources/click.wav"));QVERIFY(write(dir.path()+"/outside.wav",wave()));
        QVERIFY(QFile::link(dir.path()+"/outside.wav",root+"/resources/click.wav"));
        QVERIFY(InterfaceSounds::readKodiTheme(root).isEmpty());
    }
    void preferencesAreAtomicAndProfileLocal() {
        QTemporaryDir dir;const auto file=dir.filePath("a/interface-sounds.json");
        const auto otherFile=dir.filePath("b/interface-sounds.json");
        InterfaceSounds a(file,{},nullptr,INTERFACE_SOUND_DEFAULTS,false),b(otherFile,{},nullptr,INTERFACE_SOUND_DEFAULTS,false);
        QVERIFY(!QFile::exists(file));QVERIFY(!QFile::exists(otherFile));
        QVERIFY(a.setVolume(35));QVERIFY(a.selectTheme("builtin.soft"));const auto before=read(file);
        InterfaceSounds restored(file,{},nullptr,INTERFACE_SOUND_DEFAULTS,false);
        QCOMPARE(restored.volume(),35);QCOMPARE(restored.theme(),"builtin.soft");QCOMPARE(read(file),before);
        QCOMPARE(b.volume(),20);QVERIFY(!QFile::exists(otherFile));
        QVERIFY(!a.setVolume(101));QCOMPARE(a.volume(),35);QCOMPARE(read(file),before);
        QVERIFY(QFile::rename(file,file+".held"));QVERIFY(QDir().mkdir(file));
        QVERIFY(!a.setEnabled(false));QVERIFY(a.enabled());QVERIFY(!a.errorString().isEmpty());
        QCOMPARE(read(file+".held"),before);QVERIFY(QDir().rmdir(file));QVERIFY(QFile::rename(file+".held",file));
        QVERIFY(write(otherFile,"{broken"));
        InterfaceSounds malformed(otherFile,{},nullptr,INTERFACE_SOUND_DEFAULTS,false);
        QVERIFY(!malformed.errorString().isEmpty());QCOMPARE(read(otherFile),QByteArray("{broken"));
    }
    void feedbackPolicyPreviewAndPrivateImport() {
        QTemporaryDir dir;const auto root=dir.path()+"/addons";const auto source=pack(root);
        pack(dir.path()+"/other-profile-addons","Other profile");
        const auto mapping=read(source+"/resources/sounds.xml"),manifest=read(source+"/addon.xml");
        InterfaceSounds sounds(dir.filePath("prefs.json"),{root},nullptr,INTERFACE_SOUND_DEFAULTS,false);
        for(const auto &theme:sounds.themes())QVERIFY(theme.toMap().value("name")!="Other profile");
        QVERIFY(sounds.selectTheme("resource.uisounds.fixture"));
        QSignalSpy cues(&sounds,&InterfaceSounds::cueRequested);
        sounds.playCommand(InputManager::NavigateDown);QCOMPARE(cues.count(),1);
        const auto url=qvariant_cast<QUrl>(cues[0][1]);QVERIFY(url.isLocalFile());
        QVERIFY(!url.toLocalFile().startsWith(source));QCOMPARE(read(url.toLocalFile()),wave());
        QCOMPARE(cues[0][2].toDouble(),.2);
        sounds.playCommand(InputManager::NavigateDown);QCOMPARE(cues.count(),1); // repeat suppression
        sounds.playEvent("error");QCOMPARE(cues.count(),2); // failure supersedes select/move
        sounds.playEvent("error");QCOMPARE(cues.count(),2);
        sounds.setMuted(true);sounds.playCommand(InputManager::Confirm);QVERIFY(!sounds.preview(sounds.theme()));QCOMPARE(cues.count(),2);
        sounds.setMuted(false);sounds.setPlaybackActive(true);QVERIFY(!sounds.preview(sounds.theme()));
        QVERIFY(sounds.setDuringPlayback(true));QVERIFY(sounds.preview(sounds.theme()));QVERIFY(sounds.previewing());
        sounds.setSuppressed(true);QVERIFY(!sounds.previewing());const auto stopped=cues.count();
        QTest::qWait(300);QCOMPARE(cues.count(),stopped);
        sounds.setSuppressed(false);sounds.setPlaybackActive(false);
        const auto prefs=read(dir.filePath("prefs.json"));
        QVERIFY(sounds.preview("builtin.soft"));sounds.stop();QCOMPARE(read(dir.filePath("prefs.json")),prefs);
        QCOMPARE(sounds.theme(),"resource.uisounds.fixture");
        QCOMPARE(read(source+"/resources/sounds.xml"),mapping);QCOMPARE(read(source+"/addon.xml"),manifest);
        QVERIFY(!sounds.selectTheme("resource.uisounds.missing"));QCOMPARE(sounds.theme(),"resource.uisounds.fixture");
    }
    void teardownCancelsPreviewAndRemovesPrivateSamples() {
        QTemporaryDir dir;auto *sounds=new InterfaceSounds(dir.filePath("prefs.json"),{},nullptr,INTERFACE_SOUND_DEFAULTS,false);
        QSignalSpy cues(sounds,&InterfaceSounds::cueRequested);QVERIFY(sounds->preview("builtin.soft"));
        QCOMPARE(cues.count(),1);const auto sample=qvariant_cast<QUrl>(cues[0][1]).toLocalFile();
        QVERIFY(QFileInfo::exists(sample));delete sounds;
        QVERIFY(!QFileInfo::exists(sample));QTest::qWait(300);QCOMPARE(cues.count(),1);
    }
    void brokenSelectedPackFallsBackWithoutRewritingTheChoice() {
        QTemporaryDir dir;const auto root=dir.path()+"/addons";const auto source=pack(root);
        const auto prefs=dir.filePath("prefs.json");InterfaceSounds sounds(prefs,{root},nullptr,INTERFACE_SOUND_DEFAULTS,false);
        QVERIFY(sounds.selectTheme("resource.uisounds.fixture"));const auto original=read(prefs);
        QVERIFY(write(source+"/resources/click.wav",QByteArray(100,'x')));sounds.refreshThemes();
        QCOMPARE(sounds.theme(),"resource.uisounds.fixture");QVERIFY(sounds.themeName().contains("using Soft"));
        QVERIFY(!sounds.errorString().isEmpty());QCOMPARE(read(prefs),original);
        QSignalSpy cues(&sounds,&InterfaceSounds::cueRequested);sounds.playCommand(InputManager::NavigateDown);
        QCOMPARE(cues.count(),1);QVERIFY(InterfaceSounds::validWave(read(qvariant_cast<QUrl>(cues[0][1]).toLocalFile())));
        QVERIFY(write(source+"/resources/click.wav",wave()));sounds.refreshThemes();
        QCOMPARE(sounds.themeName(),"Fixture");QCOMPARE(read(prefs),original);
    }
};
QTEST_GUILESS_MAIN(InterfaceSoundTests)
#include "tst_interface_sounds.moc"
