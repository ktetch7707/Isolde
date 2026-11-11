#include "InterfaceSounds.h"
#include "input/InputManager.h"
#include <QSoundEffect>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QXmlStreamReader>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QDataStream>
#include <QtEndian>
#include <QSet>
#include <QDirIterator>
#include <QtAlgorithms>
#include <cmath>

namespace {
constexpr qsizetype MaxXml=64*1024, MaxWave=512*1024;
const QString Soft="builtin.soft";
const QSet<QString> Events{"up","down","left","right","select","back","menu","search","home","error","notification"};
const QSet<QString> KodiActions{"up","down","left","right","select","previousmenu","parentdir","contextmenu","search","home","error","notification"};
bool unlinked(QString path) {
    path=QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    for (;;) {
        const QFileInfo info(path);
        if (info.isSymLink()) return false;
        const auto parent=info.absolutePath();
        if (parent==path) return true;
        path=parent;
    }
}
QByteArray boundedFile(const QString &path, qsizetype limit) {
    const QFileInfo info(path);
    if (!unlinked(path) || !info.isFile() || info.size()<1 || info.size()>limit) return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const auto bytes=file.read(limit+1);
    return bytes.size()==info.size() && bytes.size()<=limit ? bytes : QByteArray{};
}
QByteArray xmlFile(const QString &path) {
    const auto bytes=boundedFile(path,MaxXml);
    return bytes.contains("<!DOCTYPE") || bytes.contains("<!ENTITY") ? QByteArray{} : bytes;
}
QString inside(const QString &root, const QString &relative) {
    if (relative.isEmpty() || relative.size()>240 || relative.contains('\\') || relative.contains(QChar::Null)
        || QDir::isAbsolutePath(relative) || relative.split('/').contains("..") || relative.contains(':')) return {};
    const auto path=QDir::cleanPath(QDir(root).filePath(relative));
    if (!path.startsWith(QDir::cleanPath(root)+'/') || !unlinked(path)) return {};
    return path;
}
bool number(const QVariant &value, int low, int high) {
    const auto json=QJsonValue::fromVariant(value);
    const double n=json.toDouble(-1);
    return json.isDouble() && std::isfinite(n) && std::floor(n)==n && n>=low && n<=high;
}
bool label(const QVariant &value, int limit) {
    return value.metaType().id()==QMetaType::QString && !value.toString().trimmed().isEmpty()
        && value.toString().size()<=limit && !value.toString().contains(QChar::Null);
}
QByteArray tone(int frequency, int ms) {
    constexpr int rate=48000;
    const int count=rate*ms/1000;
    QByteArray bytes; QDataStream out(&bytes,QIODevice::WriteOnly);out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF",4);out<<quint32(36+count*2);out.writeRawData("WAVEfmt ",8);
    out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(rate)<<quint32(rate*2)<<quint16(2)<<quint16(16);
    out.writeRawData("data",4);out<<quint32(count*2);
    for(int i=0;i<count;++i) {
        const double envelope=std::sin(3.141592653589793*i/count);
        out<<qint16(32767*.12*envelope*envelope*std::sin(2*3.141592653589793*frequency*i/rate));
    }
    return bytes;
}
int durationMs(const QByteArray &bytes) {
    quint32 rate=0,size=0;
    for(qsizetype p=12;p+8<=bytes.size();) {
        const auto length=qFromLittleEndian<quint32>(bytes.constData()+p+4);
        if(bytes.mid(p,4)=="fmt ")rate=qFromLittleEndian<quint32>(bytes.constData()+p+16);
        else if(bytes.mid(p,4)=="data")size=length;
        p+=8+length+(length&1);
    }
    return rate?int(std::ceil(1000.0*size/rate)):0;
}
}

