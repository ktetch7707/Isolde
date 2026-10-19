#include "settings/AmbientSettings.h"
#include "settings/ProfileManager.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QColor>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>

AmbientSettings::AmbientSettings(QObject *parent) : QObject(parent), m_settingsGroup(ProfileManager::ambientGroup())
{
    load();
}

bool AmbientSettings::enabled() const { return m_enabled; }
int AmbientSettings::timeoutMinutes() const { return m_timeoutMinutes; }
QString AmbientSettings::mode() const { return m_mode; }
int AmbientSettings::slideshowSeconds() const { return m_slideshowSeconds; }
bool AmbientSettings::showClock() const { return m_showClock; }
bool AmbientSettings::showDate() const { return m_showDate; }
bool AmbientSettings::showSystemHint() const { return m_showSystemHint; }
QString AmbientSettings::photoDirectory() const { return m_photoDirectory; }
QVariantList AmbientSettings::photoUrls() const { return m_photoUrls; }
int AmbientSettings::photoCount() const { return m_photoUrls.size(); }
QString AmbientSettings::imageFit() const { return m_imageFit; }
bool AmbientSettings::shuffle() const { return m_shuffle; }
QString AmbientSettings::errorString() const { return m_errorString; }

void AmbientSettings::toggleEnabled()
{
    m_enabled = !m_enabled;
    save();
    emit changed();
}

void AmbientSettings::adjustTimeout(int delta)
{
    const auto next = clamp(m_timeoutMinutes + delta, 1, 120);
    if (next == m_timeoutMinutes)
        return;
    m_timeoutMinutes = next;
    save();
    emit changed();
}

void AmbientSettings::nextMode()
{
    static const QStringList modes{QStringLiteral("slideshow"), QStringLiteral("gradient"), QStringLiteral("dim")};
    const auto next = (modes.indexOf(m_mode) + 1) % modes.size();
    m_mode = modes.at(next);
    save();
    emit changed();
}

void AmbientSettings::adjustSlideshowSeconds(int delta)
{
    const auto next = clamp(m_slideshowSeconds + delta, 5, 300);
    if (next == m_slideshowSeconds)
        return;
    m_slideshowSeconds = next;
    save();
    emit changed();
}

void AmbientSettings::toggleClock()
{
    m_showClock = !m_showClock;
    save();
    emit changed();
}

void AmbientSettings::toggleDate()
{
    m_showDate = !m_showDate;
    save();
    emit changed();
}

void AmbientSettings::toggleSystemHint()
{
    m_showSystemHint = !m_showSystemHint;
    save();
    emit changed();
}

void AmbientSettings::useManagedPhotoDirectory()
{
    setPhotoDirectory(defaultPhotoDirectory());
}

bool AmbientSettings::setPhotoDirectory(const QString &path)
{
    const QFileInfo info(path.trimmed());
    if (!info.exists() || !info.isDir()) {
        m_errorString = tr("Photo folder does not exist.");
        emit changed();
        return false;
    }
    const auto canonical = info.canonicalFilePath();
    if (canonical.isEmpty()) {
        m_errorString = tr("Photo folder could not be resolved.");
        emit changed();
        return false;
    }
    if (m_photoDirectory == canonical)
        return true;
    m_photoDirectory = canonical;
    m_errorString.clear();
    scanPhotos();
    save();
    emit changed();
    return true;
}

bool AmbientSettings::importSamplePhotos()
{
    if (m_photoDirectory.isEmpty())
        m_photoDirectory = defaultPhotoDirectory();
    if (!QDir().mkpath(m_photoDirectory)) {
        m_errorString = tr("Unable to create ambient photo folder.");
        emit changed();
        return false;
    }

    const QVector<QPair<QString, QPair<QColor, QColor>>> samples{
        {QStringLiteral("aurora.png"), {QColor(QStringLiteral("#0A142D")), QColor(QStringLiteral("#438CB4"))}},
        {QStringLiteral("ember.png"), {QColor(QStringLiteral("#37140C")), QColor(QStringLiteral("#BE5F2D"))}},
        {QStringLiteral("meadow.png"), {QColor(QStringLiteral("#08251E")), QColor(QStringLiteral("#5AA87A"))}}
    };
    for (const auto &sample : samples) {
        QImage image(1280, 720, QImage::Format_RGB32);
        for (int y = 0; y < image.height(); ++y) {
            const auto t = static_cast<double>(y) / static_cast<double>(image.height() - 1);
            const auto r = static_cast<int>(sample.second.first.red() * (1.0 - t) + sample.second.second.red() * t);
            const auto g = static_cast<int>(sample.second.first.green() * (1.0 - t) + sample.second.second.green() * t);
            const auto b = static_cast<int>(sample.second.first.blue() * (1.0 - t) + sample.second.second.blue() * t);
            for (int x = 0; x < image.width(); ++x)
                image.setPixelColor(x, y, QColor(r, g, b));
        }
        if (!image.save(QDir(m_photoDirectory).filePath(sample.first), "PNG")) {
            m_errorString = tr("Unable to write sample ambient photo.");
            emit changed();
            return false;
        }
    }
    m_errorString.clear();
    scanPhotos();
    save();
    emit changed();
    return true;
}

