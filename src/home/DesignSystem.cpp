#include "DesignSystem.h"
#include "ComponentSchema.h"
#include <QJsonArray>
#include <QSet>
#include <QObject>
#include <cmath>

namespace DesignSystem {
QVariantMap styleProperties(const QVariantMap &value)
{
    const QSet<QString> keys{"surfaceColor","fillEndColor","fillMode","fillDirection","iconColor","labelColor","selectedLabelColor","surfaceOpacity","radius","labelSize","iconSize","labelPosition","textAlign","labelBold","labelWrap","labelLines","labelSurfaceColor","labelPill","focusWidth","focusColor","focusEnabled","borderColor","borderWidth","cardGap","padding","artFit","artFocalX","artFocalY","artZoom","states"};
    QVariantMap result;for(auto it=value.cbegin();it!=value.cend();++it)if(keys.contains(it.key()))result[it.key()]=it.value();return result;
}
bool validResponsive(const QVariantMap &rule)
{
    const QSet<QString> keys{"schemaVersion", "policy", "minimumWidth", "minimumHeight", "aspect"};
    if (rule.value("schemaVersion").metaType().id()==QMetaType::QString || rule.value("schemaVersion").metaType().id()==QMetaType::Bool
        || rule.value("schemaVersion").toDouble()!=1 || rule.size()!=5
        || !QStringList{"wrap","scroll","shrink","stack"}.contains(rule.value("policy").toString()))return false;
    for (auto it = rule.cbegin(); it != rule.cend(); ++it) {
        if (!keys.contains(it.key())) return false;
        if (it.key() == "minimumWidth" || it.key() == "minimumHeight" || it.key() == "aspect") {
            bool ok = false; const double n = it.value().toDouble(&ok);
            if (!ok || it.value().metaType().id() == QMetaType::QString || it.value().metaType().id() == QMetaType::Bool
                || !std::isfinite(n) || n < (it.key() == "aspect" ? .25 : 16) || n > (it.key() == "aspect" ? 4 : 800)) return false;
        }
    }
    return true;
}
bool valid(const QVariantMap &library)
{
    if (library.size() != 3 || library.value("schemaVersion").metaType().id()==QMetaType::QString || library.value("schemaVersion").metaType().id()==QMetaType::Bool || library.value("schemaVersion").toDouble()!=1
        || library.value("styles").metaType().id() != QMetaType::QVariantList
        || library.value("components").metaType().id() != QMetaType::QVariantList) return false;
    for (const auto &kind : {"styles", "components"}) {
        const auto entries = library.value(kind).toList(); if (entries.size() > 64) return false;
        QSet<QString> ids;
        for (const auto &value : entries) {
            const auto entry = value.toMap(); const auto id = entry.value("id").toString();
            if (entry.size() != 3 || !ComponentSchema::safeId(id) || ids.contains(id)
                || entry.value("name").toString().trimmed().isEmpty() || entry.value("name").toString().size() > 80) return false;
            ids.insert(id);
            if (QString(kind) == "styles") {
                const auto style = entry.value("presentation").toMap();
                if (entry.value("presentation").metaType().id() != QMetaType::QVariantMap || styleProperties(style)!=style || style.contains("styleId")
                    || !ComponentSchema::validPresentation(style)) return false;
            } else {
                const auto nodes = entry.value("nodes").toList();
                if (nodes.isEmpty() || nodes.size() > 128) return false;
                QSet<QString> nodeIds;
                for (const auto &nodeValue : nodes) {
                    const auto node = nodeValue.toMap(); const auto nodeId = node.value("id").toString();
                    // Nested linked definitions are refused, so symbol cycles cannot enter storage.
                    if (node.contains("linkedDesign") || node.contains("designNode") || nodeIds.contains(nodeId)
                        || ComponentSchema::component(node, false).isEmpty()) return false;
                    if (nodeIds.isEmpty() ? !node.value("parentId").toString().isEmpty() : !nodeIds.contains(node.value("parentId").toString())) return false;
                    nodeIds.insert(nodeId);
                }
            }
        }
    }
    return true;
}
bool validDocument(const QJsonObject &document, QString *error)
{
    const auto library=document["composition"].toObject()["designLibrary"].toObject().toVariantMap();
    QSet<QString> styles, symbols;
    for(const auto &entry:library.value("styles").toList())styles.insert(entry.toMap().value("id").toString());
    for(const auto &entry:library.value("components").toList())symbols.insert(entry.toMap().value("id").toString());
    QList<QJsonObject> layouts{document};
    for(const auto &entry:document["pages"].toArray())layouts.append(entry.toObject()["layout"].toObject());
    for(const auto &layout:layouts) {
        for(const auto &entry:layout["categories"].toArray()) {
            const auto category=entry.toObject(),style=category["presentation"].toObject();const auto rule=style["responsive"].toObject();if(rule.isEmpty())continue;
            bool invalid=style["variant"]=="library" || layout["composition"].toObject()["grid"].toObject()["mode"]=="sections";
            int count=0;for(const auto &value:layout["components"].toArray())if(value.toObject()["categoryId"]==category["id"] && value.toObject()["parentId"].toString().isEmpty()){++count;if(value.toObject().contains("placement"))invalid=true;}
            const auto policy=rule["policy"].toString();const int columns=policy=="stack"?1:policy=="wrap"?qMax(1,qMin(style["columns"].toInt(4),int(360/rule["minimumWidth"].toDouble()))):style["columns"].toInt(4);
            const auto composition=layout["composition"].toObject();
            const double gap=style["cardGap"].toDouble(16);
            const double sectionWidth=(1440-2*composition["margin"].toDouble(24))/qMax(1,composition["columns"].toInt(1))-2*style["padding"].toDouble();
            const double cardWidth=(sectionWidth-(columns-1)*gap)/columns;
            const double cardHeight=qMax(rule["minimumHeight"].toDouble(),cardWidth/rule["aspect"].toDouble());
            const double available=style["height"].toDouble(180)-2*style["padding"].toDouble()-64;
            const int rows=int(std::ceil(double(count)/columns));
            if(cardWidth<rule["minimumWidth"].toDouble() || cardHeight>available || (policy!="scroll" && rows*cardHeight+qMax(0,rows-1)*gap>available))invalid=true;
            if(invalid){if(error)*error=QObject::tr("Responsive section rules overflow their minimum viewport or use an authored grid. Increase section height, reduce minimum size, or choose Scroll.");return false;}
        }
    }
    for(const auto &layout:layouts)for(const auto &value:layout["components"].toArray()) {
        auto node=value.toObject().toVariantMap();const auto authored=node.value("presentation").toMap();
        if((authored.contains("styleId") && !styles.contains(authored.value("styleId").toString()))
            || (node.contains("linkedDesign") && !symbols.contains(node.value("linkedDesign").toString()))) {
            if(error)*error=QObject::tr("A shared style or component definition is missing. Import its whole interface or detach it first.");return false;
        }
        if(!authored.contains("styleId") && !node.contains("linkedDesign"))continue;
        node["presentation"]=presentation(authored,library);
        if(ComponentSchema::component(node,false).isEmpty()) {if(error)*error=QObject::tr("A shared style is incompatible with a linked component. Nothing changed.");return false;}
    }
    return true;
}
QVariantMap presentation(const QVariantMap &authored, const QVariantMap &library)
{
    QVariantMap result;
    for (const auto &value : library.value("styles").toList()) {
        const auto style = value.toMap();
        if (style.value("id") == authored.value("styleId")) result = style.value("presentation").toMap();
    }
    for (auto it = authored.cbegin(); it != authored.cend(); ++it) if (it.key() != "styleId") result[it.key()] = it.value();
    return result;
}
QVariantMap responsive(const QVariantMap &authored, double width, double height, double unit)
{
    auto result = authored; const auto rule = result.value("responsive").toMap();
    if (rule.isEmpty() || width <= 0 || height <= 0 || !std::isfinite(unit) || unit <= 0) return result;
    const auto policy = rule.value("policy").toString();
    const double minimum = qMax(rule.value("minimumWidth").toDouble(), rule.value("minimumHeight").toDouble() * rule.value("aspect").toDouble());
    const int columns = result.value("columns", 4).toInt();
    if (policy == "stack") result["columns"] = 1;
    else if (policy == "wrap") result["columns"] = qMax(1, qMin(columns, int(width / minimum)));
    else if (policy == "shrink") {
        // contentWidth is authored scene data; viewport dimensions are pixels.
        // Return scene units so the renderer applies its scale exactly once.
        const double available = width / unit;
        result["contentWidth"] = qMax(minimum, qMin(available, result.value("contentWidth", available).toDouble()));
    }
    return result;
}
}