bool InterfaceSounds::validOptions(const QVariantMap &options) {
    if (options.size()!=5 || !number(options.value("schemaVersion"),1,1)) return false;
    const auto defaults=options.value("defaults").toMap(), tones=options.value("tones").toMap();
    if (defaults.size()!=4 || defaults.value("enabled").metaType().id()!=QMetaType::Bool
        || defaults.value("duringPlayback").metaType().id()!=QMetaType::Bool || !number(defaults.value("volume"),0,100)
        || defaults.value("theme")!="resource.uisounds.kodi" || tones.size()!=4) return false;
    for (const auto &name:QStringList{"move","select","back","error"}) {
        const auto spec=tones.value(name).toMap();
        if(spec.size()!=2 || !number(spec.value("frequency"),100,2000) || !number(spec.value("durationMs"),10,150))return false;
    }
    if (options.value("events").metaType().id()!=QMetaType::QVariantList || options.value("events").toList().size()!=Events.size()) return false;
    QSet<QString> ids;
    for(const auto &value:options.value("events").toList()) {
        const auto entry=value.toMap();const auto id=entry.value("id").toString();
        if(entry.size()!=4 || !Events.contains(id) || ids.contains(id) || !label(entry.value("label"),80)
            || !tones.contains(entry.value("tone").toString()) || entry.value("kodi").metaType().id()!=QMetaType::QVariantList) return false;
        ids.insert(id);QSet<QString> aliases;const auto list=entry.value("kodi").toList();
        if(list.isEmpty() || list.size()>2)return false;
        for(const auto &alias:list) {if(alias.metaType().id()!=QMetaType::QString || !KodiActions.contains(alias.toString()) || aliases.contains(alias.toString()))return false;aliases.insert(alias.toString());}
    }
    const QSet<QString> fields{"enabled","theme","volume","duringPlayback","preview","refresh"};QSet<QString> seen;
    if(options.value("fields").metaType().id()!=QMetaType::QVariantList || options.value("fields").toList().size()!=fields.size())return false;
    for(const auto &value:options.value("fields").toList()) {
        const auto field=value.toMap();const auto key=field.value("key").toString();
        if(field.size()!=3 || !fields.contains(key) || seen.contains(key) || !label(field.value("label"),80) || !label(field.value("help"),240))return false;
        seen.insert(key);
    }
    return true;
}

bool InterfaceSounds::validWave(const QByteArray &bytes) {
    if(bytes.size()<44 || bytes.size()>MaxWave || bytes.left(4)!="RIFF" || bytes.mid(8,4)!="WAVE")return false;
    const auto u16=[&](qsizetype p){return qFromLittleEndian<quint16>(bytes.constData()+p);};
    const auto u32=[&](qsizetype p){return qFromLittleEndian<quint32>(bytes.constData()+p);};
    if(quint64(u32(4))+8!=quint64(bytes.size()))return false;
    bool format=false,data=false;quint32 rate=0,dataSize=0;quint16 frame=0;
    for(qsizetype p=12;p+8<=bytes.size();) {
        const auto size=u32(p+4);const auto start=p+8;
        if(quint64(start)+size>quint64(bytes.size()))return false;
        if(bytes.mid(p,4)=="fmt ") {
            if(format || size<16 || u16(start)!=1)return false;
            const auto channels=u16(start+2),bits=u16(start+14);rate=u32(start+4);frame=u16(start+12);
            if((channels!=1 && channels!=2) || !QList<int>{8,16,24,32}.contains(bits) || rate<8000 || rate>96000
                || frame!=channels*bits/8 || u32(start+8)!=rate*frame)return false;
            format=true;
        } else if(bytes.mid(p,4)=="data") {if(data || !size)return false;data=true;dataSize=size;}
        p=start+size+(size&1);
        if(p>bytes.size())return false;
        if(p==bytes.size())return format && data && dataSize%frame==0 && quint64(dataSize)<=quint64(rate)*frame*2;
    }
    return false;
}

