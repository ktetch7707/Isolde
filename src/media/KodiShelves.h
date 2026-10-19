#pragma once
#include "KodiMedia.h"
#include <QVector>

// Derived Home presentation. Never edits the existing Home layout or Kodi.
class KodiShelves final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList sections READ sections NOTIFY changed)
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    Q_PROPERTY(QVariantList components READ sections NOTIFY changed)
public:
    KodiShelves(const QString &profile, KodiMedia *library, QObject *parent = nullptr, const QString &testDirectory = {});
    QVariantList sections() const;
    QVariantList items() const;
    void setAvailable(bool available);
    void refresh();
signals:
    void changed();
private:
    void rebuild();
    QString m_profile, m_testDirectory;
    KodiMedia *m_library;
    bool m_available = false;
    struct Shelf { QVariantMap definition; KodiMedia *client; };
    QVector<Shelf> m_shelves;
};
