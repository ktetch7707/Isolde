#include "app/LocalCatalogProvider.h"

#include <QJsonDocument>

LocalCatalogProvider::LocalCatalogProvider(AppCatalog *catalog, QObject *parent)
    : StoreProvider(parent), m_catalog(catalog) {}

QVariantList LocalCatalogProvider::search(const QString &query)
{
    QVariantList result;
    if (!m_catalog)
        return result;
    const auto entries = m_catalog->entries();
    const QString q = query.trimmed().toLower();
    for (const auto &value : entries) {
        const auto map = value.toMap();
        const QString id = map.value(QStringLiteral("id")).toString();
        const QString name = map.value(QStringLiteral("name")).toString();
        const QString category = map.value(QStringLiteral("category")).toString();
        const QString description = map.value(QStringLiteral("description")).toString();
        const QString publisher = map.value(QStringLiteral("publisher")).toString();
        const QString storeType = map.value(QStringLiteral("storeType")).toString();
        const QString type = map.value(QStringLiteral("type")).toString();
        const QString version = map.value(QStringLiteral("version")).toString();
        const QString provenance = map.value(QStringLiteral("provenance")).toString();
        const QString state = map.value(QStringLiteral("state")).toString();

        if (!q.isEmpty()) {
            const auto haystack = QStringLiteral("%1 %2 %3 %4 %5")
                .arg(name, category, description, publisher, type);
            if (!haystack.toLower().contains(q))
                continue;
        }

        QVariantMap entry;
        entry[QStringLiteral("id")] = id;
        entry[QStringLiteral("name")] = name;
        entry[QStringLiteral("publisher")] = publisher;
        entry[QStringLiteral("category")] = category;
        entry[QStringLiteral("iconSource")] = QVariant();
        entry[QStringLiteral("type")] = type;
        entry[QStringLiteral("store")] = providerId();
        entry[QStringLiteral("storeId")] = id;
        entry[QStringLiteral("version")] = version;
        entry[QStringLiteral("sizeBytes")] = QVariant();
        entry[QStringLiteral("provenance")] = provenance;
        entry[QStringLiteral("downloadUrl")] = QVariant();
        entry[QStringLiteral("sha256")] = QVariant();
        entry[QStringLiteral("networkRequired")] = (storeType == QStringLiteral("android") || type == QStringLiteral("web"));
        entry[QStringLiteral("consentText")] = QVariant();
        entry[QStringLiteral("state")] = state;
        entry[QStringLiteral("storeType")] = storeType;
        result.append(entry);
    }
    return result;
}

QUrl LocalCatalogProvider::artifactUrl(const QString &storeId) const
{
    Q_UNUSED(storeId);
    return QUrl();
}

QByteArray LocalCatalogProvider::artifactHash(const QString &storeId) const
{
    Q_UNUSED(storeId);
    return QByteArray();
}

QString LocalCatalogProvider::provenanceText(const QString &storeId) const
{
    Q_UNUSED(storeId);
    return QObject::tr("Isolde curated catalog");
}