QVariantMap InterfaceSounds::readKodiTheme(const QString &directory, QString *error, bool includeAudio) {
    const auto fail=[&](const QString &message)->QVariantMap {if(error)*error=message;return {};};
    const auto root=QDir::cleanPath(QFileInfo(directory).absoluteFilePath());
    if(!QFileInfo(root).isDir() || !unlinked(root))return fail("The sound theme directory is unavailable or redirected.");
    const auto addon=xmlFile(root+"/addon.xml");if(addon.isEmpty())return fail("The sound theme manifest is invalid or too large.");
    QXmlStreamReader manifest(addon);
    if(!manifest.readNextStartElement() || manifest.name()!=QLatin1String("addon"))return fail("Expected a Kodi sound resource add-on.");
    const auto id=manifest.attributes().value("id").toString(),name=manifest.attributes().value("name").toString();
    if(!QRegularExpression("^resource\\.uisounds\\.[a-z0-9][a-z0-9.-]{0,79}\\z").match(id).hasMatch()
        || QFileInfo(root).fileName()!=id || !label(name,80))return fail("Invalid Kodi sound theme identity.");
    bool soundResource=false;
    while(manifest.readNextStartElement()) {
        if(manifest.name()==QLatin1String("extension")) {
            const auto point=manifest.attributes().value("point");
            if(point==QLatin1String("kodi.resource.uisounds"))soundResource=true;
            else if(point!=QLatin1String("kodi.addon.metadata") && point!=QLatin1String("xbmc.addon.metadata"))return fail("Only declarative Kodi sound resource packs are supported.");
        }
        manifest.skipCurrentElement();
    }
    while(!manifest.atEnd())manifest.readNext();
    if(manifest.hasError() || !soundResource)return fail("The add-on is not a valid Kodi UI sound resource.");
    const auto mapping=xmlFile(root+"/resources/sounds.xml");if(mapping.isEmpty())return fail("The sound mapping is invalid or too large.");
    QXmlStreamReader xml(mapping);
    if(!xml.readNextStartElement() || xml.name()!=QLatin1String("sounds"))return fail("Expected Kodi sounds.xml.");
    QMap<QString,QString> files;int count=0;
    while(xml.readNextStartElement()) {
        const auto group=xml.name().toString();
        if(group!="actions" && group!="windows")return fail("Unknown sound mapping group.");
        while(xml.readNextStartElement()) {
            if(++count>128 || (group=="actions" ? xml.name()!=QLatin1String("action") : xml.name()!=QLatin1String("window")))return fail("Invalid or excessive sound mappings.");
            QMap<QString,QString> fields;
            while(xml.readNextStartElement()) {
                const auto key=xml.name().toString();
                if(fields.contains(key) || !QStringList{"name","file","activate","deactivate"}.contains(key))return fail("Invalid sound mapping fields.");
                fields[key]=xml.readElementText();
            }
            auto action=fields.value("name"),file=fields.value("file");
            if(group=="windows") {if(action!="notification")continue;file=fields.value("activate");}
            if(!KodiActions.contains(action) || file.isEmpty())continue;
            if(files.contains(action))return fail("Duplicate sound action.");
            files[action]=file;
        }
    }
    while(!xml.atEnd())xml.readNext();
    if(xml.hasError() || files.isEmpty())return fail("The sound pack has no supported action mappings.");
    QVariantMap sounds;
    for(auto it=files.cbegin();it!=files.cend();++it) {
        const auto path=inside(root+"/resources",it.value());
        if(!path.endsWith(".wav",Qt::CaseInsensitive))return fail("Only contained WAV sound files are supported.");
        const QFileInfo info(path);
        if(!info.isFile() || info.size()<44 || info.size()>MaxWave)return fail("A mapped sound file is missing or too large.");
        // Discovery reads bounded metadata only. Full decoding validation and
        // private caching happen before selecting or previewing a theme.
        if(!includeAudio)continue;
        const auto wave=boundedFile(path,MaxWave);
        if(!validWave(wave))return fail("A theme sound is missing or is not a bounded PCM WAV (maximum two seconds).");
        sounds[it.key()]=wave;
    }
    if(error)error->clear();
    return {{"id",id},{"name",name},{"sounds",sounds}};
}

