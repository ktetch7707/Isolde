#pragma once

#include "app/AppCatalog.h"
#include "app/StoreProvider.h"

class LocalCatalogProvider : public StoreProvider
{
    Q_OBJECT
  public:
    explicit LocalCatalogProvider(AppCatalog* catalog, QObject* parent = nullptr);

    [[nodiscard]] QVariantList
    search(const QString& query) override;
    [[nodiscard]] QUrl
    artifactUrl(const QString& storeId) const override;
    [[nodiscard]] QByteArray
    artifactHash(const QString& storeId) const override;
    [[nodiscard]] QString
    provenanceText(const QString& storeId) const override;

  protected:
    [[nodiscard]] QString
    providerId() const override
    {
        return QStringLiteral("local");
    }
    [[nodiscard]] QString
    providerName() const override
    {
        return QObject::tr("Local catalog");
    }
    [[nodiscard]] bool
    isOnline() const override
    {
        return false;
    }

  private:
    AppCatalog* m_catalog;
};
