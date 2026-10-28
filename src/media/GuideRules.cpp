#include "GuideRules.h"
#include "home/ComponentSchema.h"
#include <QJsonDocument>
#include <QSet>
#include <QTimeZone>
#include <cmath>

namespace {
bool keys(const QVariantMap &v, const QSet<QString> &allowed) {
    for (auto it=v.cbegin(); it!=v.cend(); ++it) if (!allowed.contains(it.key())) return false;
    return true;
}
bool integer(const QVariant &v, qint64 low, qint64 high) {
    const auto type=v.metaType().id();
    if (type!=QMetaType::Int && type!=QMetaType::LongLong && type!=QMetaType::UInt
        && type!=QMetaType::ULongLong && type!=QMetaType::Double) return false;
    bool ok=false; const double number=v.toDouble(&ok);
    return ok && std::isfinite(number) && std::floor(number)==number && number>=low && number<=high;
}
bool text(const QVariant &v, int limit, bool empty=false) {
    if (v.metaType().id()!=QMetaType::QString) return false;
    const auto s=v.toString();
    if ((!empty && s.trimmed().isEmpty()) || s.size()>limit) return false;
    for (const auto c:s) if (c.unicode()<32) return false;
    return true;
}
QVariantMap reject(QString *error, const QString &message) {
    if (error) *error=message;
    return {};
}
bool numericField(const QString &field) {
    return QStringList{"year","rating","playcount","duration","season","episode"}.contains(field);
}
bool validRule(const QVariantMap &v, int depth, int &count) {
    if (depth>8 || ++count>128 || v.isEmpty()) return false;
    const auto op=v.value("op").toString();
    if (op=="all" || op=="any" || op=="not") {
        if (!keys(v,{"op","rules"}) || v.value("rules").metaType().id()!=QMetaType::QVariantList) return false;
        const auto children=v.value("rules").toList();
        if (children.isEmpty() || children.size()>32 || (op=="not" && children.size()!=1)) return false;
        for (const auto &child:children)
            if (child.metaType().id()!=QMetaType::QVariantMap || !validRule(child.toMap(),depth+1,count)) return false;
        return true;
    }
    if (!keys(v,{"field","op","value"}) || !text(v.value("field"),32) || !text(v.value("op"),32)) return false;
    const auto field=v.value("field").toString();
    if (!QStringList{"mediaType","title","showtitle","studio","network","genre","tag","year","rating",
                     "playcount","duration","language","actor","director","source","path","season","episode"}.contains(field)) return false;
    if (op=="missing" || op=="present") return !v.contains("value");
    if (!v.contains("value")) return false;
    const auto value=v.value("value");
    const bool numeric=numericField(field);
    const auto scalar=[numeric](const QVariant &item) {
        if (!numeric) return text(item,256);
        const auto type=item.metaType().id();
        if (type==QMetaType::Bool || type==QMetaType::QString) return false;
        bool ok=false; const auto n=item.toDouble(&ok);
        return ok && std::isfinite(n) && n>=0 && n<=10000000;
    };
    if (op=="one-of" || op=="in-range") {
        if (value.metaType().id()!=QMetaType::QVariantList) return false;
        const auto values=value.toList();
        if (values.isEmpty() || values.size()>32 || (op=="in-range" && (!numeric || values.size()!=2))) return false;
        for (const auto &item:values) if (!scalar(item)) return false;
        return op!="in-range" || values[0].toDouble()<=values[1].toDouble();
    }
    if (!scalar(value)) return false;
    if (op=="is" || op=="is-not") return true;
    if (numeric) return op=="before" || op=="after";
    return op=="contains" || op=="does-not-contain" || op=="starts-with";
}
}

