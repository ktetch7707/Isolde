#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

class StoreIdentityProvider : public QObject
{
    Q_OBJECT
  public:
    explicit StoreIdentityProvider(QObject* parent = nullptr) : QObject(parent) {}
    ~StoreIdentityProvider() override = default;

    [[nodiscard]] virtual QString
    providerId() const = 0;
    [[nodiscard]] virtual QString
    providerName() const = 0;
    [[nodiscard]] virtual bool
    isOnline() const = 0;
};

class StoreSearchProvider
{
  public:
    virtual ~StoreSearchProvider() = default;

    // Search returns a list of StoreSearchResult maps:
    //  { id, name, publisher, category, iconSource, type, store, storeId,
    //    version, sizeBytes, provenance, downloadUrl, sha256, networkRequired, consentText }
    [[nodiscard]] virtual QVariantList
    search(const QString& query) = 0;
    virtual void
    requestSearch(const QString& query)
    {
        m_results = search(query);
    }
    virtual void
    cancel()
    {
    }
    virtual void
    requestPage(int)
    {
    }
    [[nodiscard]] virtual QVariantList
    results() const
    {
        return m_results;
    }

  protected:
    QVariantList m_results;
};

class StoreUpdateProvider
{
  public:
    virtual ~StoreUpdateProvider() = default;

    virtual void
    requestUpdates(const QStringList&)
    {
    }
    [[nodiscard]] virtual QVariantList
    updateResults() const
    {
        return {};
    }
    [[nodiscard]] virtual QString
    status() const
    {
        return {};
    }
    [[nodiscard]] virtual bool
    hasNextPage() const
    {
        return false;
    }
    [[nodiscard]] virtual bool
    hasPreviousPage() const
    {
        return false;
    }
};

class StoreArtifactProvider
{
  public:
    StoreArtifactProvider() = default;
    virtual ~StoreArtifactProvider() = default;

    [[nodiscard]] virtual QUrl
    artifactUrl(const QString& storeId) const = 0;
    [[nodiscard]] virtual QByteArray
    artifactHash(const QString& storeId) const = 0;
    [[nodiscard]] virtual QString
    provenanceText(const QString& storeId) const = 0;
};

// StoreProvider composes all provider roles into a single store plugin interface.
class StoreProvider : public StoreIdentityProvider,
                      public StoreSearchProvider,
                      public StoreUpdateProvider,
                      public StoreArtifactProvider
{
    Q_OBJECT
  public:
    using StoreIdentityProvider::StoreIdentityProvider;
    using StoreSearchProvider::StoreSearchProvider;
    using StoreUpdateProvider::StoreUpdateProvider;
    ~StoreProvider() override = default;

    // Synchronous providers publish through the same signal as asynchronous providers.
    void requestSearch(const QString& query) override
    {
        StoreSearchProvider::requestSearch(query);
        emit resultsChanged();
    }

  signals:
    void providerError(const QString& message);
    void resultsChanged();
    void updatesChanged();
};
