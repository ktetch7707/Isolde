#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include "media/GuideStore.h"
#include "media/GuideRules.h"
#include "home/ComponentSchema.h"

namespace {
constexpr qint64 Now=1793052000, Day=86400;
QVariantMap definition(const QString &id="cartoons", int number=101) {
    return {{"schemaVersion",1},{"id",id},{"name","Cartoon Network"},{"number",number},{"clock","real-time"},
        {"sequence","seeded-shuffle"},{"seed",42},{"timezone","America/Los_Angeles"},
        {"rule",QVariantMap{{"field","studio"},{"op","contains"},{"value","Cartoon Network"}}}};
}
QVariantList media() {
    QVariantList result;
    for (int i=1;i<=8;++i) result.append(QVariantMap{{"mediaType",i%2?"episode":"movie"},{"mediaId",i},
        {"title",QString("Program %1").arg(i)},{"duration",1800+i*60},{"studio",QStringList{"Cartoon Network"}},
        {"showtitle",i%2?"Series A":""},{"season",1},{"episode",i},{"year",2000+i},{"playcount",0}});
    return result;
}
QByteArray bytes(const QString &path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; return file.readAll(); }
}
class GuideTests final : public QObject {
    Q_OBJECT
private slots:
    void rulesAreBoundedAndMixedMediaMatches();
    void persistsSevenDaysAndTwelveHourOffset();
    void channelAndExtensionFailuresRollback();
    void rulesOnlyAffectUncommittedSlots();
    void targetedMissingMediaPreservesIntervals();
    void profileIsolationAndInvalidation();
    void lineupPreferencesSurviveRestart();
    void virtualClockPausesWhileProfileIsOff();
    void unknownDatabaseIsNeverRewritten();
    void intervalOverlapRejectedAtDatabaseBoundary();
};