InterfaceSounds::InterfaceSounds(const QString &settingsPath,const QStringList &addonRoots,QObject *parent,const QString &optionsPath,bool audioBackend)
    :QObject(parent),m_path(settingsPath),m_roots(addonRoots),m_audioBackend(audioBackend) {
    QFile options(optionsPath);
    if(options.open(QIODevice::ReadOnly) && options.size()<=MaxXml)m_options=QJsonDocument::fromJson(options.readAll()).object().toVariantMap();
    if(!validOptions(m_options)) {m_options.clear();m_enabled=false;m_error="Interface sound defaults are invalid.";return;}
    const auto defaults=m_options.value("defaults").toMap();m_enabled=defaults.value("enabled").toBool();m_volume=defaults.value("volume").toInt();
    m_duringPlayback=defaults.value("duringPlayback").toBool();m_theme=defaults.value("theme").toString();
    if(m_audioBackend) {
        auto *devices=new QMediaDevices(this);
        connect(devices,&QMediaDevices::audioOutputsChanged,this,[this] {
            stop();
            const auto output=QMediaDevices::defaultAudioOutput();
            for(auto *effect:m_effects)effect->setAudioDevice(output);
        });
    }
    if(QFileInfo::exists(m_path) || QFileInfo(m_path).isSymLink()) {
        const auto bytes=boundedFile(m_path,4096);const auto saved=QJsonDocument::fromJson(bytes).object().toVariantMap();
        const auto id=saved.value("theme").toString();
        if(saved.size()==5 && number(saved.value("schemaVersion"),1,1) && number(saved.value("volume"),0,100)
            && saved.value("enabled").metaType().id()==QMetaType::Bool && saved.value("duringPlayback").metaType().id()==QMetaType::Bool
            && (id==Soft || QRegularExpression("^resource\\.uisounds\\.[a-z0-9][a-z0-9.-]{0,79}\\z").match(id).hasMatch())) {
            m_enabled=saved.value("enabled").toBool();m_volume=saved.value("volume").toInt();m_duringPlayback=saved.value("duringPlayback").toBool();m_theme=id;
        } else m_error="Saved interface sound settings are invalid. Defaults are in use; the file was not changed.";
    }
    m_previewTimer.setInterval(260);
    connect(&m_previewTimer,&QTimer::timeout,this,[this] {
        const QStringList sequence{"down","select","back","error"};
        if(m_previewIndex>=sequence.size() || quiet()) {stop();return;}
        const auto event=sequence[m_previewIndex++];request(event,true);
        m_previewTimer.setInterval(qMax(120,m_durations.value(event)+80));
    });
    const auto loadError=m_error;refreshThemes();if(!loadError.isEmpty())m_error=loadError;
}
InterfaceSounds::~InterfaceSounds() { m_previewTimer.stop();silence();qDeleteAll(m_effects); }

QString InterfaceSounds::themeName() const {
    for(const auto &value:m_themes)if(value.toMap().value("id")==m_theme)
        return value.toMap().value("name").toString()+(m_failedTheme==m_theme?tr(" · using Soft"):QString{});
    return tr("Unavailable theme · using Soft");
}
QString InterfaceSounds::effectiveTheme() const {return m_themePaths.contains(m_theme) && m_failedTheme!=m_theme?m_theme:Soft;}

bool InterfaceSounds::quiet() const {return !m_enabled || m_volume==0 || m_muted || m_suppressed || (m_playbackActive && !m_duringPlayback);}
void InterfaceSounds::silence() {
    m_pendingEvent.clear();if(m_playing)m_playing->stop();m_playing=nullptr;
}
void InterfaceSounds::stop() {
    const bool preview=m_previewTimer.isActive();m_previewTimer.stop();silence();
    if(preview)prepare(effectiveTheme());
    emit changed();
}
void InterfaceSounds::setSuppressed(bool value) {if(m_suppressed==value)return;m_suppressed=value;if(quiet())stop();emit changed();}
void InterfaceSounds::setPlaybackActive(bool value) {if(m_playbackActive==value)return;m_playbackActive=value;if(quiet())stop();emit changed();}
void InterfaceSounds::setMuted(bool value) {if(m_muted==value)return;m_muted=value;if(quiet())stop();emit changed();}

