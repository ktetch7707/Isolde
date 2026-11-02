#pragma once
#include <QVariantMap>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QDateTime>
#include <QTimeZone>
#include <QCryptographicHash>
#include <QUrl>
#include <QUrlQuery>
#include <QSet>
#include <algorithm>
#include <functional>

// Explicit, bounded feed import. No fetching, scripts, Kodi configuration writes
// or implicit refresh. A failed generation never replaces the previous cache.
namespace GuideFeeds {
inline bool streamUrl(const QString &text) {
    const QUrl url(text,QUrl::StrictMode);
    if(text.isEmpty() || text.size()>4096 || !url.isValid() || url.host().isEmpty()
        || !QStringList{"http","https","rtsp","rtp","udp"}.contains(url.scheme())
        || !url.userInfo().isEmpty() || url.hasFragment() || text.contains('|'))return false;
    for(const auto c:text)if(c.isSpace() || c.unicode()<32 || c.unicode()==127)return false;
    const auto parameters=QUrlQuery(url).queryItems(QUrl::FullyDecoded);if(parameters.size()>256)return false;
    for(const auto &item:parameters) {
        auto key=item.first.toCaseFolded();key.remove('-');key.remove('_');
        if(QStringList{"key","auth","user","username","pass","sig","jwt","ticket"}.contains(key))return false;
        for(const auto &part:QStringList{"password","secret","token","signature","credential","authorization","apikey","session","bearer"})if(key.contains(part))return false;
    }
    return true;
}
inline QString identity(const QString &instance,const QString &station) {
    return "feed-"+QString::fromLatin1(QCryptographicHash::hash(instance.toUtf8()+":"+station.toUtf8(),QCryptographicHash::Sha256).toHex());
}
inline qint64 xmlTime(const QString &value) {
    // XMLTV timestamps must identify an instant; never guess the host timezone.
    static const QRegularExpression format("^([0-9]{14}) ([+-])([0-9]{2})([0-9]{2})$");
    const auto m=format.match(value);if(!m.hasMatch())return -1;
    const auto date=QDateTime::fromString(m.captured(1),"yyyyMMddHHmmss");
    const int hours=m.captured(3).toInt(),minutes=m.captured(4).toInt();
    if(!date.isValid() || date.toString("yyyyMMddHHmmss")!=m.captured(1) || hours>14 || minutes>59 || (hours==14 && minutes))return -1;
    return QDateTime(date.date(),date.time(),QTimeZone::utc()).toSecsSinceEpoch()
        -(m.captured(2)=="-"?-1:1)*(hours*3600+minutes*60);
}
inline QVariantMap parse(const QByteArray &m3u,const QByteArray &xml,const QString &instance,qint64 refreshed,const std::function<bool()> &cancelled={}) {
    const auto fail=[](const QString &message){return QVariantMap{{"ok",false},{"error",message+" Cached listings were preserved."}};};
    if(m3u.size()>4*1024*1024 || xml.size()>16*1024*1024 || m3u.isEmpty())return fail("Choose a playlist up to 4 MiB and XMLTV up to 16 MiB.");
    const auto playlist=QString::fromUtf8(m3u);if(playlist.toUtf8()!=m3u || playlist.contains(QChar::ReplacementCharacter))return fail("Playlist must be UTF-8.");
    const auto lines=playlist.split('\n');bool header=false;QVariantMap pending;QVariantList entries;QSet<QString> ids;
    static const QRegularExpression attribute("([A-Za-z0-9_-]+)=\"([^\"]*)\"");
    for(auto line:lines) {
        if(cancelled && cancelled())return fail("Source preview cancelled.");
        line=line.trimmed();if(line.isEmpty())continue;
        if(!header){if(line!=QStringLiteral("#EXTM3U") && !line.startsWith("#EXTM3U "))return fail("Playlist needs an EXTM3U header.");header=true;continue;}
        if(line.startsWith("#EXTINF:")) {
            if(!pending.isEmpty())return fail("A playlist station has no stream URL.");
            bool quoted=false;int comma=-1;for(int i=8;i<line.size();++i){if(line[i]=='"')quoted=!quoted;if(line[i]==',' && !quoted){comma=i;break;}}
            if(comma<0)return fail("Malformed EXTINF station.");
            const auto name=line.mid(comma+1).trimmed();if(name.isEmpty() || name.size()>160)return fail("Station names must contain 1–160 characters.");
            pending={{"name",name}};auto match=attribute.globalMatch(line.left(comma));
            while(match.hasNext()){const auto a=match.next();if(pending.contains(a.captured(1)))return fail("Duplicate playlist attribute.");pending[a.captured(1)]=a.captured(2);}
            continue;
        }
        if(line.startsWith('#')) {
            if(line.startsWith("#EXTVLCOPT") || line.startsWith("#KODIPROP"))return fail("Stream options and executable addon properties are unsupported.");
            continue;
        }
        if(pending.isEmpty() || !streamUrl(line))return fail("Each station needs a compatible URL without credentials or player options.");
        auto station=pending.value("tvg-id").toString();if(station.isEmpty())station=QString::fromLatin1(QCryptographicHash::hash(line.toUtf8(),QCryptographicHash::Sha256).toHex());
        if(station.size()>160 || ids.contains(station))return fail("Station IDs must be unique and at most 160 characters.");ids.insert(station);
        int number=entries.size()+1;
        if(pending.contains("tvg-shift"))return fail("Playlist time shifts must be resolved into explicit XMLTV UTC offsets before import.");
        if(pending.contains("tvg-chno")){const auto text=pending.value("tvg-chno").toString();bool ok=false;const auto proposed=text.toInt(&ok);if(!QRegularExpression("^[0-9]{1,5}$").match(text).hasMatch() || !ok || proposed<1 || proposed>99999)return fail("Playlist channel numbers must be whole values from 1 to 99999.");number=proposed;}
        QVariantMap source{{"instance",instance},{"stationId",station},{"epgId",pending.value("tvg-id",station)},{"url",line},{"number",number}};
        QVariantMap definition{{"schemaVersion",1},{"id",identity(instance,station)},{"owner","guide-feed"},{"source",source},{"name",pending.value("name")},{"number",number},{"sourceAvailable",true},{"refreshedUtc",refreshed}};
        if(!pending.value("group-title").toString().isEmpty())definition["groups"]=QVariantList{pending.value("group-title")};
        entries.append(QVariantMap{{"definition",definition},{"programs",QVariantList{}}});pending.clear();
        if(entries.size()>1024)return fail("A source supports at most 1,024 stations.");
    }
    if(!pending.isEmpty() || entries.isEmpty())return fail("Incomplete or empty playlist.");
    QVariantMap listings;QSet<QString> declared;int count=0;
    if(!xml.isEmpty()) {
        QXmlStreamReader reader(xml);bool root=false;
        while(!reader.atEnd()) {
            if(cancelled && cancelled())return fail("Source preview cancelled.");
            const auto token=reader.readNext();
            if(token==QXmlStreamReader::DTD || token==QXmlStreamReader::EntityReference)return fail("XMLTV DTDs and entities are refused.");
            if(!reader.isStartElement())continue;
            if(!root){if(reader.name()!=u"tv")return fail("XMLTV needs a tv root.");root=true;continue;}
            if(reader.name()==u"channel") {
                const auto id=reader.attributes().value("id").toString();if(id.isEmpty() || id.size()>160 || declared.contains(id) || declared.size()>=4096)return fail("Invalid or duplicate XMLTV channel ID.");declared.insert(id);
            } else if(reader.name()==u"programme") {
                const auto attributes=reader.attributes();const auto channel=attributes.value("channel").toString();
                const auto start=xmlTime(attributes.value("start").toString()),end=xmlTime(attributes.value("stop").toString());QString title;
                if(++count>100000 || channel.isEmpty() || start<0 || end<=start || end-start>7*86400)return fail("Invalid or oversized XMLTV program intervals.");
                while(reader.readNextStartElement()) {
                    if(reader.name()==u"title"){const auto t=reader.readElementText();if(title.isEmpty())title=t;}else reader.skipCurrentElement();
                }
                if(title.trimmed().isEmpty() || title.size()>512)return fail("XMLTV programs need readable titles up to 512 characters.");
                if(start<refreshed-7*86400 || end>refreshed+31*86400)continue;
                auto programs=listings.value(channel).toList();if(programs.size()>=1000)return fail("Limit XMLTV to 1,000 programs per station.");
                programs.append(QVariantMap{{"broadcastId",QString::number(start)},{"title",title},{"startUtc",start},{"endUtc",end}});listings[channel]=programs;
            }
        }
        if(reader.hasError() || !root)return fail("Malformed or truncated XMLTV.");
    }
    for(const auto &id:declared)if(!listings.contains(id))listings[id]=QVariantList{};
    for(auto &v:entries){auto entry=v.toMap();const auto epg=entry.value("definition").toMap().value("source").toMap().value("epgId").toString();
        auto programs=listings.value(epg).toList();std::sort(programs.begin(),programs.end(),[](const QVariant &a,const QVariant &b){return a.toMap().value("startUtc").toLongLong()<b.toMap().value("startUtc").toLongLong();});
        qint64 end=-1;for(const auto &p:programs){const auto program=p.toMap();if(program.value("startUtc").toLongLong()<end)return fail("XMLTV programs overlap.");end=program.value("endUtc").toLongLong();}
        entry["programs"]=programs;v=entry;
    }
    return {{"ok",true},{"entries",entries},{"listings",listings},{"epgIds",QStringList(declared.values())}};
}
}
