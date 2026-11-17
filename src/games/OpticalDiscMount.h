#pragma once

#include <QDBusMessage>
#include <QString>
#include <functional>

class OpticalDiscMount
{
  public:
    using DbusCall = std::function<QDBusMessage(
        const QString&, const QString&, const QString&, const QString&, const QVariantList&)>;

    static bool
    mountReadOnly(const QString& device, QString* error = nullptr);
    static bool
    mountUsing(const QString& device,
               bool mediaAvailable,
               const DbusCall& call,
               QString* mountPoint,
               QString* error = nullptr);
    static bool
    eject(const QString& device, QString* error = nullptr);
    static bool
    ejectUsing(const QString& device, bool mounted, const DbusCall& call, QString* error = nullptr);
};