bool InterfaceSounds::save(bool enabled,int volume,bool duringPlayback,const QString &theme) {
    if(m_options.isEmpty() || volume<0 || volume>100) {m_error="Choose a valid interface sound level from 0 to 100%.";emit changed();return false;}
    if(!unlinked(m_path) || !QDir().mkpath(QFileInfo(m_path).absolutePath()) || !unlinked(m_path)) {m_error="Cannot safely save interface sound settings.";emit changed();return false;}
    const auto bytes=QJsonDocument(QJsonObject{{"schemaVersion",1},{"enabled",enabled},{"volume",volume},{"duringPlayback",duringPlayback},{"theme",theme}}).toJson(QJsonDocument::Compact);
    QSaveFile file(m_path);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) {m_error="Interface sound settings could not be saved. Your previous settings are unchanged.";emit changed();return false;}
    m_enabled=enabled;m_volume=volume;m_duringPlayback=duringPlayback;m_theme=theme;m_error.clear();
    stop();emit changed();return true;
}
bool InterfaceSounds::setEnabled(bool value) {return save(value,m_volume,m_duringPlayback,m_theme);}
bool InterfaceSounds::setVolume(int value) {return save(m_enabled,value,m_duringPlayback,m_theme);}
bool InterfaceSounds::setDuringPlayback(bool value) {return save(m_enabled,m_volume,value,m_theme);}

void InterfaceSounds::refreshThemes() {
    stop();m_themePaths.clear();m_themes={QVariantMap{{"id",Soft},{"name",tr("Soft (built-in)")}}};
    QString warning;int checked=0;
    for(const auto &root:m_roots.mid(0,4)) {
        if(!unlinked(root) || !QFileInfo(root).isDir())continue;
        QDirIterator entries(root,{"resource.uisounds.*"},QDir::Dirs|QDir::NoDotAndDotDot);
        while(entries.hasNext()) {
            entries.next();const auto entry=entries.fileInfo();
            if(++checked>64){warning="Only the first 64 installed sound packs were scanned.";break;}
            QString error;const auto pack=readKodiTheme(entry.absoluteFilePath(),&error,false);
            if(pack.isEmpty()) {if(warning.isEmpty())warning=entry.fileName()+": "+error;continue;}
            const auto id=pack.value("id").toString();if(m_themePaths.contains(id))continue;
            m_themePaths[id]=entry.absoluteFilePath();m_themes.append(QVariantMap{{"id",id},{"name",pack.value("name")}});
        }
        if(checked>64)break;
    }
    m_prepared.clear();m_failedTheme.clear();m_error=warning;
    if(!prepare(effectiveTheme())) {
        const auto error=m_error;m_failedTheme=m_theme;prepare(Soft);
        m_error=error.isEmpty()?tr("The interface sound theme could not be loaded. Soft is in use."):error;
    }
    emit changed();
}

