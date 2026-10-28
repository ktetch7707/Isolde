#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include "media/GuideStore.h"
#include "media/GuideRules.h"
#include "home/ComponentSchema.h"
#include "home/HomeLayout.h"
#include "app/AppRegistry.h"

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
    void packagedGuideOptionsValidateOverlayAndLegacyBoundaries();
    void persistsSevenDaysAndTwelveHourOffset();
    void channelAndExtensionFailuresRollback();
    void rulesOnlyAffectUncommittedSlots();
    void nextUnwatchedUsesStrictKodiSnapshotAndPreservesCommittedSchedule();
    void targetedMissingMediaPreservesIntervals();
    void profileIsolationAndInvalidation();
    void lineupPreferencesSurviveRestart();
    void numberSwapIsRevisionedAtomicAndPreservesSchedules();
    void channelSchedulePolicyPersistsAndPrunesWithoutReplacingFuture();
    void selectedSlotRepairPreservesIntervalsAndRejectsStaleOrForeignChoices();
    void virtualClockPausesWhileProfileIsOff();
    void unknownDatabaseIsNeverRewritten();
    void corruptKnownDatabaseIsNeverRewritten();
    void intervalOverlapRejectedAtDatabaseBoundary();
    void cancelledTransactionsAndInterruptedVirtualClock();
    void rollingMaintenancePreservesSlotsAndCatchesUpAfterLongOutage();
    void pvrGenerationIsAtomicAndPreservesUserOverrides();
    void regenerationRequiresCurrentReviewAndPreservesAiringProgram();
};