void GuideTests::rulesAreBoundedAndMixedMediaMatches() {
    auto def=definition(); QString error;
    QVERIFY2(!ComponentSchema::guideChannel(def,&error).isEmpty(),qPrintable(error));
    for (const auto &field:QStringList{"schemaVersion","number","seed"}) { auto bad=def; bad[field]="1"; QVERIFY(ComponentSchema::guideChannel(bad).isEmpty()); }
    for (const auto &field:QStringList{"clock","sequence","timezone"}) { auto bad=def; bad[field]="unknown"; QVERIFY(ComponentSchema::guideChannel(bad).isEmpty()); }
    auto bad=def; bad["exec"]="anything"; QVERIFY(ComponentSchema::guideChannel(bad).isEmpty());
    bad=def; bad["id"]="../other"; QVERIFY(ComponentSchema::guideChannel(bad).isEmpty());
    QVariantMap condition{{"op","all"},{"rules",QVariantList{
        def.value("rule"),QVariantMap{{"op","any"},{"rules",QVariantList{
            QVariantMap{{"field","mediaType"},{"op","is"},{"value","movie"}},
            QVariantMap{{"field","mediaType"},{"op","is"},{"value","episode"}}}}},
        QVariantMap{{"op","not"},{"rules",QVariantList{QVariantMap{{"field","year"},{"op","before"},{"value",2000}}}}}}}};
    QVERIFY(!ComponentSchema::guideRule(condition).isEmpty());
    for (const auto &item:media()) QVERIFY(GuideRules::matches(condition,item.toMap()));
    auto excluded=media().first().toMap(); excluded["year"]=1990; QVERIFY(!GuideRules::matches(condition,excluded));
    QVariantMap missing{{"field","genre"},{"op","missing"}};
    QVERIFY(GuideRules::matches(missing,excluded));
    auto negative=QVariantMap{{"field","genre"},{"op","is-not"},{"value","Horror"}};
    QVERIFY(!GuideRules::matches(negative,excluded)); // missing metadata is not guessed
    auto range=QVariantMap{{"field","year"},{"op","in-range"},{"value",QVariantList{2001,2005}}};
    QVERIFY(!ComponentSchema::guideRule(range).isEmpty()); QVERIFY(GuideRules::matches(range,media().first().toMap()));
    range["value"]=QVariantList{2005,2001}; QVERIFY(ComponentSchema::guideRule(range).isEmpty());
    auto deep=missing; for (int i=0;i<10;++i) deep={{"op","not"},{"rules",QVariantList{deep}}};
    QVERIFY(ComponentSchema::guideRule(deep).isEmpty());
    QVariantList wide; for (int i=0;i<33;++i) wide.append(missing);
    QVERIFY(ComponentSchema::guideRule({{"op","any"},{"rules",wide}}).isEmpty());
}
void GuideTests::persistsSevenDaysAndTwelveHourOffset() {
    QTemporaryDir dir; const auto path=dir.filePath("guide.sqlite"); QVariantList baseline; QVariantMap target;
    {
        GuideStore store(path); QVERIFY2(store.ready(),qPrintable(store.errorString()));
        QVERIFY2(store.createLocal(definition(),media(),Now),qPrintable(store.errorString()));
        baseline=store.programs("cartoons",Now,Now+7*Day,1000); QVERIFY(baseline.size()>200);
        target=store.tuneTarget("cartoons",Now+12*3600+351);
        QVERIFY(!target.isEmpty());
        QCOMPARE(target.value("offsetSeconds").toLongLong(),Now+12*3600+351-target.value("startUtc").toLongLong());
        QVERIFY(target.value("offsetSeconds").toLongLong()<target.value("endUtc").toLongLong()-target.value("startUtc").toLongLong());
        for (int i=1;i<baseline.size();++i) QCOMPARE(baseline[i-1].toMap().value("endUtc"),baseline[i].toMap().value("startUtc"));
        const auto boundary=baseline.first().toMap().value("endUtc").toLongLong();
        QCOMPARE(store.tuneTarget("cartoons",boundary).value("id"),baseline[1].toMap().value("id"));
    }
    for (int restart=0;restart<3;++restart) {
        GuideStore store(path); QVERIFY(store.ready());
        QCOMPARE(store.programs("cartoons",Now,Now+7*Day,1000),baseline);
        QCOMPARE(store.tuneTarget("cartoons",Now+12*3600+351),target);
        QVERIFY(store.extend("cartoons",Now+8*Day,1));
        QCOMPARE(store.programs("cartoons",Now,Now+7*Day,1000),baseline);
    }
}
void GuideTests::channelAndExtensionFailuresRollback() {
    QTemporaryDir dir; GuideStore store(dir.filePath("guide.sqlite"));
    store.failNextCommitForTest(); QVERIFY(!store.createLocal(definition(),media(),Now));
    QVERIFY(store.channels().isEmpty()); QVERIFY(store.programs("cartoons",Now,Now+Day).isEmpty());
    QVERIFY(store.createLocal(definition(),media(),Now,1));
    const auto baseline=store.programs("cartoons",Now,Now+3*Day,1000);
    store.failNextCommitForTest(); QVERIFY(!store.extend("cartoons",Now+3*Day,1));
    QCOMPARE(store.programs("cartoons",Now,Now+3*Day,1000),baseline);
    QVERIFY(store.extend("cartoons",Now+3*Day,1));
    QVERIFY(store.programs("cartoons",Now,Now+3*Day,1000).size()>baseline.size());
    QVERIFY(!store.createLocal(definition("duplicate-number"),media(),Now)); QCOMPARE(store.channels().size(),1);
    auto invalid=media(); auto item=invalid.first().toMap(); item["duration"]=0; invalid[0]=item;
    QVERIFY(!store.createLocal(definition("bad",102),invalid,Now)); QCOMPARE(store.channels().size(),1);
    QVERIFY(!store.extend("cartoons",Now+100*Day,1));
}
void GuideTests::rulesOnlyAffectUncommittedSlots() {
    QTemporaryDir dir; GuideStore store(dir.filePath("guide.sqlite"));
    QVERIFY(store.createLocal(definition(),media(),Now,1));
    const auto baseline=store.programs("cartoons",Now,Now+Day,1000);
    auto def=definition(); def["rule"]=QVariantMap{{"field","mediaType"},{"op","is"},{"value","movie"}};
    QVERIFY(store.updateRules(def,media(),1));
    QVERIFY(!store.updateRules(def,media(),1)); // stale editor
    QCOMPARE(store.programs("cartoons",Now,Now+Day,1000),baseline);
    QVERIFY(store.extend("cartoons",Now+2*Day,2));
    const auto boundary=baseline.last().toMap().value("endUtc").toLongLong();
    for (const auto &v:store.programs("cartoons",boundary,Now+2*Day,1000)) QCOMPARE(v.toMap().value("mediaType").toString(),QString("movie"));
}
void GuideTests::targetedMissingMediaPreservesIntervals() {
    QTemporaryDir dir; GuideStore store(dir.filePath("guide.sqlite"));
    QVERIFY(store.createLocal(definition(),media(),Now,1));
    const auto baseline=store.programs("cartoons",Now,Now+Day,1000);
    const auto missing=baseline.first().toMap();
    QVERIFY(store.markUnavailable(missing.value("mediaType").toString(),missing.value("mediaId").toInt()));
    const auto repaired=store.programs("cartoons",Now,Now+Day,1000); QCOMPARE(baseline.size(),repaired.size());
    for (int i=0;i<baseline.size();++i) {
        auto original=baseline[i].toMap();
        if (original.value("mediaType")==missing.value("mediaType") && original.value("mediaId")==missing.value("mediaId")) original["status"]="unavailable";
        QCOMPARE(repaired[i].toMap(),original);
    }
    QVERIFY(store.tuneTarget("cartoons",missing.value("startUtc").toLongLong()).isEmpty());
    const auto boundary=baseline.last().toMap().value("endUtc").toLongLong();
    QVERIFY(store.extend("cartoons",Now+2*Day,2));
    for (const auto &v:store.programs("cartoons",boundary,Now+2*Day,1000)) {
        const auto item=v.toMap();
        QVERIFY(item.value("mediaType")!=missing.value("mediaType") || item.value("mediaId")!=missing.value("mediaId"));
    }
}
void GuideTests::profileIsolationAndInvalidation() {
    QTemporaryDir dir;
    GuideStore a(dir.filePath("a/guide.sqlite")),b(dir.filePath("b/guide.sqlite"));
    QVERIFY(a.createLocal(definition(),media(),Now,1)); QVERIFY(b.channels().isEmpty());
    QVERIFY(b.createLocal(definition(),media(),Now+Day,1));
    a.invalidate(); QVERIFY(a.channels().isEmpty()); QVERIFY(!a.extend("cartoons",Now+2*Day,1));
    QVERIFY(!a.createLocal(definition("late",102),media(),Now)); QVERIFY(!b.channels().isEmpty());
    GuideStore reopened(dir.filePath("a/guide.sqlite")); QCOMPARE(reopened.channels().size(),1);
}
void GuideTests::lineupPreferencesSurviveRestart() {
    QTemporaryDir dir; const auto path=dir.filePath("guide.sqlite");
    {
        GuideStore store(path); QVERIFY(store.createLocal(definition(),media(),Now,1));
        QVERIFY(store.createLocal(definition("second",102),media(),Now,1));
        QVERIFY(store.setPreferences("cartoons",{{"favorite",true},{"hidden",true}},1));
        QVERIFY(!store.setPreferences("cartoons",{{"favorite",false}},1));
        QVERIFY(!store.reorder({"cartoons","cartoons"}));
        store.failNextCommitForTest(); QVERIFY(!store.reorder({"second","cartoons"}));
        QCOMPARE(store.channels().first().toMap().value("id").toString(),QString("cartoons"));
        QVERIFY(store.reorder({"second","cartoons"}));
        QVERIFY(store.tuneTarget("cartoons",Now).isEmpty());
    }
    GuideStore store(path); const auto list=store.channels(); QCOMPARE(list.size(),2);
    QCOMPARE(list.first().toMap().value("id").toString(),QString("second"));
    QVERIFY(list.last().toMap().value("favorite").toBool()); QVERIFY(list.last().toMap().value("hidden").toBool());
    const auto revision=list.first().toMap().value("revision").toInt();
    store.failNextCommitForTest(); QVERIFY(!store.remove("second",revision)); QCOMPARE(store.channels().size(),2);
    QVERIFY(store.remove("second",revision)); QCOMPARE(store.channels().size(),1);
    QVERIFY(store.programs("second",Now,Now+Day).isEmpty());
}
void GuideTests::virtualClockPausesWhileProfileIsOff() {
    QTemporaryDir dir; const auto path=dir.filePath("guide.sqlite"); auto def=definition(); def["clock"]="pause-when-off";
    QVariantMap paused;
    {
        GuideStore store(path); QVERIFY(store.createLocal(def,media(),Now));
        paused=store.tuneTarget("cartoons",Now+800);
        QVERIFY(store.setActive(false,Now+800));
        QCOMPARE(store.tuneTarget("cartoons",Now+12*3600),paused);
    }
    GuideStore store(path); QCOMPARE(store.tuneTarget("cartoons",Now+12*3600),paused);
    QVERIFY(store.setActive(true,Now+12*3600));
    QCOMPARE(store.tuneTarget("cartoons",Now+12*3600),paused);
    QCOMPARE(store.tuneTarget("cartoons",Now+12*3600+10).value("offsetSeconds").toLongLong(),paused.value("offsetSeconds").toLongLong()+10);
    QVERIFY(!store.setActive(false,Now-1));
}
void GuideTests::unknownDatabaseIsNeverRewritten() {
    QTemporaryDir dir; const auto path=dir.filePath("unknown.sqlite");
    {
        auto db=QSqlDatabase::addDatabase("QSQLITE","unknown-guide"); db.setDatabaseName(path); QVERIFY(db.open());
        { QSqlQuery q(db); QVERIFY(q.exec("CREATE TABLE personal_data (value TEXT)")); QVERIFY(q.exec("PRAGMA user_version=77")); }
        db.close();
    }
    QSqlDatabase::removeDatabase("unknown-guide"); const auto before=bytes(path); QVERIFY(!before.isEmpty());
    { GuideStore store(path); QVERIFY(!store.ready()); QVERIFY(!store.errorString().isEmpty()); }
    QCOMPARE(bytes(path),before);
}
void GuideTests::intervalOverlapRejectedAtDatabaseBoundary() {
    QTemporaryDir dir; const auto path=dir.filePath("guide.sqlite");
    GuideStore store(path); QVERIFY(store.createLocal(definition(),media(),Now,1));
    {
        auto db=QSqlDatabase::addDatabase("QSQLITE","overlap-guide"); db.setDatabaseName(path); QVERIFY(db.open());
        { QSqlQuery q(db); q.prepare("INSERT INTO slots VALUES('intruder','cartoons','movie',1,'Overlap',?,?,'ready')");
          q.addBindValue(Now+1); q.addBindValue(Now+20); QVERIFY(!q.exec()); QVERIFY(q.lastError().text().contains("overlapping")); }
        db.close();
    }
    QSqlDatabase::removeDatabase("overlap-guide");
}
QTEST_GUILESS_MAIN(GuideTests)
#include "tst_guide.moc"