bool InterfaceSounds::prepare(const QString &id) {
    if(id==m_prepared)return true;
    if(!m_cache.isValid() || m_options.isEmpty()) {m_error="Interface sound cache is unavailable.";emit changed();return false;}
    QSet<QString> retained;
    for(const auto &url:m_sources)retained.insert(url.toLocalFile());
    for(auto it=m_cacheFiles.begin();it!=m_cacheFiles.end();) {
        if(!retained.contains(*it)) {
            if(!unlinked(*it) || !it->startsWith(m_cache.path()+'/') || !QFile::remove(*it)) {m_error="Cannot safely refresh the interface sound cache.";emit changed();return false;}
            it=m_cacheFiles.erase(it);
        } else ++it;
    }
    QVariantMap sounds;
    if(id!=Soft) {
        if(!m_themePaths.contains(id)){m_error="That sound theme is unavailable. Refresh themes and retry.";emit changed();return false;}
        const auto pack=readKodiTheme(m_themePaths.value(id),&m_error);if(pack.isEmpty()){emit changed();return false;}sounds=pack.value("sounds").toMap();
    }
    QMap<QString,QUrl> sources;QMap<QString,int> durations;
    for(const auto &value:m_options.value("events").toList()) {
        const auto event=value.toMap();const auto key=event.value("id").toString();QByteArray bytes;
        if(id==Soft) {const auto spec=m_options.value("tones").toMap().value(event.value("tone").toString()).toMap();bytes=tone(spec.value("frequency").toInt(),spec.value("durationMs").toInt());}
        else for(const auto &alias:event.value("kodi").toList())if(sounds.contains(alias.toString())){bytes=sounds.value(alias.toString()).toByteArray();break;}
        if(bytes.isEmpty())continue;
        durations[key]=durationMs(bytes);
        const auto path=m_cache.filePath(QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())+".wav");
        if(!QFileInfo::exists(path)) {QSaveFile file(path);file.setDirectWriteFallback(false);if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()){m_error="Cannot prepare the sound theme. Retry after checking storage.";emit changed();return false;}m_cacheFiles.insert(path);}
        sources[key]=QUrl::fromLocalFile(path);
    }
    silence();qDeleteAll(m_effects);m_effects.clear();m_sources=sources;m_durations=durations;m_prepared=id;
    if(m_audioBackend)for(auto it=sources.cbegin();it!=sources.cend();++it) {
        auto *effect=new QSoundEffect(QMediaDevices::defaultAudioOutput(),this);const auto key=it.key();m_effects[key]=effect;effect->setLoopCount(1);
        connect(effect,&QSoundEffect::statusChanged,this,[this,effect,key] {
            if(effect->status()==QSoundEffect::Error) {if(m_pendingEvent==key)m_pendingEvent.clear();m_error="Interface sound playback is unavailable. Check Sound Output or choose another theme.";emit changed();}
            else if(effect->status()==QSoundEffect::Ready && m_pendingEvent==key) {
                m_pendingEvent.clear();
                if(!quiet() && m_lastCue.isValid() && m_lastCue.elapsed()<=150) {m_playing=effect;effect->setVolume(m_volume/100.0);effect->play();}
            }
        });
        effect->setSource(it.value());
    }
    return true;
}

bool InterfaceSounds::selectTheme(const QString &id) {
    if(!prepare(id))return false;
    const auto failed=m_failedTheme;m_failedTheme.clear();
    if(!save(m_enabled,m_volume,m_duringPlayback,id)) {const auto error=m_error;m_failedTheme=failed;m_prepared.clear();prepare(effectiveTheme());m_error=error;emit changed();return false;}
    return true;
}
void InterfaceSounds::request(const QString &event,bool preview) {
    if(quiet() || !Events.contains(event) || (!preview && m_previewTimer.isActive()))return;
    if(m_lastCue.isValid() && ((event!="error" && m_lastCue.elapsed()<55) || (event=="error" && m_lastEvent==event && m_lastCue.elapsed()<100)))return;
    if(!m_sources.contains(event))return;
    m_lastCue.start();m_lastEvent=event;silence();
    emit cueRequested(event,m_sources.value(event),m_volume/100.0);
    auto *effect=m_effects.value(event,nullptr);if(!effect)return;
    if(effect->status()==QSoundEffect::Ready){m_playing=effect;effect->setVolume(m_volume/100.0);effect->play();}
    else if(effect->status()==QSoundEffect::Loading)m_pendingEvent=event;
}
void InterfaceSounds::playEvent(const QString &event) {
    if(quiet() || m_previewTimer.isActive())return;
    if(prepare(effectiveTheme()))request(event,false);
}
void InterfaceSounds::playCommand(int command) {
    QString event;
    switch(command) {
    case InputManager::NavigateUp:event="up";break;case InputManager::NavigateDown:event="down";break;
    case InputManager::NavigateLeft:event="left";break;case InputManager::NavigateRight:event="right";break;
    case InputManager::Confirm:event="select";break;case InputManager::Back:event="back";break;
    case InputManager::Menu:event="menu";break;case InputManager::Search:event="search";break;case InputManager::Home:event="home";break;
    default:return;
    }
    playEvent(event);
}
bool InterfaceSounds::preview(const QString &themeId) {
    stop();
    if(quiet()){m_error="Enable interface sounds, raise their level and unmute to preview. Playback and app quiet rules still apply.";emit changed();return false;}
    if(!prepare(themeId))return false;
    m_error.clear();m_lastCue.invalidate();m_previewIndex=1;request("down",true);
    m_previewTimer.start(qMax(120,m_durations.value("down")+80));emit changed();return true;
}