QVariantMap ComponentSchema::guideOptions(const QVariantMap &value, QString *error) {
    if(!keys(value,{"rows","minutes","channelWidth","labelSize","gap","radius","sourceBadges","labels","toolbar","sources","surface","overlayWidth","overlayHeight","overlayRows","overlayPosition"})
        || !integer(value.value("rows"),3,12) || !integer(value.value("minutes"),30,240)
        || !integer(value.value("channelWidth"),120,400) || !integer(value.value("labelSize"),14,40)
        || !integer(value.value("gap"),0,32) || !integer(value.value("radius"),0,60)
        || value.value("sourceBadges").metaType().id()!=QMetaType::Bool || value.value("labels").metaType().id()!=QMetaType::Bool
        || value.value("toolbar").metaType().id()!=QMetaType::QVariantList || value.value("sources").metaType().id()!=QMetaType::QVariantList)
        return reject(error,"Invalid guide appearance and controls.");
    QVariantMap presentation;
    const QMap<QString,QString> overlayKeys{{"surface","guideSurface"},{"overlayWidth","guideOverlayWidth"},{"overlayHeight","guideOverlayHeight"},{"overlayRows","guideOverlayRows"},{"overlayPosition","guideOverlayPosition"}};
    for(auto it=overlayKeys.cbegin();it!=overlayKeys.cend();++it)if(value.contains(it.key()))presentation[it.value()]=value.value(it.key());
    if(!validPresentation(presentation,true))return reject(error,"Invalid guide overlay presentation.");
    QSet<QString> seen;
    for(const auto &v:value.value("toolbar").toList()) {
        const auto item=v.toMap();const auto id=item.value("id").toString();
        if(!keys(item,{"id","label"}) || !QStringList{"now","add","manage","sources","browse","close"}.contains(id)
            || seen.contains(id) || !text(item.value("label"),60))return reject(error,"Invalid guide toolbar.");
        seen.insert(id);
    }
    if(!seen.contains("now") || !seen.contains("add") || !seen.contains("manage") || !seen.contains("sources") || !seen.contains("close"))return reject(error,"Keep guide recovery and creation controls reachable.");
    seen.clear();const auto sources=value.value("sources").toList();if(sources.size()>32)return reject(error,"Too many guide service destinations.");
    for(const auto &v:sources) {
        const auto item=v.toMap();const auto id=item.value("id").toString();const auto launch=action(item.value("action").toMap());
        const bool pvr=item.value("kind")=="kodi-pvr";
        if(!keys(item,{"id","name","description","action","kind"}) || !safeId(id) || seen.contains(id)
            || !text(item.value("name"),80) || !text(item.value("description"),256)
            || (pvr?item.contains("action"):(!QStringList{"launch-app","open-url"}.contains(launch.value("type").toString()) || item.contains("kind"))))return reject(error,"Invalid guide service destination.");
        seen.insert(id);
    }
    if(error)error->clear();return value;
}
QVariantMap ComponentSchema::guideRule(const QVariantMap &value, QString *error) {
    int count=0;
    if (QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact).size()>65536 || !validRule(value,0,count))
        return reject(error,"Invalid guide rule. Use known fields/operators, at most 128 rules and eight nested levels.");
    if (error) error->clear();
    return value;
}
QVariantMap ComponentSchema::guideChannel(const QVariantMap &v, QString *error) {
    if(v.contains("description") && !text(v.value("description"),512,true))return reject(error,"Use a plain-text channel description of at most 512 characters.");
    if(v.contains("groups")) {
        if(v.value("groups").metaType().id()!=QMetaType::QVariantList || v.value("groups").toList().size()>32)return reject(error,"Choose at most 32 channel groups.");
        QSet<QString> groups;
        for(const auto &group:v.value("groups").toList()) {
            if(!text(group,80) || group.toString()!=group.toString().trimmed() || group.toString().contains(',') || groups.contains(group.toString().toCaseFolded()))return reject(error,"Use unique group names of at most 80 characters without commas.");
            groups.insert(group.toString().toCaseFolded());
        }
    }
    if(v.value("owner")=="kodi-pvr") {
        const auto source=v.value("source").toMap();
        if(!keys(v,{"schemaVersion","id","name","number","description","groups","owner","source","sourceAvailable","refreshedUtc"})
            || !integer(v.value("schemaVersion"),1,1) || !safeId(v.value("id").toString()) || !text(v.value("name"),160)
            || !integer(v.value("number"),1,99999) || !keys(source,{"instance","channelId","clientId","uniqueId","number"})
            || !safeId(source.value("instance").toString()) || !integer(source.value("channelId"),1,2147483647)
            || !integer(source.value("clientId"),-2147483648LL,2147483647) || !integer(source.value("uniqueId"),-2147483648LL,2147483647)
            || !integer(source.value("number"),0,99999)
            || v.value("sourceAvailable").metaType().id()!=QMetaType::Bool || !integer(v.value("refreshedUtc"),0,253402214400LL))
            return reject(error,"Invalid Kodi PVR channel identity or capabilities.");
        if(error)error->clear();return v;
    }
    if((v.contains("horizonDays") && !integer(v.value("horizonDays"),1,31)) || (v.contains("retentionDays") && !integer(v.value("retentionDays"),0,7)))return reject(error,"Choose a 1–31 day schedule horizon and 0–7 days of retained history.");
    if(v.contains("clockOffsetSeconds") && (!integer(v.value("clockOffsetSeconds"),-31*86400,31*86400)
        || (v.value("clock")!="real-time" && v.value("clockOffsetSeconds").toLongLong()!=0)))return reject(error,"Only real-time local channels support a broadcast offset, bounded to 31 days.");
    if (!keys(v,{"schemaVersion","id","name","number","description","groups","clock","sequence","seed","timezone","rule","mediaKinds","horizonDays","retentionDays","clockOffsetSeconds"})
        || !integer(v.value("schemaVersion"),1,1) || !text(v.value("id"),160) || !safeId(v.value("id").toString())
        || !text(v.value("name"),160) || !integer(v.value("number"),1,99999)
        || !QStringList{"real-time","pause-when-off"}.contains(v.value("clock").toString())
        || !QStringList{"episode-order","seeded-shuffle","rotation","next-unwatched"}.contains(v.value("sequence").toString())
        || !integer(v.value("seed"),0,2147483647) || !text(v.value("timezone"),128)
        || !QTimeZone(v.value("timezone").toString().toUtf8()).isValid()
        || v.value("rule").metaType().id()!=QMetaType::QVariantMap || guideRule(v.value("rule").toMap()).isEmpty())
        return reject(error,"Invalid local channel definition. Check its version, name, number, clock, sequencing, timezone and rules.");
    if(v.contains("mediaKinds")) {
        if(v.value("mediaKinds").metaType().id()!=QMetaType::QVariantList || v.value("mediaKinds").toList().isEmpty() || v.value("mediaKinds").toList().size()>4)return reject(error,"Choose one to four local media types.");
        QSet<QString> kinds;
        for(const auto &kind:v.value("mediaKinds").toList()) {
            if(kind.metaType().id()!=QMetaType::QString || !QStringList{"movie","episode","musicvideo","song"}.contains(kind.toString()) || kinds.contains(kind.toString()))return reject(error,"Invalid or duplicate local media type.");
            kinds.insert(kind.toString());
        }
    }
    if (error) error->clear();
    return v;
}

