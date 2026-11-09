#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>
#include <QVector>

class SubtitleService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY changed)
    Q_PROPERTY(QVariantList languagePacks READ languagePacks NOTIFY changed)
    Q_PROPERTY(bool languagePackBusy READ languagePackBusy NOTIFY changed)
    Q_PROPERTY(QString languagePackStatus READ languagePackStatus NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)

public:
    explicit SubtitleService(QObject *parent = nullptr, const QString &cacheRoot = {},
                             const QString &localPythonExecutable = {});
    ~SubtitleService() override;

    QVariantList jobs() const;
    QVariantList languagePacks() const { return m_languagePacks; }
    bool languagePackBusy() const { return m_languageProcess.state() != QProcess::NotRunning; }
    QString languagePackStatus() const { return m_languagePackStatus; }
    QString status() const { return m_status; }
    QString errorString() const { return m_error; }

    Q_INVOKABLE QString queueTranslation(const QString &sourceSrt, const QString &sourceLanguage,
                                         const QString &targetLanguage);
    Q_INVOKABLE QString queueGeneration(const QString &mediaFile, const QString &localWhisperModel);
    Q_INVOKABLE void cancel(const QString &jobId);
    Q_INVOKABLE bool forgetJob(const QString &jobId);
    Q_INVOKABLE QString reviewText(const QString &jobId);
    Q_INVOKABLE bool saveReview(const QString &jobId, const QString &srtText);
    Q_INVOKABLE void refreshLanguagePacks();
    Q_INVOKABLE bool installLanguagePack(const QString &localPackagePath);
    Q_INVOKABLE QString importOnlineSubtitle(const QString &title, const QString &language,
                                             const QString &srtText, int repositoryFileId);

signals:
    void changed();

private:
    struct Cue { QString header; QString text; };
    struct Job {
        QString id, mode, source, sourceLanguage, targetLanguage, output, error, provenance, modelPath;
        QString sourceHash, stagingDirectory;
        QVariantMap qualityReport;
        QVector<Cue> cues;
        QStringList translatedCues;
        qsizetype translatedBytes = 0;
        int nextCue = 0;
        bool cancelled = false;
        QString state = QStringLiteral("Queued");
    };

    static bool parseSrt(const QString &text, QVector<Cue> *cues, QString *error);
    static QString serializeSrt(const QVector<Cue> &cues, const QStringList *translated = nullptr);
    static bool validLanguage(const QString &language);
    int jobIndex(const QString &id) const;
    QString outputPath(const QString &sourcePath, const QString &targetLanguage, bool forceManagedCache = false) const;
    bool writeOutput(Job &job, const QString &srtText, QString *error) const;
    QVariantMap evaluateQuality(const Job &job, const QVector<Cue> &outputCues) const;
    void startNext();
    void startTranslationCue();
    void startGeneration();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void failCurrent(const QString &message);
    void finishCurrent(const QString &srtText);
    void cleanStaging(const Job &job) const;
    void finishLanguagePackProcess(int exitCode, QProcess::ExitStatus exitStatus);

    QString m_cacheRoot, m_pythonExecutable;
    QString m_status = QStringLiteral("Offline subtitle tools are idle."), m_error;
    QVector<Job> m_jobs;
    QProcess m_process;
    QProcess m_languageProcess;
    QTimer m_processTimeout;
    QTimer m_languagePackTimeout;
    int m_active = -1;
    bool m_killPending = false;
    bool m_timeoutPending = false;
    bool m_languagePackTimedOut = false;
    QString m_languagePackAction;
    QString m_languagePackStatus = QStringLiteral("Local Argos language packs have not been checked.");
    QVariantList m_languagePacks;
};
