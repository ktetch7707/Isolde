#include "ManagedGif.h"
#include <QBuffer>
#include <QElapsedTimer>
#include <QImage>
#include <QImageReader>

bool ManagedGif::matches(const QByteArray &data)
{
    return data.startsWith("GIF87a") || data.startsWith("GIF89a");
}
QVariantMap ManagedGif::decode(const QByteArray &data, bool includeFrames, QString *error)
{
    if(error)error->clear();
    const auto fail=[&](const QString &reason){if(error)*error=reason;return QVariantMap{};};
    if(!matches(data) || data.size()>2*1024*1024)return fail(QStringLiteral("Use a GIF of at most 2 MiB."));
    QElapsedTimer elapsed;elapsed.start();QBuffer input;input.setData(data);input.open(QIODevice::ReadOnly);
    QImageReader reader(&input,"gif");const auto size=reader.size();
    if(!size.isValid() || size.width()>1024 || size.height()>1024)return fail(QStringLiteral("GIF dimensions must be within 1024 × 1024 pixels."));
    const auto count=reader.imageCount();
    if(count<1 || count>60)return fail(QStringLiteral("A GIF must contain 1–60 readable frames."));
    if(qint64(size.width())*size.height()*4*count>4*1024*1024)return fail(QStringLiteral("GIF decoded frames exceed the 4 MiB animation budget."));
    const auto loops=reader.loopCount();
    QVariantList frames,delays;qint64 encoded=0,decoded=0;int duration=0;
    for(int index=0;index<count;++index) {
        const auto frame=reader.read();
        if(frame.isNull() || frame.size()!=size)return fail(QStringLiteral("The GIF contains an unreadable or inconsistent frame."));
        decoded+=frame.sizeInBytes();if(decoded>4*1024*1024)return fail(QStringLiteral("GIF decoded frames exceed the 4 MiB animation budget."));
        const int delay=count==1?100:reader.nextImageDelay();
        if(delay<20 || delay>1000)return fail(QStringLiteral("GIF frame delays must be 20–1000 milliseconds."));
        duration+=delay;if(duration>30000)return fail(QStringLiteral("One GIF loop must last at most 30 seconds."));
        delays.append(delay);
        if(includeFrames) {
            QByteArray png;QBuffer output(&png);output.open(QIODevice::WriteOnly);
            if(!frame.save(&output,"PNG"))return fail(QStringLiteral("Could not prepare a GIF frame."));
            encoded+=png.size();if(encoded>2*1024*1024)return fail(QStringLiteral("GIF playback frames exceed the 2 MiB cache budget."));
            frames.append(QStringLiteral("data:image/png;base64,")+QString::fromLatin1(png.toBase64()));
        }
        if(elapsed.elapsed()>250)return fail(QStringLiteral("GIF decoding exceeded the 250 millisecond work budget."));
    }
    return {{"width",size.width()},{"height",size.height()},{"frameCount",count},{"duration",duration},{"loops",loops},{"delays",delays},{"frames",frames}};
}
