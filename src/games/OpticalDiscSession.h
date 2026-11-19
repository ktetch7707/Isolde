#pragma once

#include <QString>
#include <QVariantMap>

class OpticalDiscSwap
{
  public:
    virtual ~OpticalDiscSwap() = default;

    static QVariantMap
    requestDiscSwap(const QString& sessionId, int expectedDiscNumber, const QString& expectedMediaSetId);
    static QVariantMap
    completeDiscSwap(const QString& sessionId, const QString& observationId, const QString& device);
    static QVariantMap
    swapSessionState(const QString& sessionId);
};

class OpticalDiscSession
{
  public:
    virtual ~OpticalDiscSession() = default;

    static QString
    registerOpticalSession(const QString& sessionId, const QString& mediaId, const QString& device);
    static void
    releaseOpticalSession(const QString& sessionId);
    static QVariantMap
    opticalSessionArbitration(const QString& device);
    static QVariantMap
    latchSession(const QVariantMap& previous, QVariantMap current);
};
