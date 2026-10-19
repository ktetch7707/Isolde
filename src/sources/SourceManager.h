#pragma once

#include "app/AppRegistry.h"

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantList>

class SourceManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool automaticSourcesEnabled READ automaticSourcesEnabled WRITE setAutomaticSourcesEnabled NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantList sections READ sections NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(int visibleSourceCount READ visibleSourceCount NOTIFY sourcesChanged)

public:
    explicit SourceManager(AppRegistry *registry, QObject *parent = nullptr);

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
    QVariantList scanSources() const;
    QString defaultHandlerForAction(const QString &actionType) const;
    void loadState();
    void saveState() const;

    AppRegistry *m_registry;
    bool m_automaticSourcesEnabled = true;
    QVariantList m_sources;
    QStringList m_probeRoots;
    QSet<QString> m_hiddenSources;
    QSet<QString> m_permanentSources;
    QTimer m_refreshTimer;
};
