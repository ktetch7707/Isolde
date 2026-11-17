#include <QFile>
#include <QFileInfo>
#include "CompatibilityRuntime.h"

QVariantMap
CompatibilityRuntime::classify(bool present, bool trusted)
{
    return {{"id", "wine"},
            {"name", "Windows compatibility installer"},
            {"state",
             !present  ? "missing"
             : trusted ? "unverified"
                       : "unsafe"},
            {"supported", false},
            {"requirements",
             "A pinned, verified Isolde compatibility runtime and isolated installer adapter are required. No Windows "
             "installer is executed."}};
}

QVariantMap
CompatibilityRuntime::discover()
{
#ifndef Q_OS_LINUX
    return classify(false, false);
#else
    return discoverLinux();
#endif
}

QVariantMap
CompatibilityRuntime::discoverLinux()
{
    const QFileInfo candidate("/usr/bin/wine");
    if (!isWineAvailable(candidate))
        return classify(false, false);
    if (!isWineBinaryValid(candidate))
        return classify(true, false);
    if (!isPathSecure(candidate))
        return classify(true, false);
    return classify(true, true);
}

bool
isWineAvailable(const QFileInfo& candidate)
{
    return candidate.exists() || candidate.isSymLink();
}

namespace
{
bool
isWineBinaryValid(const QFileInfo& candidate)
{
    const auto resolved = candidate.canonicalFilePath();
    return resolved.startsWith("/usr/") && QFileInfo(resolved).isFile() && QFileInfo(resolved).isExecutable();
}
bool
isPathSecure(const QFileInfo& path)
{
    for (auto p = path;; p = QFileInfo(p.absolutePath()))
    {
        if (p.isSymLink() || p.ownerId() != 0 || (p.permissions() & (QFile::WriteGroup | QFile::WriteOther)))
            return false;
        if (p.absoluteFilePath() == "/")
            break;
    }
    return true;
}
} // namespace
