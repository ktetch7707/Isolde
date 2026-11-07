#pragma once

#include "app/AppRegistry.h"

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QPointer>
#include <QMap>

class HomeLayout;

class SourceManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool automaticSourcesEnabled READ automaticSourcesEnabled WRITE setAutomaticSourcesEnabled NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantList sections READ sections NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(int visibleSourceCount READ visibleSourceCount NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantMap policy READ policy NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantList hiddenSources READ hiddenSources NOTIFY sourcesChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY sourcesChanged)
    Q_PROPERTY(int revision READ revision NOTIFY sourcesChanged)

public:
    explicit SourceManager(AppRegistry *registry, QObject *parent = nullptr, const QString &statePath = {});
    void setLayout(HomeLayout *layout);
    QVariantMap policy() const;
    QVariantList hiddenSources() const;
    QString errorString() const { return m_error; }
    int revision() const { return m_revision; }
    Q_INVOKABLE bool setPolicy(const QVariantMap &changes);
    Q_INVOKABLE QVariantList projected(const QString &pageId, const QString &slotId, bool notice, const QVariantMap &workflow = {}) const;
    Q_INVOKABLE bool dismissSource(const QString &id);
    Q_INVOKABLE QVariantList handlers(const QString &id) const;
    Q_INVOKABLE bool chooseHandler(const QString &id, const QString &handler);
    Q_INVOKABLE bool promoteSource(const QString &id);

    [[nodiscard]] bool automaticSourcesEnabled() const;
    void setAutomaticSourcesEnabled(bool enabled);
    [[nodiscard]] QVariantList items() const;
    [[nodiscard]] QVariantList sections() const;
    [[nodiscard]] QVariantList sources() const;
    [[nodiscard]] int visibleSourceCount() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool hideSource(const QString &id);
    Q_INVOKABLE bool markPermanent(const QString &id);
    Q_INVOKABLE bool forgetSourceDecision(const QString &id);
    Q_INVOKABLE QVariantMap sourceAction(const QString &id) const;

    void setProbeRootsForTests(const QStringList &roots);

signals:
    void sourcesChanged();

private:
    QVariantList scanSources(QString *error) const;
    QString defaultHandlerForAction(const QString &actionType) const;
    void loadState();
    bool saveState();
    bool fail(const QString &message);
    bool decide(const QString &id, const QString &key, const QVariant &value);

    AppRegistry *m_registry;
    bool m_automaticSourcesEnabled = true;
    QVariantList m_sources;
    QStringList m_probeRoots;
    QTimer m_refreshTimer;
    QPointer<HomeLayout> m_layout;
    QVariantMap m_decisions;
    QVariantList m_discovered;
    QMap<QString,qint64> m_events;
    QSet<QString> m_seen, m_dismissed;
    QString m_statePath, m_error;
    bool m_writable=true;
    int m_revision=0;
    int m_scanTicks=0;
};