bool GuideRules::matchesChannel(const QVariantMap &definition, const QVariantMap &metadata) {
    if (definition.contains("mediaKinds") && !definition.value("mediaKinds").toList().contains(metadata.value("mediaType"))) return false;
    // Missing, malformed or unknown watch state is never treated as unwatched.
    if (definition.value("sequence")=="next-unwatched" && !integer(metadata.value("playcount"),0,0)) return false;
    return matches(definition.value("rule").toMap(),metadata);
}

bool GuideRules::matches(const QVariantMap &rule, const QVariantMap &metadata) {
    const auto op=rule.value("op").toString();
    if (op=="all" || op=="any" || op=="not") {
        const auto children=rule.value("rules").toList();
        if (children.isEmpty()) return false;
        if (op=="not") return !matches(children.first().toMap(),metadata);
        for (const auto &child:children) {
            const bool match=matches(child.toMap(),metadata);
            if (op=="all" && !match) return false;
            if (op=="any" && match) return true;
        }
        return op=="all";
    }
    const auto field=rule.value("field").toString();
    const auto actual=metadata.value(field);
    const bool absent=!actual.isValid() || actual.isNull()
        || (actual.toString().isEmpty() && actual.toList().isEmpty());
    if (op=="missing") return absent;
    if (op=="present") return !absent;
    if (absent) return false;
    const auto expected=rule.value("value");
    if (numericField(field)) {
        bool ok=false; const auto n=actual.toDouble(&ok);
        if (!ok || !std::isfinite(n)) return false;
        if (op=="is") return n==expected.toDouble();
        if (op=="is-not") return n!=expected.toDouble();
        if (op=="before") return n<expected.toDouble();
        if (op=="after") return n>expected.toDouble();
        if (op=="in-range") { const auto range=expected.toList(); return n>=range[0].toDouble() && n<=range[1].toDouble(); }
        if (op=="one-of") { for (const auto &v:expected.toList()) if (n==v.toDouble()) return true; }
        return false;
    }
    QStringList values;
    if (actual.metaType().id()==QMetaType::QStringList) values=actual.toStringList();
    else if (actual.metaType().id()==QMetaType::QVariantList) for (const auto &v:actual.toList()) values.append(v.toString());
    else values.append(actual.toString());
    bool found=false;
    for (const auto &value:values) {
        const auto target=expected.toString();
        if (op=="is" || op=="is-not") found |= value.compare(target,Qt::CaseInsensitive)==0;
        else if (op=="contains" || op=="does-not-contain") found |= value.contains(target,Qt::CaseInsensitive);
        else if (op=="starts-with") found |= value.startsWith(target,Qt::CaseInsensitive);
        else if (op=="one-of") for (const auto &v:expected.toList()) found |= value.compare(v.toString(),Qt::CaseInsensitive)==0;
    }
    return op=="is-not" || op=="does-not-contain" ? !found : found;
}
