#pragma once

#include "app/AppDefinition.h"

#include <QObject>
#include <QVariantList>
#include <QVector>

class AppRegistry final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList apps READ apps NOTIFY appsChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    explicit AppRegistry(QObject *parent = nullptr);

    [[nodiscard]] QVariantList apps() const;
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] const AppDefinition *find(const QString &id) const;
    bool addApp(const AppDefinition &app);
    bool replaceApp(const AppDefinition &app);
    bool removeApp(const QString &id);

    Q_INVOKABLE bool load(const QString &path = {});
    bool loadFromData(const QByteArray &data);

signals:
    void appsChanged();
    void errorStringChanged();

private:
    void setError(const QString &message);
    QVector<AppDefinition> m_apps;
    QString m_errorString;
};
