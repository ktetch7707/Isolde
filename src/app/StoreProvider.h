#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QByteArray>

class StoreProvider : public QObject {
    Q_OBJECT
public:
    explicit StoreProvider(QObject *parent = nullptr) : QObject(parent) {}
    ~StoreProvider() override = default;

    [[nodiscard]] virtual QString providerId() const = 0;
    [[nodiscard]] virtual QString providerName() const = 0;
    [[nodiscard]] virtual bool isOnline() const = 0;

    // Search returns a list of StoreSearchResult maps:
    //  { id, name, publisher, category, iconSource, type, store, storeId,
    //    version, sizeBytes, provenance, downloadUrl, sha256, networkRequired, consentText }
    // Android installation additionally requires packageName, decimal-string versionCode,
    // metadataVerified (signed repository index validated by this provider), and
    // compatible (ABI/minSdk/capabilities checked against the configured runtime).
    // These flags are provider output, never user-entered claims.
    [[nodiscard]] virtual QVariantList search(const QString &query) = 0;
    virtual void requestSearch(const QString &query) { m_results=search(query); emit resultsChanged(); }
    virtual void cancel() {}
    [[nodiscard]] QVariantList results() const { return m_results; }

    [[nodiscard]] virtual QUrl artifactUrl(const QString &storeId) const = 0;
    [[nodiscard]] virtual QByteArray artifactHash(const QString &storeId) const = 0;
    [[nodiscard]] virtual QString provenanceText(const QString &storeId) const = 0;

signals:
    void resultsChanged();
    void providerError(const QString &message);
protected:
    QVariantList m_results;
};
