#pragma once
#include <QProcessEnvironment>
#include <QStringList>

struct NativeSandboxPlan {
    QString program;
    QStringList arguments;
    QProcessEnvironment environment;
    QString homeDirectory;
    QString actionTarget;
    QString error;
};

// Builds a fail-closed launch. Only append the application's arguments afterward.
class NativeSandbox {
public:
    static NativeSandboxPlan prepare(const QString &profileId, const QString &appId,
                                    const QString &executable, const QProcessEnvironment &host,
                                    const QString &actionTarget = {}, const QString &storageRoot = {},
                                    const QString &emulatorRuntime = {}, const QString &gameSourceRoot = {});
};