void GuideTests::packagedGuideOptionsValidateOverlayAndLegacyBoundaries() {
    QFile file(QFINDTESTDATA("../config/studio-defaults.json"));QVERIFY(file.open(QIODevice::ReadOnly));
    const auto document=QJsonDocument::fromJson(file.readAll()).object();const auto packaged=document.value("guide").toObject().toVariantMap();QString error;
    QVERIFY2(!ComponentSchema::guideOptions(packaged,&error).isEmpty(),qPrintable(error));
    auto legacy=packaged;for(const auto &key:QStringList{"surface","overlayWidth","overlayHeight","overlayRows","overlayPosition"})legacy.remove(key);
    QVERIFY2(!ComponentSchema::guideOptions(legacy,&error).isEmpty(),qPrintable(error));
    for(const auto &value:QVariantList{64,101,95.5,true,QString("95")}){auto invalid=packaged;invalid["overlayWidth"]=value;QVERIFY(ComponentSchema::guideOptions(invalid).isEmpty());}
    for(const auto &key:QStringList{"surface","overlayPosition"}){auto invalid=packaged;invalid[key]="unsupported";QVERIFY(ComponentSchema::guideOptions(invalid).isEmpty());}
    auto unknown=packaged;unknown["executableLayout"]="script";QVERIFY(ComponentSchema::guideOptions(unknown).isEmpty());
    QTemporaryDir dir;AppRegistry registry;HomeLayout normal(&registry);QVERIFY(normal.load(QFINDTESTDATA("../config/default-home-layout.json"),dir.filePath("normal-profile.json")));QVERIFY(!normal.studioOptions().isEmpty());
    const auto localDefaults=dir.filePath("default-home-layout.json");QVERIFY(QFile::copy(QFINDTESTDATA("../config/default-home-layout.json"),localDefaults));
    auto custom=document;custom["guide"]=QJsonObject::fromVariantMap(legacy);const auto legacyBytes=QJsonDocument(custom).toJson(QJsonDocument::Compact);QVERIFY(legacyBytes.size()<=64*1024);const auto customPath=dir.filePath("studio-defaults.json");
    {QFile output(customPath);QVERIFY(output.open(QIODevice::WriteOnly));QCOMPARE(output.write(legacyBytes),legacyBytes.size());}
    QVERIFY(QFile::exists(":/config/studio-defaults.json"));HomeLayout compatible(&registry);QVERIFY(compatible.load(localDefaults,dir.filePath("legacy-profile.json")));const auto resolved=compatible.studioOptions();QVERIFY(!resolved.isEmpty());
    for(const auto &key:QStringList{"surface","overlayWidth","overlayHeight","overlayRows","overlayPosition"})QCOMPARE(resolved.value("guide").toMap().value(key),packaged.value(key));
    QCOMPARE(bytes(customPath),legacyBytes);QVERIFY(!QFile::exists(dir.filePath("legacy-profile.json")));
}
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
void GuideTests::nextUnwatchedUsesStrictKodiSnapshotAndPreservesCommittedSchedule() {
    auto def=definition();def["sequence"]="next-unwatched";
    QVariantList candidates;
    const QVariantList states{0,0,1,QVariant(),false,QString("0"),-1};
    for(int i=0;i<states.size();++i){auto item=media().first().toMap();item["mediaId"]=i+1;item["duration"]=3600;item["episode"]=2-i;item["showtitle"]="Series";if(states[i].isValid())item["playcount"]=states[i];else item.remove("playcount");candidates.append(item);}
    QTemporaryDir dir;const auto path=dir.filePath("guide.sqlite");QVariantList committed;
    {
        GuideStore store(path);QVERIFY(store.createLocal(def,candidates,Now,1));
        committed=store.programs("cartoons",Now,Now+Day,1000);QCOMPARE(committed.size(),24);
        for(int i=0;i<committed.size();++i)QCOMPARE(committed[i].toMap().value("mediaId").toInt(),i%2?1:2);
        auto watched=candidates;for(auto &value:watched){auto item=value.toMap();item["playcount"]=1;value=item;}
        const auto channels=store.channels();
        auto empty=def;empty["id"]="all-watched";empty["number"]=102;QVERIFY(!store.createLocal(empty,watched,Now));QCOMPARE(store.channels(),channels);
        QVERIFY(!store.updateRules(def,watched,1));QCOMPARE(store.channels(),channels);QCOMPARE(store.programs("cartoons",Now,Now+Day,1000),committed);
        auto refreshed=candidates;auto first=refreshed[0].toMap();first["playcount"]=1;refreshed[0]=first;
        auto third=refreshed[2].toMap();third["playcount"]=0;third["episode"]=0;refreshed[2]=third;
        QVERIFY(store.updateRules(def,refreshed,1));QCOMPARE(store.programs("cartoons",Now,Now+Day,1000),committed);
        QVERIFY(store.extend("cartoons",Now+2*Day,2));const auto later=store.programs("cartoons",Now+Day,Now+2*Day,1000);QCOMPARE(later.size(),24);
        for(int i=0;i<later.size();++i)QCOMPARE(later[i].toMap().value("mediaId").toInt(),i%2?2:3);
    }
    GuideStore reopened(path);QCOMPARE(reopened.programs("cartoons",Now,Now+Day,1000),committed);
    QCOMPARE(reopened.tuneTarget("cartoons",Now+Day).value("mediaId").toInt(),3);
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
        const QVariantMap revisions{{"cartoons",2},{"second",1}};
        QVERIFY(!store.reorder({"cartoons","cartoons"},revisions));
        store.failNextCommitForTest(); QVERIFY(!store.reorder({"second","cartoons"},revisions));
        QCOMPARE(store.channels().first().toMap().value("id").toString(),QString("cartoons"));
        QVERIFY(store.reorder({"second","cartoons"},revisions));
        QVERIFY(!store.reorder({"cartoons","second"},revisions));
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
void GuideTests::numberSwapIsRevisionedAtomicAndPreservesSchedules() {
    QTemporaryDir dir;GuideStore store(dir.filePath("guide.sqlite"));
    QVERIFY(store.createLocal(definition(),media(),Now,1));QVERIFY(store.createLocal(definition("second",102),media(),Now,1));
    const auto channels=store.channels(),first=store.programs("cartoons",Now,Now+Day,1000),second=store.programs("second",Now,Now+Day,1000);
    const QVariantMap review{{"cartoons",1},{"second",1}};
    QVERIFY(!store.swapNumbers("cartoons","second",{{"cartoons",1},{"second",2}}));QCOMPARE(store.channels(),channels);
    store.failNextCommitForTest();QVERIFY(!store.swapNumbers("cartoons","second",review));QCOMPARE(store.channels(),channels);
    QVERIFY(store.swapNumbers("cartoons","second",review));
    QCOMPARE(store.channel("cartoons").value("number").toInt(),102);QCOMPARE(store.channel("second").value("number").toInt(),101);
    QCOMPARE(store.channel("cartoons").value("revision").toInt(),2);QCOMPARE(store.channel("second").value("revision").toInt(),2);
    QVERIFY(!store.swapNumbers("cartoons","second",review));
    QCOMPARE(store.programs("cartoons",Now,Now+Day,1000),first);QCOMPARE(store.programs("second",Now,Now+Day,1000),second);
}
void GuideTests::channelSchedulePolicyPersistsAndPrunesWithoutReplacingFuture() {
    QTemporaryDir dir;const auto path=dir.filePath("policy.sqlite");auto def=definition();def["horizonDays"]=3;def["retentionDays"]=0;
    QVariantList future;
    {GuideStore store(path);QVERIFY(store.createLocal(def,media(),Now));QCOMPARE(store.definition("cartoons"),def);
     QVERIFY(store.channel("cartoons").value("generatedThroughUtc").toLongLong()>=Now+3*Day);
     future=store.programs("cartoons",Now+Day,Now+3*Day,1000);}
    GuideStore store(path);QVERIFY(store.ready());QCOMPARE(store.definition("cartoons"),def);
    def["horizonDays"]=1;QVERIFY(store.updateRules(def,media(),1));QVERIFY(store.maintain("cartoons",Now+Day,2));
    QCOMPARE(store.programs("cartoons",Now+Day,Now+3*Day,1000),future); // reducing horizon preserves committed future
    for(const auto &slot:store.programs("cartoons",Now,Now+Day,1000))QVERIFY(slot.toMap().value("endUtc").toLongLong()>Now+Day);
    const auto saved=store.channels();
    for(const auto &value:QVariantList{0,32,true,1.5}){auto invalid=def;invalid["horizonDays"]=value;QVERIFY(!store.updateRules(invalid,media(),2));QCOMPARE(store.channels(),saved);}
    for(const auto &value:QVariantList{-1,8,true,1.5}){auto invalid=def;invalid["retentionDays"]=value;QVERIFY(!store.updateRules(invalid,media(),2));QCOMPARE(store.channels(),saved);}
    auto legacy=definition("legacy",102);QVERIFY(store.createLocal(legacy,media(),Now));QCOMPARE(store.definition("legacy"),legacy);
    QVERIFY(store.channel("legacy").value("generatedThroughUtc").toLongLong()>=Now+7*Day);
    QVERIFY(store.maintain("legacy",Now+3*Day,1));
    const auto retained=store.programs("legacy",Now,Now+3*Day,1000);QVERIFY(!retained.isEmpty());
    for(const auto &slot:retained)QVERIFY(slot.toMap().value("endUtc").toLongLong()>Now+Day); // legacy two-day history
}
void GuideTests::selectedSlotRepairPreservesIntervalsAndRejectsStaleOrForeignChoices() {
    QTemporaryDir dir;GuideStore store(dir.filePath("repair.sqlite"));auto candidates=media();
    for(int i=0;i<candidates.size();++i){auto item=candidates[i].toMap();item["duration"]=i==7?1900:1800;candidates[i]=item;}
    QVERIFY(store.createLocal(definition(),candidates,Now,1));const auto schedule=store.programs("cartoons",Now,Now+Day,1000);int index=0;while(schedule[index].toMap().value("endUtc").toLongLong()-schedule[index].toMap().value("startUtc").toLongLong()!=1800)++index;const auto slot=schedule[index].toMap();const auto id=slot.value("id").toString();
    const auto review=store.repairPreview("cartoons",id,1,Now);QVERIFY(!review.isEmpty());QCOMPARE(review.value("incompatibleCount").toInt(),1);
    const auto alternatives=review.value("alternatives").toList();QVERIFY(!alternatives.isEmpty());QVariantMap replacement;
    for(const auto &value:alternatives){const auto item=value.toMap();if(item.value("mediaId")!=slot.value("mediaId")){replacement=item;break;}}
    QVERIFY(!replacement.isEmpty());const auto state=store.channel("cartoons");const auto savedCandidates=store.candidateSnapshot("cartoons");
    QVERIFY(!store.repairSlot("cartoons",id,"movie",99999,1));QVERIFY(!store.repairSlot("cartoons",id,"movie",8,1));
    store.failNextCommitForTest();QVERIFY(!store.repairSlot("cartoons",id,replacement.value("mediaType").toString(),replacement.value("mediaId").toInt(),1));
    QCOMPARE(store.channel("cartoons"),state);QCOMPARE(store.programs("cartoons",Now,Now+Day,1000),schedule);
    QVERIFY(store.repairSlot("cartoons",id,replacement.value("mediaType").toString(),replacement.value("mediaId").toInt(),1));
    auto expected=schedule;auto changed=slot;for(const auto &key:QStringList{"mediaType","mediaId","title"})changed[key]=replacement.value(key);changed["status"]="ready";expected[index]=changed;
    QCOMPARE(store.programs("cartoons",Now,Now+Day,1000),expected);QCOMPARE(store.candidateSnapshot("cartoons"),savedCandidates);
    QVERIFY(!store.repairSlot("cartoons",id,QString(),0,1));QVERIFY(store.repairPreview("cartoons",id,1,Now).isEmpty());
    QVERIFY(store.repairSlot("cartoons",id,QString(),0,2));changed["status"]="unavailable";expected[index]=changed;QCOMPARE(store.programs("cartoons",Now,Now+Day,1000),expected);
    auto finalState=store.channel("cartoons");finalState["revision"]=state.value("revision");QCOMPARE(finalState,state);
}
void GuideTests::virtualClockPausesWhileProfileIsOff() {
    QTemporaryDir dir; const auto path=dir.filePath("guide.sqlite"); auto def=definition(); def["clock"]="pause-when-off";
    QVariantMap paused;
    {
        GuideStore store(path); QVERIFY(store.createLocal(def,media(),Now));
        paused=store.tuneTarget("cartoons",Now+800);
        QVERIFY(store.setActive(false,Now+800));
        paused["displayEndUtc"]=paused.value("endUtc").toLongLong()+12*3600-800; // wall projection shifts while broadcast clock is paused
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
void GuideTests::corruptKnownDatabaseIsNeverRewritten() {
    QTemporaryDir dir;const auto path=dir.filePath("corrupt.sqlite");
    {GuideStore store(path);QVERIFY(store.createLocal(definition(),media(),Now,1));}
    {
        auto db=QSqlDatabase::addDatabase("QSQLITE","corrupt-guide");db.setDatabaseName(path);QVERIFY(db.open());
        {QSqlQuery q(db);QVERIFY(q.exec("UPDATE channels SET definition='{}'"));QVERIFY(q.exec("PRAGMA wal_checkpoint(TRUNCATE)"));}
        db.close();
    }
    QSqlDatabase::removeDatabase("corrupt-guide");const auto before=bytes(path);QVERIFY(!before.isEmpty());
    {GuideStore store(path);QVERIFY(!store.ready());QVERIFY(store.errorString().contains("preserved"));}
    QCOMPARE(bytes(path),before);
}
void GuideTests::cancelledTransactionsAndInterruptedVirtualClock() {
    QTemporaryDir dir;const auto path=dir.filePath("guide.sqlite");auto def=definition();def["clock"]="pause-when-off";
    {
        GuideStore store(path);store.setCancellationCheck([]{return true;});
        QVERIFY(!store.createLocal(def,media(),Now));QVERIFY(store.channels().isEmpty());
        store.setCancellationCheck({});QVERIFY(store.createLocal(def,media(),Now));
        QVERIFY(store.setActive(true,Now+800)); // durable checkpoint; simulate crash without shutdown checkpoint
        QVERIFY(store.setMetadata("cartoons",{{"name","Renamed"},{"number",202}},1));
        QCOMPARE(store.channel("cartoons").value("name").toString(),QString("Renamed"));
        QVERIFY(!store.setMetadata("cartoons",{{"name","Stale"}},1));
        QVERIFY(!store.setMetadata("cartoons",{{"number",true}},2));
    }
    GuideStore reopened(path);QVERIFY(reopened.resumeClocks(Now+12*3600));
    const auto target=reopened.tuneTarget("cartoons",Now+12*3600);
    QCOMPARE(target.value("clockUtc").toLongLong(),Now+800);
    QCOMPARE(reopened.channel("cartoons").value("number").toInt(),202);
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
void GuideTests::rollingMaintenancePreservesSlotsAndCatchesUpAfterLongOutage() {
    QTemporaryDir dir;GuideStore store(dir.filePath("guide.sqlite"));
    QVERIFY(store.createLocal(definition(),media(),Now));
    const auto committed=store.programs("cartoons",Now+5*Day,Now+7*Day,1000);
    QVERIFY(store.maintain("cartoons",Now+5*Day,1));
    QCOMPARE(store.programs("cartoons",Now+5*Day,Now+7*Day,1000),committed);
    QVERIFY(!store.tuneTarget("cartoons",Now+11*Day).isEmpty());
    QVERIFY(store.programs("cartoons",Now,Now+2*Day).isEmpty());
    const auto future=store.programs("cartoons",Now+5*Day,Now+12*Day,1000);
    store.failNextCommitForTest();QVERIFY(!store.maintain("cartoons",Now+6*Day,1));
    QCOMPARE(store.programs("cartoons",Now+5*Day,Now+12*Day,1000),future);
    QVERIFY(!store.maintain("cartoons",Now+6*Day,2));
    // A year offline must not materialize a year's missing historical slots.
    QVERIFY2(store.maintain("cartoons",Now+400*Day,1),qPrintable(store.errorString()));
    QVERIFY(!store.tuneTarget("cartoons",Now+400*Day+723).isEmpty());
    QVERIFY(!store.tuneTarget("cartoons",Now+406*Day).isEmpty());
    const auto first=store.programs("cartoons",Now+398*Day,Now+407*Day,1000);
    QVERIFY(!first.isEmpty());QVERIFY(first.size()<1000);
    QVERIFY(store.maintain("cartoons",Now+400*Day,1));
    QCOMPARE(store.programs("cartoons",Now+398*Day,Now+407*Day,1000),first);
    auto paused=definition("paused",102);paused["clock"]="pause-when-off";
    QVERIFY(store.createLocal(paused,media(),Now,1));QVERIFY(store.setActive(false,Now+700));
    QVERIFY(store.maintain("paused",Now+400*Day,1));
    QCOMPARE(store.tuneTarget("paused",Now+400*Day).value("clockUtc").toLongLong(),Now+700);
}
void GuideTests::pvrGenerationIsAtomicAndPreservesUserOverrides() {
    QTemporaryDir dir;GuideStore store(dir.filePath("guide.sqlite"));
    const auto entry=[](int kodi,int unique,int number){
        const auto id="pvr-"+QString::fromLatin1(QCryptographicHash::hash(QByteArray("kodi-pvr:9:")+QByteArray::number(unique),QCryptographicHash::Sha256).toHex());
        const QVariantMap def{{"schemaVersion",1},{"id",id},{"name",QString("PVR %1").arg(unique)},{"number",number},{"owner","kodi-pvr"},
            {"source",QVariantMap{{"instance","kodi-pvr"},{"channelId",kodi},{"clientId",9},{"uniqueId",unique},{"number",number}}},{"sourceAvailable",true},{"refreshedUtc",Now}};
        return QVariantMap{{"definition",def},{"programs",QVariantList{QVariantMap{{"broadcastId","a"},{"title","Source program"},{"startUtc",Now-600},{"endUtc",Now+1200}}}}};
    };
    auto first=entry(11,101,1),second=entry(12,102,2);second["programs"]=QVariantList();
    const auto firstId=first.value("definition").toMap().value("id").toString(),secondId=second.value("definition").toMap().value("id").toString();
    store.failNextCommitForTest();QVERIFY(!store.importPvr("kodi-pvr",{first,second},Now,{}));QVERIFY(store.channels().isEmpty());
    QVERIFY2(store.importPvr("kodi-pvr",{first,second},Now,{}),qPrintable(store.errorString()));
    QCOMPARE(store.tuneTarget(firstId,Now).value("launchKind").toString(),QString("kodi_pvr"));
    QVERIFY(!store.tuneTarget(secondId,Now).isEmpty());QVERIFY(store.programs(secondId,Now,Now+1).isEmpty()); // no EPG still tunable
    const auto baseline=store.programs(firstId,Now-600,Now+1200);
    QVERIFY(store.setMetadata(firstId,{{"name","My station"},{"number",42},{"groups",QVariantList{QString("Favorites")}}, {"description","My station notes"}},1));QVERIFY(store.setPreferences(firstId,{{"favorite",true}},2));
    auto invalid=first;auto bad=invalid.value("programs").toList();bad.append(bad.first());invalid["programs"]=bad;
    const QVariantMap revisions{{firstId,3},{secondId,1}};
    QVERIFY(!store.importPvr("kodi-pvr",{invalid,second},Now,revisions));QCOMPARE(store.programs(firstId,Now-600,Now+1200),baseline);
    auto changed=first.value("definition").toMap();auto source=changed.value("source").toMap();source["channelId"]=99;changed["source"]=source;first["definition"]=changed;
    auto recreatedPrograms=first.value("programs").toList();auto recreatedProgram=recreatedPrograms[0].toMap();recreatedProgram["broadcastId"]="new-kodi-database-id";recreatedPrograms[0]=recreatedProgram;first["programs"]=recreatedPrograms;
    QVERIFY(store.importPvr("kodi-pvr",{first},Now,revisions));
    const auto updated=store.channel(firstId);QCOMPARE(updated.value("name").toString(),QString("My station"));QCOMPARE(updated.value("number").toInt(),42);QVERIFY(updated.value("favorite").toBool());
    QCOMPARE(updated.value("groups").toList(),QVariantList{QString("Favorites")});QCOMPARE(updated.value("description").toString(),QString("My station notes"));
    QCOMPARE(store.tuneTarget(firstId,Now).value("mediaId").toInt(),99);auto remapped=baseline;for(auto &v:remapped){auto p=v.toMap();p["mediaId"]=99;v=p;}QCOMPARE(store.programs(firstId,Now-600,Now+1200),remapped); // stable source broadcast IDs/times
    QVERIFY(!store.channel(secondId).value("sourceAvailable").toBool());QVERIFY(store.tuneTarget(secondId,Now).isEmpty());
    QVERIFY(!store.importPvr("kodi-pvr",{first,second},Now,revisions)); // stale preview
    const QVariantMap current{{firstId,4},{secondId,2}};
    QVERIFY(store.importPvr("kodi-pvr",{first,second},Now,current));QVERIFY(!store.tuneTarget(secondId,Now).isEmpty());
    QVERIFY(!store.maintain(firstId,Now,5)); // source is its only scheduler
}
void GuideTests::regenerationRequiresCurrentReviewAndPreservesAiringProgram() {
    QTemporaryDir dir;GuideStore store(dir.filePath("guide.sqlite"));QVERIFY(store.createLocal(definition(),media(),Now,1));
    const auto original=store.programs("cartoons",Now,Now+Day,1000);const auto airing=original.first().toMap();
    auto def=definition();def["rule"]=QVariantMap{{"field","mediaType"},{"op","is"},{"value","movie"}};QVERIFY(store.updateRules(def,media(),1));
    const auto review=store.regenerationPreview("cartoons",Now+100);QVERIFY(!review.isEmpty());
    QCOMPARE(review.value("fromUtc"),airing.value("endUtc"));
    store.failNextCommitForTest();QVERIFY(!store.regenerate("cartoons",review,Now+101));QCOMPARE(store.programs("cartoons",Now,Now+Day,1000),original);
    QVERIFY(!store.regenerate("cartoons",review,airing.value("endUtc").toLongLong()+1));QCOMPARE(store.programs("cartoons",Now,Now+Day,1000),original);
    QVERIFY2(store.regenerate("cartoons",review,Now+102),qPrintable(store.errorString()));
    const auto replaced=store.programs("cartoons",Now,Now+Day,1000);QCOMPARE(replaced.first().toMap(),airing);
    for(int i=1;i<replaced.size();++i){QCOMPARE(replaced[i].toMap().value("mediaType").toString(),QString("movie"));QCOMPARE(replaced[i-1].toMap().value("endUtc"),replaced[i].toMap().value("startUtc"));}
    QVERIFY(replaced[1].toMap().value("id")!=original[1].toMap().value("id"));
    QVERIFY(!store.regenerate("cartoons",review,Now+103)); // accepted review cannot be reused
    QCOMPARE(store.candidateSnapshot("cartoons").size(),4);QCOMPARE(store.definition("cartoons"),def);
}
QTEST_GUILESS_MAIN(GuideTests)
#include "tst_guide.moc"
