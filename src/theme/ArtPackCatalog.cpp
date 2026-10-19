#include "theme/ArtPackCatalog.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

ArtPackCatalog::ArtPackCatalog(QObject *parent) : QObject(parent) {}

QVariantList ArtPackCatalog::packs() const { return m_packs; }
QVariantList ArtPackCatalog::artwork() const { return m_artwork; }
QVariantList ArtPackCatalog::themes() const { return m_themes; }
QString ArtPackCatalog::errorString() const { return m_errorString; }

bool ArtPackCatalog::load(const QString &path)
{
    QString resolvedPath = path;
    if (resolvedPath.isEmpty()) {
        resolvedPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config/art-packs.json"));
        if (!QFile::exists(resolvedPath))
            resolvedPath = QStringLiteral(":/config/art-packs.json");
    }
    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(tr("Unable to open art pack catalog: %1").arg(resolvedPath));
        return false;
    }
    return loadFromData(file.readAll());
}

bool ArtPackCatalog::loadFromData(const QByteArray &data)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || document.object().value(QStringLiteral("schemaVersion")).toInt() != 1
        || !document.object().value(QStringLiteral("packs")).isArray()) {
        setError(tr("Invalid art pack catalog: %1").arg(parseError.errorString()));
        return false;
    }

    static const QRegularExpression safeId(QStringLiteral("^[a-z0-9][a-z0-9.-]*$"));
    QVariantList packs;
    QVariantList artwork;
    QVariantList themes;
    QSet<QString> packIds;
    QSet<QString> artworkKeys;
    QSet<QString> themeKeys;
    for (const auto &packValue : document.object().value(QStringLiteral("packs")).toArray()) {
        const auto pack = packValue.toObject();
        const auto packId = pack.value(QStringLiteral("id")).toString().trimmed();
        const auto packName = pack.value(QStringLiteral("name")).toString().trimmed();
        if (!safeId.match(packId).hasMatch() || packName.isEmpty() || packIds.contains(packId)
            || !pack.value(QStringLiteral("artwork")).isArray()) {
            setError(tr("Art packs require unique IDs, names, and artwork arrays."));
            return false;
        }
        packIds.insert(packId);
        QVariantList packThemes;
        for (const auto &themeValue : pack.value(QStringLiteral("themes")).toArray()) {
            const auto theme = themeValue.toObject();
            const auto themeId = theme.value(QStringLiteral("id")).toString().trimmed();
            const auto name = theme.value(QStringLiteral("name")).toString().trimmed();
            const QColor start(theme.value(QStringLiteral("backgroundStart")).toString());
            const QColor end(theme.value(QStringLiteral("backgroundEnd")).toString());
            const QColor focus(theme.value(QStringLiteral("focusColor")).toString(QStringLiteral("#F8FAFC")));
            const auto roundness = theme.contains(QStringLiteral("tileRoundness")) ? theme.value(QStringLiteral("tileRoundness")).toDouble() : -1.0;
            const auto typographyScale = theme.contains(QStringLiteral("typographyScale")) ? theme.value(QStringLiteral("typographyScale")).toDouble() : 1.0;
            const auto tileOpacity = theme.contains(QStringLiteral("tileOpacity")) ? theme.value(QStringLiteral("tileOpacity")).toDouble() : 1.0;
            const auto key = packId + QStringLiteral("/") + themeId;
            if (!safeId.match(themeId).hasMatch() || name.isEmpty() || !start.isValid() || !end.isValid()
                || !focus.isValid() || roundness < 0.0 || roundness > 1.0 || typographyScale < 0.8 || typographyScale > 1.3
                || tileOpacity < 0.2 || tileOpacity > 1.0 || themeKeys.contains(key)) {
                setError(tr("Theme entries require unique IDs, names, valid colors, roundness from 0 to 1, typography scale from 0.8 to 1.3, and tile opacity from 0.2 to 1."));
                return false;
            }
            QVariantMap suggestedArtwork;
            const auto suggested = theme.value(QStringLiteral("suggestedArtwork")).toObject();
            for (auto it = suggested.constBegin(); it != suggested.constEnd(); ++it) {
                const auto suggestion = it.value().toObject();
                const auto symbol = suggestion.value(QStringLiteral("symbol")).toString().trimmed();
                const QColor accent(suggestion.value(QStringLiteral("accent")).toString());
                if (!safeId.match(it.key()).hasMatch() || symbol.isEmpty() || !accent.isValid()) {
                    setError(tr("Theme suggested artwork mappings require safe IDs."));
                    return false;
                }
                suggestedArtwork.insert(it.key(), QVariantMap{{QStringLiteral("symbol"), symbol.left(3)},
                                                              {QStringLiteral("accent"), accent.name()}});
            }
            themeKeys.insert(key);
            const QVariantMap entry{{QStringLiteral("packId"), packId},
                                    {QStringLiteral("id"), themeId},
                                    {QStringLiteral("name"), name},
                                    {QStringLiteral("description"), theme.value(QStringLiteral("description")).toString().trimmed()},
                                    {QStringLiteral("backgroundStart"), start.name()},
                                    {QStringLiteral("backgroundEnd"), end.name()},
                                    {QStringLiteral("focusColor"), focus.name()},
                                    {QStringLiteral("typographyScale"), typographyScale},
                                    {QStringLiteral("tileRoundness"), roundness},
                                    {QStringLiteral("tileOpacity"), tileOpacity},
                                    {QStringLiteral("suggestedArtwork"), suggestedArtwork}};
            packThemes.append(entry);
            themes.append(entry);
        }
        QVariantList packArtwork;
        for (const auto &artValue : pack.value(QStringLiteral("artwork")).toArray()) {
            const auto art = artValue.toObject();
            const auto artId = art.value(QStringLiteral("id")).toString().trimmed();
            const auto name = art.value(QStringLiteral("name")).toString().trimmed();
            const auto symbol = art.value(QStringLiteral("symbol")).toString().trimmed();
            const QColor accent(art.value(QStringLiteral("accent")).toString());
            const auto key = packId + QStringLiteral("/") + artId;
            if (!safeId.match(artId).hasMatch() || name.isEmpty() || symbol.isEmpty() || !accent.isValid()
                || artworkKeys.contains(key)) {
                setError(tr("Artwork entries require unique IDs, names, symbols, and valid accent colors."));
                return false;
            }
            artworkKeys.insert(key);
            const QVariantMap entry{{QStringLiteral("packId"), packId},
                                    {QStringLiteral("id"), artId},
                                    {QStringLiteral("name"), name},
                                    {QStringLiteral("category"), art.value(QStringLiteral("category")).toString().trimmed()},
                                    {QStringLiteral("symbol"), symbol.left(3)},
                                    {QStringLiteral("accent"), accent.name()}};
            packArtwork.append(entry);
            artwork.append(entry);
        }
        packs.append(QVariantMap{{QStringLiteral("id"), packId},
                                 {QStringLiteral("name"), packName},
                                 {QStringLiteral("description"), pack.value(QStringLiteral("description")).toString().trimmed()},
                                 {QStringLiteral("themes"), packThemes},
                                 {QStringLiteral("artwork"), packArtwork}});
    }
    m_packs = packs;
    m_artwork = artwork;
    m_themes = themes;
    setError({});
    emit packsChanged();
    return true;
}

void ArtPackCatalog::setError(const QString &message)
{
    if (m_errorString == message)
        return;
    m_errorString = message;
    emit errorStringChanged();
}
