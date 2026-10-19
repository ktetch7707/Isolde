#pragma once

#include <QObject>
#include <QVariantList>

class ArtPackCatalog final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList packs READ packs NOTIFY packsChanged)
    Q_PROPERTY(QVariantList artwork READ artwork NOTIFY packsChanged)
    Q_PROPERTY(QVariantList themes READ themes NOTIFY packsChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    explicit ArtPackCatalog(QObject *parent = nullptr);

    [[nodiscard]] QVariantList packs() const;
    [[nodiscard]] QVariantList artwork() const;
    [[nodiscard]] QVariantList themes() const;
    [[nodiscard]] QString errorString() const;

    bool load(const QString &path = {});
    bool loadFromData(const QByteArray &data);

signals:
    void packsChanged();
    void errorStringChanged();

private:
    void setError(const QString &message);

    QVariantList m_packs;
    QVariantList m_artwork;
    QVariantList m_themes;
    QString m_errorString;
};
