#pragma once

#include <QObject>
#include <QVariantList>

class AmbientSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(int timeoutMinutes READ timeoutMinutes NOTIFY changed)
    Q_PROPERTY(QString mode READ mode NOTIFY changed)
    Q_PROPERTY(int slideshowSeconds READ slideshowSeconds NOTIFY changed)
    Q_PROPERTY(bool showClock READ showClock NOTIFY changed)
    Q_PROPERTY(bool showDate READ showDate NOTIFY changed)
    Q_PROPERTY(bool showSystemHint READ showSystemHint NOTIFY changed)
    Q_PROPERTY(QString photoDirectory READ photoDirectory NOTIFY changed)
    Q_PROPERTY(QVariantList photoUrls READ photoUrls NOTIFY changed)
    Q_PROPERTY(int photoCount READ photoCount NOTIFY changed)
    Q_PROPERTY(QString imageFit READ imageFit NOTIFY changed)
    Q_PROPERTY(bool shuffle READ shuffle NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)

public:
    explicit AmbientSettings(QObject *parent = nullptr);

    [[nodiscard]] bool enabled() const;
    [[nodiscard]] int timeoutMinutes() const;
    [[nodiscard]] QString mode() const;
    [[nodiscard]] int slideshowSeconds() const;
    [[nodiscard]] bool showClock() const;
    [[nodiscard]] bool showDate() const;
    [[nodiscard]] bool showSystemHint() const;
    [[nodiscard]] QString photoDirectory() const;
    [[nodiscard]] QVariantList photoUrls() const;
    [[nodiscard]] int photoCount() const;
    [[nodiscard]] QString imageFit() const;
    [[nodiscard]] bool shuffle() const;
    [[nodiscard]] QString errorString() const;

    Q_INVOKABLE void toggleEnabled();
    Q_INVOKABLE void adjustTimeout(int delta);
    Q_INVOKABLE void nextMode();
    Q_INVOKABLE void adjustSlideshowSeconds(int delta);
    Q_INVOKABLE void toggleClock();
    Q_INVOKABLE void toggleDate();
    Q_INVOKABLE void toggleSystemHint();
    Q_INVOKABLE void useManagedPhotoDirectory();
    Q_INVOKABLE bool setPhotoDirectory(const QString &path);
    Q_INVOKABLE bool importSamplePhotos();
    Q_INVOKABLE void rescanPhotos();
    Q_INVOKABLE void nextImageFit();
    Q_INVOKABLE void toggleShuffle();
    Q_INVOKABLE void reset();

signals:
    void changed();

private:
    const QString m_settingsGroup;
    void load();
    void save() const;
    void scanPhotos();
    QString defaultPhotoDirectory() const;
    static int clamp(int value, int minimum, int maximum);

    bool m_enabled = true;
    int m_timeoutMinutes = 5;
    QString m_mode = QStringLiteral("slideshow");
    int m_slideshowSeconds = 15;
    bool m_showClock = true;
    bool m_showDate = true;
    bool m_showSystemHint = true;
    QString m_photoDirectory;
    QVariantList m_photoUrls;
    QString m_imageFit = QStringLiteral("fill");
    bool m_shuffle = false;
    QString m_errorString;
};
