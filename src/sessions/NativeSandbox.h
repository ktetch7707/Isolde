#pragma once
#include <QProcessEnvironment>
#include <QStringList>

struct NativeSandboxPlan {
    QString program;
    QStringList arguments;
    QProcessEnvironment environment;
    QString homeDirectory;
    QString actionTarget;
    QStringList steamLibraryRoots;
    QString error;
};

// Builds a fail-closed launch. Only append the application's arguments afterward.
class NativeSandbox {
public:
    static bool isGameSession(const QString &emulatorRuntime, const QString &steamGameId,
                              const QString &gameSourceRoot, const QString &managedGameRoot,
                              const QString &opticalMediaId);
    static bool hasGamepadKeyCapabilities(const QString &bitmap);
    static NativeSandboxPlan prepare(const QString &profileId, const QString &appId,
                                    const QString &executable, const QProcessEnvironment &host,
                                    const QString &actionTarget = {}, const QString &storageRoot = {},
                                    const QString &emulatorRuntime = {}, const QString &gameSourceRoot = {}, const QString &steamGameId = {},
                                    const QString &managedGameRoot = {}, const QString &opticalMediaId = {},
                                    const QString &opticalObservationId = {});
};
