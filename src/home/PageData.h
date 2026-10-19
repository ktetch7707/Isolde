#pragma once
#include <QObject>
#include <QHash>
#include <QVariantMap>
class KodiMedia;
class KodiArtwork;
class PageData final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY changed)
public:
    PageData(const QString &profile,KodiMedia *player,QObject *parent=nullptr,const QString &directory={});
    Q_INVOKABLE QObject *client(const QString &key,const QVariantMap &source);
    Q_INVOKABLE QObject *artwork(const QString &kind,int id);
    Q_INVOKABLE void activate(QObject *client,int index,bool resume=true);
    Q_INVOKABLE void refresh(QObject *client);
    int revision() const { return m_revision; }
    void invalidate(const QString &reason);
signals:
    void changed();
private:
    QString m_profile,m_directory;
    KodiMedia *m_player;
    int m_revision=0;
    struct Entry { KodiMedia *client; QVariantMap source; };
    QHash<QString,Entry> m_clients;
    QHash<QString,KodiArtwork *> m_artwork;
};
