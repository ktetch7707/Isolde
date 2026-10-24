#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVariantMap>

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
    QColor accent;
    QVariantMap launchConfiguration;

    [[nodiscard]] bool isValid() const
    {
        return !id.trimmed().isEmpty() && !name.trimmed().isEmpty() && !type.trimmed().isEmpty();
    }
};
