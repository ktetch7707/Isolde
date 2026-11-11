#pragma once
#include <QColor>
#include <QMap>
#include <QStringList>
#include <QVariantMap>

// Five shared theme colors. Explicit role values always override their group.
// Resolution is read-only; legacy documents never acquire palette data on load.
namespace ThemeColors {
inline QMap<QString, QStringList> roles() {
    return {{"background", {"backgroundStart", "backgroundEnd"}},
            {"cards", {"surfaceColor", "selectedColor"}},
            {"text", {"textColor", "mutedColor", "headingColor", "labelColor", "bodyColor"}},
            {"highlight", {"focusColor", "borderColor"}},
            {"accent", {"accentColor", "iconColor"}}};
}
inline bool role(const QString &key) {
    for (const auto &keys : roles()) if (keys.contains(key)) return true;
    return false;
}
inline bool valid(const QVariant &value) {
    if(value.metaType().id()!=QMetaType::QVariantMap)return false;
    const auto palette=value.toMap();
    if(palette.size()!=2 || (palette.value("schemaVersion").metaType().id()==QMetaType::Bool || palette.value("schemaVersion").metaType().id()==QMetaType::QString)
       || palette.value("schemaVersion").toDouble()!=1
       || palette.value("colors").metaType().id()!=QMetaType::QVariantMap)return false;
    const auto colors=palette.value("colors").toMap();
    if(colors.size()!=5)return false;
    for(auto it=colors.cbegin();it!=colors.cend();++it){
        const QColor color(it.value().toString());
        if(!roles().contains(it.key()) || it.value().metaType().id()!=QMetaType::QString || !color.isValid() || color.alpha()!=255)return false;
    }
    return true;
}
inline QVariantMap resolve(const QVariantMap &base,const QVariantMap &authored) {
    auto result=base;
    const auto palette=authored.value("colorTheme");
    if(valid(palette)){
        const auto colors=palette.toMap().value("colors").toMap();
        const auto groups=roles();
        for(auto it=groups.cbegin();it!=groups.cend();++it)
            for(const auto &key:it.value())result[key]=colors.value(it.key());
    }
    for(auto it=authored.cbegin();it!=authored.cend();++it)result[it.key()]=it.value();
    // Older page themes use one text role. Preserve that interpretation when
    // inheriting a new palette from Home without rewriting their documents.
    if(!authored.contains("colorTheme") && authored.contains("textColor"))
        for(const auto &key:QStringList{"headingColor","labelColor","bodyColor","iconColor"})
            if(!authored.contains(key))result[key]=authored.value("textColor");
    return result;
}
}
