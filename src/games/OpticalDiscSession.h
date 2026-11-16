#pragma once

#include <QVariantMap>
#include <QString>

class OpticalDiscSwap {
  public:
    virtual ~OpticalDiscSwap() = default;

    virtual QVariantMap requestDiscSwap(const QString& sessionId,
                                        int expectedDiscNumber,
                                        const QString& expectedMediaSetId) = 0;
    virtual QVariantMap completeDiscSwap(const QString& sessionId,
                                         const QString& observationId,
                                         const QString& device) = 0;
    virtual QVariantMap swapSessionState(const QString& sessionId) = 0;
};

class OpticalDiscSession {
  public:
    virtual ~OpticalDiscSession() = default;

    virtual QString registerOpticalSession(const QString& sessionId,
                                           const QString& mediaId,
                                           const QString& device) = 0;
    virtual void releaseOpticalSession(const QString& sessionId) = 0;
    virtual QVariantMap opticalSessionArbitration(const QString& device) = 0;
    virtual QVariantMap latchSession(const QVariantMap& previous, QVariantMap current) = 0;
};
