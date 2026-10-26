#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QRegularExpression>
#include <QSet>

struct AppDefinition {
    QString id;
    QString name;
    QString type;
    QString presentationMode = QStringLiteral("native-tv");
    QString networkRequirement = QStringLiteral("none");
    QStringList handledActions;
    QString section;
    QString symbol;
    QString iconSource; // Resolved managed artwork, never a launch command or remote URL.
    QString cutoutIconSource;
    QColor accent;
    QVariantMap launchConfiguration;

    [[nodiscard]] bool isValid() const
    {
        if(id.trimmed().isEmpty() || name.trimmed().isEmpty() || type.trimmed().isEmpty())return false;
        if(type == "android") {
            static const QRegularExpression runtimeId("^[a-z0-9][a-z0-9.-]{0,79}$");
            const auto runtimeApp = launchConfiguration.value("runtimeApp");
            if(runtimeApp.metaType()!=QMetaType::fromType<QString>() || !runtimeId.match(runtimeApp.toString()).hasMatch()
                || presentationMode!="desktop-bridge" || !handledActions.isEmpty())return false;
            for(const auto &key : {"command", "executable", "arguments", "desktopId", "actionTarget"})
                if(launchConfiguration.contains(key))return false;
        }
        if(!launchConfiguration.contains("legacyIds"))return true;
        const auto value=launchConfiguration.value("legacyIds");
        if(value.metaType()!=QMetaType::fromType<QVariantList>() || value.toList().size()>8)return false;
        static const QRegularExpression safe("^[a-z0-9][a-z0-9.-]{0,79}$");
        QSet<QString> seen{id};
        for(const auto &entry:value.toList()){
            const auto alias=entry.toString();
            if(entry.metaType()!=QMetaType::fromType<QString>() || !safe.match(alias).hasMatch() || seen.contains(alias))return false;
            seen.insert(alias);
        }
        return true;
    }
    [[nodiscard]] QStringList legacyIds() const {QStringList result;for(const auto &entry:launchConfiguration.value("legacyIds").toList())result.append(entry.toString());return result;}
};
