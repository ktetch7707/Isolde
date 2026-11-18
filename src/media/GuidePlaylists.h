#pragma once
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include "home/ComponentSchema.h"
namespace GuidePlaylists
{
inline const QMap<QString, QString> fields{{"title", "title"},
                                           {"showtitle", "tvshow"},
                                           {"studio", "studio"},
                                           {"genre", "genre"},
                                           {"tag", "tag"},
                                           {"year", "year"},
                                           {"rating", "rating"},
                                           {"playcount", "playcount"},
                                           {"actor", "actor"},
                                           {"director", "director"},
                                           {"path", "path"},
                                           {"season", "season"},
                                           {"episode", "episode"}};
inline const QMap<QString, QString> operators{{"is", "is"},
                                              {"is-not", "isnot"},
                                              {"contains", "contains"},
                                              {"does-not-contain", "doesnotcontain"},
                                              {"starts-with", "startswith"}};
inline const QMap<QString, QString> kinds{
    {"movie", "movies"}, {"episode", "episodes"}, {"musicvideo", "musicvideos"}, {"song", "songs"}};


inline bool
validatePlaylistRoot(QXmlStreamReader& reader, QString& kind)
{
    if (reader.name() != u"smartplaylist" || reader.attributes().size() != 1)
        return false;
    kind = kinds.key(reader.attributes().value("type").toString());
    return !kind.isEmpty();
}

inline bool
parsePlaylistElement(QXmlStreamReader& reader, QString& name, QString& match, QVariantList& rules)
{
    if (reader.name() == u"name")
        return parsePlaylistName(reader, name);
    if (reader.name() == u"match")
        return parsePlaylistMatch(reader, match);
    if (reader.name() == u"rule")
    {
        const auto field = fields.key(reader.attributes().value("field").toString()),
                   op = operators.key(reader.attributes().value("operator").toString());
        if (reader.attributes().size() != 2 || field.isEmpty() || op.isEmpty() || !reader.readNextStartElement() ||
            reader.name() != u"value")
            return false;
        const auto text = reader.readElementText();
        if (reader.readNextStartElement())
            return false;
        QVariant value = text;
        if (QStringList{"year", "rating", "playcount", "season", "episode"}.contains(field))
        {
            bool ok = false;
            const auto number = text.toDouble(&ok);
            if (!ok)
                return false;
            value = number;
        }
        QVariantMap rule{{"field", field}, {"op", op}, {"value", value}};
        if (ComponentSchema::guideRule(rule).isEmpty() || rules.size() >= 32)
            return false;
        rules.append(rule);
        return true;
    }
    return false; // order/limit/nested rules are meaningful: never discard them.
}

inline QByteArray
encode(const QVariantMap& definition, QString* error)
{
    const auto fail = [error]()
    {
        if (error)
            *error =
                "This rule set cannot be represented losslessly as a Kodi smart playlist. Export a JSON channel "
                "package instead.";
        return QByteArray();
    };
    const auto media = definition.value("mediaKinds").toList();
    if (media.size() != 1 || !kinds.contains(media.first().toString()))
        return fail();
    const auto rule = definition.value("rule").toMap();
    QVariantList children;
    QString match = "all";
    if (rule.value("op") == "all" || rule.value("op") == "any")
    {
        children = rule.value("rules").toList();
        match = rule.value("op").toString();
    }
    else
        children = {rule};
    QByteArray output;
    QXmlStreamWriter writer(&output);
    writer.writeStartDocument();
    writer.writeStartElement("smartplaylist");
    writer.writeAttribute("type", kinds.value(media.first().toString()));
    writer.writeTextElement("name", definition.value("name").toString());
    writer.writeTextElement("match", match == "any" ? "one" : match);
    for (const auto& v : children)
    {
        const auto child = v.toMap();
        if (child == QVariantMap{{"field", "source"}, {"op", "is"}, {"value", "kodi"}} && match == "all")
            continue;
        const auto field = child.value("field").toString(), op = child.value("op").toString();
        if (child.size() != 3 || !fields.contains(field) || !operators.contains(op) ||
            child.value("value").metaType().id() == QMetaType::QVariantList)
            return fail();
        writer.writeStartElement("rule");
        writer.writeAttribute("field", fields.value(field));
        writer.writeAttribute("operator", operators.value(op));
        writer.writeTextElement("value", child.value("value").toString());
        writer.writeEndElement();
    }
    writer.writeEndElement();
    writer.writeEndDocument();
    if (error)
        error->clear();
    return output;
}

inline bool
validatePlaylistSize(const QByteArray& bytes)
{
    return !bytes.isEmpty() && bytes.size() <= 256 * 1024;
}

inline bool
validatePlaylistCompletion(QXmlStreamReader& reader, bool root, const QString& name, const QString& match)
{
    return !reader.hasError() && root && !name.trimmed().isEmpty() && name.size() <= 160 && !match.isEmpty();
}

inline bool
parsePlaylistName(QXmlStreamReader& reader, QString& name)
{
    if (!name.isEmpty())
        return false;
    name = reader.readElementText();
    return true;
}

inline bool
parsePlaylistMatch(QXmlStreamReader& reader, QString& match)
{
    if (!match.isEmpty())
        return false;
    match = reader.readElementText();
    return match == "all" || match == "one" || match == "any";
}

inline QVariantMap
decode(const QByteArray& bytes, const QVariantMap& fresh, QString* error)
{
    const auto fail = [error]()
    {
        if (error)
            *error =
                "Unsupported or malformed Kodi smart playlist. Only single-media, flat all/any rules with known "
                "fields and operators can be imported without losing meaning.";
        return QVariantMap();
    };
    if (!validatePlaylistSize(bytes))
        return fail();
    QXmlStreamReader reader(bytes);
    QVariantList rules;
    QString name, match, kind;
    bool root = false;
    while (!reader.atEnd())
    {
        const auto token = reader.readNext();
        if (token == QXmlStreamReader::DTD || token == QXmlStreamReader::EntityReference)
            return fail();
        if (!reader.isStartElement())
            continue;
        if (!root)
        {
            if (!validatePlaylistRoot(reader, kind))
                return fail();
            root = true;
            continue;
        }
        if (!parsePlaylistElement(reader, name, match, rules))
            return fail();
    }
    if (!validatePlaylistCompletion(reader, root, name, match))
        return fail();
    auto result = fresh;
    result["name"] = name;
    result["mediaKinds"] = QVariantList{kind};
    result["rule"] = rules.isEmpty()     ? QVariantMap{{"field", "source"}, {"op", "is"}, {"value", "kodi"}}
                     : rules.size() == 1 ? rules.first().toMap()
                                         : QVariantMap{{"op", match == "all" ? "all" : "any"}, {"rules", rules}};
    return ComponentSchema::guideChannel(result, error);
}


} // namespace GuidePlaylists
