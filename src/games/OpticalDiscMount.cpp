#include "OpticalDiscIdentity.h"
#include "OpticalDiscMount.h"
#include "OpticalDiscSession.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDir>
#include <QStorageInfo>

bool
OpticalDiscMount::mountReadOnly(const QString& device, QString* error)
{
    bool available = false;
    for (const auto& item : OpticalDiscHash::mountedDiscs())
    {
        const auto disc = item.toMap();
        if (disc.value("device") == device && disc.value("state") == "unmounted")
            available = true;
    }
    const auto call = [](const QString& service,
                         const QString& path,
                         const QString& interface,
                         const QString& method,
                         const QVariantList& arguments)
    {
        QDBusInterface object(service, path, interface, QDBusConnection::systemBus());
        return object.callWithArgumentList(QDBus::Block, method, arguments);
    };
    QString mountPoint;
    if (!mountUsing(device, available, call, &mountPoint, error))
        return false;
    QStorageInfo mounted(mountPoint);
    mounted.refresh();
    if (!mounted.isValid() || !mounted.isReady() || !mounted.isReadOnly() ||
        QString::fromLatin1(mounted.device()) != device)
    {
        if (error)
            *error =
                "A mount was requested, but Isolde could not verify the same optical device is mounted read-only. "
                "Do not use it until refreshed.";
        return false;
    }
    return true;
}

bool
OpticalDiscMount::mountUsing(
    const QString& device, bool mediaAvailable, const DbusCall& call, QString* mountPoint, QString* error)
{
    if (error)
        error->clear();
    if (mountPoint)
        mountPoint->clear();
    if (!QRegularExpression("^/dev/sr[0-9]{1,3}$").match(device).hasMatch())
    {
        if (error)
            *error = "Invalid optical device.";
        return false;
    }
    if (!mediaAvailable)
    {
        if (error)
            *error = "No unmounted optical media is available in this drive. Refresh and retry.";
        return false;
    }
    if (!call)
    {
        if (error)
            *error = "Read-only mount is unavailable; the optical drive service is not responding.";
        return false;
    }
    const auto object = "/org/freedesktop/UDisks2/block_devices/" + device.mid(QString("/dev/").size());
    const auto reply = call("org.freedesktop.UDisks2",
                            object,
                            "org.freedesktop.UDisks2.Filesystem",
                            "Mount",
                            {QVariantMap{{"options", "ro"}}});
    if (reply.type() == QDBusMessage::ErrorMessage)
    {
        if (error)
            *error =
                "The disc could not be mounted read-only. Check that media is present and readable, then refresh; "
                "no installer or disc content was started.";
        return false;
    }
    if (reply.arguments().isEmpty())
    {
        if (error)
            *error = "The drive did not confirm a mountpoint. Refresh optical media before continuing.";
        return false;
    }
    const auto path = reply.arguments().first().toString();
    if (!QDir::isAbsolutePath(path) || QDir::cleanPath(path) != path)
    {
        if (error)
            *error = "The drive returned an invalid mountpoint. Refresh optical media before continuing.";
        return false;
    }
    if (mountPoint)
        *mountPoint = path;
    return true;
}

bool
OpticalDiscMount::eject(const QString& device, QString* error)
{
    bool mounted = false;
    for (const auto& volume : QStorageInfo::mountedVolumes())
        mounted |= volume.isValid() && volume.isReady() && QString::fromLatin1(volume.device()) == device;
    const auto arbitration = OpticalDiscSession::opticalSessionArbitration(device);
    if (arbitration.value("state").toString() == "blocked")
    {
        if (error)
            *error = "The disc is in use by an active game session. Stop the session or return Home before ejecting.";
        return false;
    }
    const auto call = [](const QString& service,
                         const QString& path,
                         const QString& interface,
                         const QString& method,
                         const QVariantList& arguments)
    {
        QDBusInterface object(service, path, interface, QDBusConnection::systemBus());
        return object.callWithArgumentList(QDBus::Block, method, arguments);
    };
    return ejectUsing(device, mounted, call, error);
}

bool
OpticalDiscMount::ejectUsing(const QString& device, bool mounted, const DbusCall& call, QString* error)
{
    if (error)
        error->clear();
    if (!QRegularExpression("^/dev/sr[0-9]{1,3}$").match(device).hasMatch())
    {
        if (error)
            *error = "Invalid optical device.";
        return false;
    }
    if (!call)
    {
        if (error)
            *error = "Safe eject is unavailable; the optical drive service is not responding.";
        return false;
    }
    const auto name = device.mid(QString("/dev/").size());
    const auto object = "/org/freedesktop/UDisks2/block_devices/" + name;
    if (mounted)
    {
        const auto reply =
            call("org.freedesktop.UDisks2", object, "org.freedesktop.UDisks2.Filesystem", "Unmount", {QVariantMap{}});
        if (reply.type() == QDBusMessage::ErrorMessage)
        {
            if (error)
                *error = "The disc is in use and could not be safely unmounted. Close the game or operation and retry.";
            return false;
        }
    }
    const auto driveReply = call("org.freedesktop.UDisks2",
                                 object,
                                 "org.freedesktop.UDisks2.DBus.Properties",
                                 "Get",
                                 {QString("org.freedesktop.UDisks2.Block"), QString("Drive")});
    if (driveReply.type() == QDBusMessage::ErrorMessage || driveReply.arguments().isEmpty())
    {
        if (error)
            *error = "Safe eject is unavailable; the optical drive service is not responding.";
        return false;
    }
    const auto drivePath = driveReply.arguments().at(0).value<QDBusVariant>().variant().value<QDBusObjectPath>().path();
    if (drivePath.isEmpty() || drivePath == "/")
    {
        if (error)
            *error = "Safe eject is unavailable; the optical drive identity is incomplete.";
        return false;
    }
    const auto reply =
        call("org.freedesktop.UDisks2", drivePath, "org.freedesktop.UDisks2.Drive", "Eject", {QVariantMap{}});
    if (reply.type() == QDBusMessage::ErrorMessage)
    {
        if (error)
            *error = "The drive refused safe eject because media is busy or unavailable. Nothing was force-ejected.";
        return false;
    }
    return true;
}
