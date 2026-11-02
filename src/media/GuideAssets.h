#pragma once
#include <QBuffer>
#include <QImageReader>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QUrl>

// Same profile artwork namespace and content-addressed convention as HomeLayout.
namespace GuideAssets {
inline QString name(const QByteArray &bytes) {
    if(bytes.isEmpty() || bytes.size()>20*1024*1024)return {};
    QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);
    const auto format=reader.format();const auto size=reader.size();
    if(!QList<QByteArray>{"png","jpeg"}.contains(format) || !size.isValid() || size.width()>8192 || size.height()>8192
        || qint64(size.width())*size.height()>32000000 || !reader.canRead() || reader.read().isNull())return {};
    return "artwork/logo-"+QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())+(format=="png"?".png":".jpg");
}
inline bool safeName(const QString &relative) {
    return QRegularExpression("^artwork/logo-[0-9a-f]{64}\\.(png|jpg)$").match(relative).hasMatch();
}
inline QString path(const QString &profile,const QString &relative) {
    if(!safeName(relative))return {};
    const auto root=QFileInfo(profile).canonicalFilePath(),assets=QDir(profile).filePath("artwork");
    if(root.isEmpty() || QFileInfo(assets).isSymLink())return {};
    const auto target=QDir(profile).filePath(relative);if(QFileInfo(target).isSymLink())return {};
    const auto canonical=QFileInfo(target).canonicalFilePath();
    if(!canonical.isEmpty() && !canonical.startsWith(root+QDir::separator()))return {};
    return target;
}
inline bool write(const QString &profile,const QString &relative,const QByteArray &bytes) {
    if(name(bytes)!=relative || path(profile,relative).isEmpty() || !QDir().mkpath(QDir(profile).filePath("artwork")))return false;
    const auto target=path(profile,relative);if(target.isEmpty())return false;
    QSaveFile file(target);file.setDirectWriteFallback(false);
    return file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size() && file.commit();
}
inline QString source(const QString &profile,const QString &relative) {
    const auto target=path(profile,relative);return !target.isEmpty() && QFileInfo(target).isFile()?QUrl::fromLocalFile(target).toString():QString();
}
}