void AmbientSettings::rescanPhotos()
{
    m_errorString.clear();
    scanPhotos();
    emit changed();
}

void AmbientSettings::nextImageFit()
{
    static const QStringList modes{QStringLiteral("fill"), QStringLiteral("fit"), QStringLiteral("stretch")};
    const auto next = (modes.indexOf(m_imageFit) + 1) % modes.size();
    m_imageFit = modes.at(next);
    save();
    emit changed();
}

void AmbientSettings::toggleShuffle()
{
    m_shuffle = !m_shuffle;
    save();
    emit changed();
}

void AmbientSettings::reset()
{
    m_enabled = true;
    m_timeoutMinutes = 5;
    m_mode = QStringLiteral("slideshow");
    m_slideshowSeconds = 15;
    m_showClock = true;
    m_showDate = true;
    m_showSystemHint = true;
    m_photoDirectory = defaultPhotoDirectory();
    m_imageFit = QStringLiteral("fill");
    m_shuffle = false;
    m_errorString.clear();
    QDir().mkpath(m_photoDirectory);
    scanPhotos();
    save();
    emit changed();
}

void AmbientSettings::load()
{
    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    m_enabled = settings.value(QStringLiteral("enabled"), m_enabled).toBool();
    m_timeoutMinutes = clamp(settings.value(QStringLiteral("timeoutMinutes"), m_timeoutMinutes).toInt(), 1, 120);
    const auto mode = settings.value(QStringLiteral("mode"), m_mode).toString();
    m_mode = (mode == QStringLiteral("gradient") || mode == QStringLiteral("dim")) ? mode : QStringLiteral("slideshow");
    m_slideshowSeconds = clamp(settings.value(QStringLiteral("slideshowSeconds"), m_slideshowSeconds).toInt(), 5, 300);
    m_showClock = settings.value(QStringLiteral("showClock"), m_showClock).toBool();
    m_showDate = settings.value(QStringLiteral("showDate"), m_showDate).toBool();
    m_showSystemHint = settings.value(QStringLiteral("showSystemHint"), m_showSystemHint).toBool();
    m_photoDirectory = settings.value(QStringLiteral("photoDirectory"), defaultPhotoDirectory()).toString();
    const auto fit = settings.value(QStringLiteral("imageFit"), m_imageFit).toString();
    m_imageFit = (fit == QStringLiteral("fit") || fit == QStringLiteral("stretch")) ? fit : QStringLiteral("fill");
    m_shuffle = settings.value(QStringLiteral("shuffle"), m_shuffle).toBool();
    QDir().mkpath(m_photoDirectory);
    scanPhotos();
}

void AmbientSettings::save() const
{
    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    settings.setValue(QStringLiteral("enabled"), m_enabled);
    settings.setValue(QStringLiteral("timeoutMinutes"), m_timeoutMinutes);
    settings.setValue(QStringLiteral("mode"), m_mode);
    settings.setValue(QStringLiteral("slideshowSeconds"), m_slideshowSeconds);
    settings.setValue(QStringLiteral("showClock"), m_showClock);
    settings.setValue(QStringLiteral("showDate"), m_showDate);
    settings.setValue(QStringLiteral("showSystemHint"), m_showSystemHint);
    settings.setValue(QStringLiteral("photoDirectory"), m_photoDirectory);
    settings.setValue(QStringLiteral("imageFit"), m_imageFit);
    settings.setValue(QStringLiteral("shuffle"), m_shuffle);
}

void AmbientSettings::scanPhotos()
{
    QVariantList urls;
    const QDir directory(m_photoDirectory);
    const auto files = directory.entryInfoList({QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.webp")},
                                               QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    for (const auto &file : files)
        urls.append(QUrl::fromLocalFile(file.absoluteFilePath()).toString());
    m_photoUrls = urls;
}

QString AmbientSettings::defaultPhotoDirectory() const
{
    if (m_settingsGroup.startsWith("profiles/"))
        return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .filePath(m_settingsGroup + "/photos");
    auto pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (pictures.isEmpty())
        pictures = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(pictures).filePath(QStringLiteral("Isolde Ambient"));
}

int AmbientSettings::clamp(int value, int minimum, int maximum)
{
    return qBound(minimum, value, maximum);
}
