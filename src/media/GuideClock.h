#pragma once
#include <QElapsedTimer>
#include <QVariantMap>
#include <QDateTime>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QDir>
#include <cmath>
namespace GuideClock {
inline QVariantMap observation() {
    QElapsedTimer timer;timer.start();QString boot;
#ifdef Q_OS_LINUX
    QFile file("/proc/sys/kernel/random/boot_id");if(file.open(QIODevice::ReadOnly))boot=QString::fromLatin1(file.read(80)).trimmed();
#endif
    return {{"schemaVersion",1},{"utc",QDateTime::currentSecsSinceEpoch()},{"monotonic",timer.msecsSinceReference()},{"boot",boot}};
}
inline QString warning(const QString &profile,const QVariantMap &current) {
    const auto path=QDir(profile).filePath("guide-clock.json");if(!QFileInfo::exists(path))return {};
    QFile file(path);if(QFileInfo(path).isSymLink() || !file.open(QIODevice::ReadOnly) || file.size()>1024)return QStringLiteral("The saved clock observation is unreadable. Review the device clock before tuning.");
    const auto previous=QJsonDocument::fromJson(file.read(1025)).toVariant().toMap();
    const auto integer=[](const QVariant &v,double low,double high){const auto type=v.metaType().id();if(type!=QMetaType::Int && type!=QMetaType::LongLong && type!=QMetaType::Double)return false;const auto n=v.toDouble();return std::isfinite(n) && std::floor(n)==n && n>=low && n<=high;};
    if(previous.size()!=4 || !integer(previous.value("schemaVersion"),1,1) || !integer(previous.value("utc"),1,253402214400.0) || !integer(previous.value("monotonic"),0,1e15)
        || previous.value("boot").metaType().id()!=QMetaType::QString || previous.value("boot").toString().size()>80)return QStringLiteral("The saved clock observation is invalid. Review the device clock before tuning.");
    const auto wall=current.value("utc").toLongLong()-previous.value("utc").toLongLong();
    if(!current.value("boot").toString().isEmpty() && current.value("boot")==previous.value("boot")) {
        const auto elapsed=(current.value("monotonic").toLongLong()-previous.value("monotonic").toLongLong())/1000;
        if(elapsed>=0 && std::abs(wall-elapsed)>120)return QStringLiteral("The device wall clock changed by %1 seconds relative to its running clock. Cached UTC programs are unchanged. Review the device clock, then choose whether to use its current time.").arg(wall-elapsed);
    }else if(wall< -120)return QStringLiteral("The device clock is earlier than the last guide checkpoint. Cached programs are unchanged. Review the clock before tuning.");
    return {};
}
inline bool save(const QString &profile,const QVariantMap &value) {
    const auto path=QDir(profile).filePath("guide-clock.json");if(QFileInfo(path).isSymLink())return false;
    const auto bytes=QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact);QSaveFile file(path);file.setDirectWriteFallback(false);
    return file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size() && file.commit();
}
}
