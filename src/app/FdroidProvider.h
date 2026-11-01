#pragma once

#include "app/StoreProvider.h"
#include <QProcess>
#include <QPointer>

// One instance per ProfileRuntime. Creating it performs no network requests.
class FdroidProvider final : public StoreProvider {
    Q_OBJECT
public:
    explicit FdroidProvider(QObject *parent = nullptr);
    ~FdroidProvider() override;
    QString providerId() const override { return QStringLiteral("fdroid"); }
    QString providerName() const override { return tr("F-Droid"); }
    bool isOnline() const override { return true; }
    QVariantList search(const QString &query) override;
    void requestSearch(const QString &query) override;
    void requestPage(int direction) override;
    void requestUpdates(const QStringList &packages) override;
    QVariantList updateResults() const override { return m_updates; }
    void cancel() override;
    QString status() const override { return m_status; }
    bool hasNextPage() const override { return m_next; }
    bool hasPreviousPage() const override { return m_previous; }
    QUrl artifactUrl(const QString &storeId) const override;
    QByteArray artifactHash(const QString &storeId) const override;
    QString provenanceText(const QString &storeId) const override;

private:
    void start(int page, bool updates = false);
    QVariantMap result(const QString &storeId) const;
    QPointer<QProcess> m_process;
    QString m_query;
    QString m_status;
    QString m_profile;
    int m_page = 0;
    bool m_next = false;
    bool m_previous = false;
    QStringList m_updateQuery;
    QVariantList m_updates;
};
