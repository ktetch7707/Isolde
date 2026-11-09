#include "media/SubtitleService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <utility>

namespace {
constexpr qsizetype MaxSubtitleBytes = 8 * 1024 * 1024;
constexpr int MaxSubtitleCues = 10000;
constexpr qsizetype MaxCueTextBytes = 16384;
constexpr int MaxQueuedJobs = 16;

QString sha256File(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const auto bytes = file.read(1024 * 1024);
        if (bytes.isEmpty() && file.error() != QFile::NoError) return {};
        hash.addData(bytes);
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString safeError(const QString &text)
{
    return text.trimmed().isEmpty() ? QStringLiteral("The offline subtitle tool failed without a diagnostic.") : text.trimmed();
}
}

SubtitleService::SubtitleService(QObject *parent, const QString &cacheRoot)
    : QObject(parent), m_cacheRoot(cacheRoot.isEmpty()
          ? (QStandardPaths::writableLocation(QStandardPaths::CacheLocation).isEmpty()
                ? QString() : QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).filePath(QStringLiteral("subtitles")))
          : cacheRoot)
{
    m_processTimeout.setSingleShot(true);
    connect(&m_processTimeout, &QTimer::timeout, this, [this] {
        if (m_process.state() != QProcess::NotRunning) {
            m_timeoutPending = true;
            const auto jobId = m_active >= 0 ? m_jobs.at(m_active).id : QString();
            m_process.terminate();
            QTimer::singleShot(1000, this, [this, jobId] {
                if (m_active >= 0 && m_jobs.at(m_active).id == jobId &&
                    m_process.state() != QProcess::NotRunning)
                    m_process.kill();
            });
        }
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &SubtitleService::processFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (m_active < 0 || error != QProcess::FailedToStart) return;
        failCurrent(QStringLiteral("Required local subtitle tool could not start: %1").arg(m_process.errorString()));
    });
}

SubtitleService::~SubtitleService()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
    for (const auto &job : std::as_const(m_jobs)) cleanStaging(job);
}

QVariantList SubtitleService::jobs() const
{
    QVariantList result;
    for (const auto &job : m_jobs) {
        const int percent = job.mode == QStringLiteral("translate") && !job.cues.isEmpty()
            ? (job.state == QStringLiteral("Ready for review") ? 100 : job.nextCue * 100 / job.cues.size())
            : job.state == QStringLiteral("Ready for review") ? 100 : 0;
        result.append(QVariantMap{
            {QStringLiteral("id"), job.id},
            {QStringLiteral("mode"), job.mode},
            {QStringLiteral("source"), job.source},
            {QStringLiteral("sourceLanguage"), job.sourceLanguage},
            {QStringLiteral("targetLanguage"), job.targetLanguage},
            {QStringLiteral("output"), job.output},
            {QStringLiteral("state"), job.state},
            {QStringLiteral("progress"), percent},
            {QStringLiteral("error"), job.error},
            {QStringLiteral("provenance"), job.provenance}
        });
    }
    return result;
}

bool SubtitleService::validLanguage(const QString &language)
{
    static const QRegularExpression languagePattern(QStringLiteral("^[a-z]{2,3}([_-][A-Za-z0-9]{2,8})?$"));
    return languagePattern.match(language.trimmed()).hasMatch();
}

bool SubtitleService::parseSrt(const QString &source, QVector<Cue> *cues, QString *error)
{
    if (!cues || source.toUtf8().size() > MaxSubtitleBytes) {
        if (error) *error = QStringLiteral("Subtitle file exceeds the 8 MiB limit.");
        return false;
    }
    QString normalized = source;
    if (normalized.startsWith(QChar(0xfeff))) normalized.remove(0, 1);
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace('\r', '\n');
    const auto blocks = normalized.split(QRegularExpression(QStringLiteral("\n[ \t]*\n+")), Qt::SkipEmptyParts);
    if (blocks.isEmpty() || blocks.size() > MaxSubtitleCues) {
        if (error) *error = QStringLiteral("Subtitle must contain between 1 and 10,000 cues.");
        return false;
    }
    static const QRegularExpression blockPattern(
        QStringLiteral("^(\\d{1,8})\\n((?:\\d{2,}:)?\\d{2}:\\d{2},\\d{3} --> (?:\\d{2,}:)?\\d{2}:\\d{2},\\d{3})(?:\\n([\\s\\S]*))?$"));
    QVector<Cue> parsed;
    parsed.reserve(blocks.size());
    for (const auto &block : blocks) {
        const auto match = blockPattern.match(block.trimmed());
        if (!match.hasMatch()) {
            if (error) *error = QStringLiteral("Subtitle contains a malformed cue or timestamp.");
            return false;
        }
        const auto body = match.captured(3);
        if (body.toUtf8().size() > MaxCueTextBytes) {
            if (error) *error = QStringLiteral("A subtitle cue exceeds the 16 KiB limit.");
            return false;
        }
        parsed.append({match.captured(1) + QLatin1Char('\n') + match.captured(2), body});
    }
    *cues = parsed;
    return true;
}

QString SubtitleService::serializeSrt(const QVector<Cue> &cues, const QStringList *translated)
{
    QStringList blocks;
    blocks.reserve(cues.size());
    for (qsizetype i = 0; i < cues.size(); ++i) {
        const auto &cue = cues.at(i);
        const auto body = translated && i < translated->size() ? translated->at(i) : cue.text;
        blocks.append(cue.header + (body.isEmpty() ? QString() : QLatin1Char('\n') + body));
    }
    return blocks.join(QStringLiteral("\n\n")) + QLatin1Char('\n');
}

int SubtitleService::jobIndex(const QString &id) const
{
    for (qsizetype i = 0; i < m_jobs.size(); ++i)
        if (m_jobs.at(i).id == id) return static_cast<int>(i);
    return -1;
}

QString SubtitleService::outputPath(const QString &sourcePath, const QString &targetLanguage, bool forceManagedCache) const
{
    const QFileInfo source(sourcePath);
    QString base = source.completeBaseName();
    const auto languageSuffix = QRegularExpression(QStringLiteral("\\.[a-z]{2,3}(?:[-_][A-Za-z0-9]{2,8})?$"),
                                                    QRegularExpression::CaseInsensitiveOption);
    base.remove(languageSuffix);
    const QString filename = base + QLatin1Char('.') + targetLanguage + QStringLiteral(".srt");
    QString directory = source.absolutePath();
    if (forceManagedCache || !QFileInfo(directory).isDir() || !QFileInfo(directory).isWritable()) {
        if (m_cacheRoot.isEmpty()) return {};
        const auto hash = QString::fromLatin1(QCryptographicHash::hash(
            source.absoluteFilePath().toUtf8(), QCryptographicHash::Sha256).toHex());
        directory = QDir(m_cacheRoot).filePath(hash);
    }
    if (!QDir().mkpath(directory)) return {};
    QString candidate = QDir(directory).filePath(filename);
    for (int version = 1; QFileInfo::exists(candidate) || QFileInfo::exists(candidate + QStringLiteral(".isolde.json")); ++version) {
        candidate = QDir(directory).filePath(base + QLatin1Char('.') + targetLanguage +
                                              QStringLiteral(".isolde-%1.srt").arg(version));
        if (version > 10000) return {};
    }
    return candidate;
}

bool SubtitleService::writeOutput(Job &job, const QString &srtText, QString *error) const
{
    if (job.output.isEmpty() || QFileInfo::exists(job.output)) {
        if (error) *error = QStringLiteral("A subtitle already exists at the selected output path; no file was overwritten.");
        return false;
    }
    const QFileInfo outputInfo(job.output);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        if (error) *error = QStringLiteral("Could not create the subtitle output directory.");
        return false;
    }
    QSaveFile subtitle(job.output);
    if (!subtitle.open(QIODevice::WriteOnly) || subtitle.write(srtText.toUtf8()) != srtText.toUtf8().size() ||
        !subtitle.commit()) {
        if (error) *error = QStringLiteral("Could not save the subtitle atomically: %1").arg(subtitle.errorString());
        return false;
    }
    const auto metadataPath = job.output + QStringLiteral(".isolde.json");
    const QJsonObject provenance{
        {QStringLiteral("format"), 1},
        {QStringLiteral("tool"), job.provenance},
        {QStringLiteral("source"), job.source},
        {QStringLiteral("sourceSha256"), job.sourceHash},
        {QStringLiteral("sourceLanguage"), job.sourceLanguage},
        {QStringLiteral("targetLanguage"), job.targetLanguage},
        {QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("offlineOnly"), true},
        {QStringLiteral("reviewRequired"), true}
    };
    QSaveFile sidecar(metadataPath);
    const auto bytes = QJsonDocument(provenance).toJson(QJsonDocument::Indented);
    if (!sidecar.open(QIODevice::WriteOnly) || sidecar.write(bytes) != bytes.size() || !sidecar.commit()) {
        QFile::remove(job.output);
        if (error) *error = QStringLiteral("Could not persist subtitle provenance; generated output was rolled back.");
        return false;
    }
    return true;
}

QString SubtitleService::queueTranslation(const QString &sourceSrt, const QString &sourceLanguage,
                                          const QString &targetLanguage)
{
    if (m_jobs.size() >= MaxQueuedJobs) {
        m_error = QStringLiteral("Subtitle queue is full (maximum 16 explicit jobs). Complete or cancel a job before adding another.");
        emit changed();
        return {};
    }
    const auto source = QFileInfo(sourceSrt).absoluteFilePath();
    if (!QFileInfo(source).isFile() || !validLanguage(sourceLanguage) || !validLanguage(targetLanguage) ||
        sourceLanguage.compare(targetLanguage, Qt::CaseInsensitive) == 0) {
        m_error = QStringLiteral("Choose an existing SRT and distinct valid source and target language codes.");
        emit changed();
        return {};
    }
    QFile file(source);
    if (!file.open(QIODevice::ReadOnly) || file.size() > MaxSubtitleBytes) {
        m_error = QStringLiteral("Could not read the SRT or it exceeds the 8 MiB limit.");
        emit changed();
        return {};
    }
    const auto bytes = file.readAll();
    const auto decoded = QString::fromUtf8(bytes);
    QVector<Cue> cues;
    if (bytes.size() > MaxSubtitleBytes || decoded.toUtf8() != bytes || !parseSrt(decoded, &cues, &m_error)) {
        if (m_error.isEmpty()) m_error = QStringLiteral("Could not read a valid UTF-8 SRT.");
        emit changed();
        return {};
    }
    Job job;
    job.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    job.mode = QStringLiteral("translate");
    job.source = source;
    job.sourceLanguage = sourceLanguage.trimmed().toLower();
    job.targetLanguage = targetLanguage.trimmed().toLower();
    job.output = outputPath(source, job.targetLanguage);
    job.cues = cues;
    job.translatedCues.reserve(cues.size());
    job.sourceHash = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    job.provenance = QStringLiteral("Argos Translate CLI; local installed language package; human review required");
    if (job.output.isEmpty()) {
        m_error = QStringLiteral("Could not select a safe output path.");
        emit changed();
        return {};
    }
    m_jobs.append(job);
    m_error.clear();
    m_status = QStringLiteral("Translation queued. Requires an already-installed local Argos language package; no download will be attempted.");
    emit changed();
    startNext();
    return job.id;
}

QString SubtitleService::queueGeneration(const QString &mediaFile, const QString &localWhisperModel)
{
    if (m_jobs.size() >= MaxQueuedJobs) {
        m_error = QStringLiteral("Subtitle queue is full (maximum 16 explicit jobs). Complete or cancel a job before adding another.");
        emit changed();
        return {};
    }
    const auto source = QFileInfo(mediaFile).absoluteFilePath();
    const QFileInfo model(localWhisperModel);
    if (!QFileInfo(source).isFile() || !model.isFile() ||
        model.suffix().compare(QStringLiteral("bin"), Qt::CaseInsensitive) != 0) {
        m_error = QStringLiteral("Choose an existing media file and an installed whisper.cpp .bin model file. Model downloads are never started.");
        emit changed();
        return {};
    }
    Job job;
    job.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    job.mode = QStringLiteral("generate");
    job.source = source;
    job.sourceLanguage = QStringLiteral("auto-detected");
    job.targetLanguage = QStringLiteral("en");
    job.output = outputPath(source, job.targetLanguage);
    job.sourceHash = sha256File(source);
    job.provenance = QStringLiteral("whisper.cpp CLI, local preinstalled multilingual model, translate task; human review required");
    job.modelPath = model.absoluteFilePath();
    if (job.sourceHash.isEmpty()) {
        m_error = QStringLiteral("Could not read the selected media file for provenance.");
        emit changed();
        return {};
    }
    if (job.output.isEmpty()) {
        m_error = QStringLiteral("Could not select a safe output path.");
        emit changed();
        return {};
    }
    m_jobs.append(job);
    m_error.clear();
    m_status = QStringLiteral("Generation queued. Whisper and the specified model must already be installed locally; no download will be attempted.");
    emit changed();
    startNext();
    return job.id;
}

void SubtitleService::startNext()
{
    if (m_active >= 0 || m_process.state() != QProcess::NotRunning) return;
    for (int i = 0; i < m_jobs.size(); ++i) {
        if (m_jobs[i].state != QStringLiteral("Queued")) continue;
        m_active = i;
        m_jobs[i].state = QStringLiteral("Working");
        m_error.clear();
        if (m_jobs[i].mode == QStringLiteral("translate")) startTranslationCue();
        else startGeneration();
        emit changed();
        return;
    }
}

void SubtitleService::startTranslationCue()
{
    if (m_active < 0) return;
    auto &job = m_jobs[m_active];
    if (job.cancelled) {
        job.state = QStringLiteral("Cancelled");
        m_active = -1;
        emit changed();
        startNext();
        return;
    }
    if (job.nextCue >= job.cues.size()) {
        finishCurrent(serializeSrt(job.cues, &job.translatedCues));
        return;
    }
    m_status = QStringLiteral("Translating cue %1 of %2 offline…").arg(job.nextCue + 1).arg(job.cues.size());
    m_process.setProgram(QStringLiteral("argos-translate"));
    m_process.setArguments({
        QStringLiteral("--from-lang"), job.sourceLanguage,
        QStringLiteral("--to-lang"), job.targetLanguage,
        QStringLiteral("--text"), job.cues.at(job.nextCue).text
    });
    m_processTimeout.start(30000);
    m_process.start();
}

void SubtitleService::startGeneration()
{
    if (m_active < 0) return;
    auto &job = m_jobs[m_active];
    const QFileInfo model(job.modelPath);
    if (!model.isFile() || !job.modelPath.endsWith(QStringLiteral(".bin"), Qt::CaseInsensitive) ||
        model.completeBaseName().endsWith(QStringLiteral(".en"), Qt::CaseInsensitive)) {
        failCurrent(QStringLiteral("Choose an existing multilingual whisper.cpp model, not an English-only model."));
        return;
    }
    const auto executable = QStandardPaths::findExecutable(QStringLiteral("whisper-cli"));
    if (executable.isEmpty()) {
        failCurrent(QStringLiteral("whisper.cpp CLI is not installed. No network request or model download was attempted."));
        return;
    }
    if (!QDir().mkpath(m_cacheRoot)) {
        failCurrent(QStringLiteral("Could not create the managed local subtitle cache."));
        return;
    }
    job.stagingDirectory = QDir(m_cacheRoot).filePath(QStringLiteral(".staging-%1").arg(job.id));
    if (!QDir().mkpath(job.stagingDirectory)) {
        failCurrent(QStringLiteral("Could not create a private subtitle staging directory."));
        return;
    }
    const auto outputBase = QDir(job.stagingDirectory).filePath(QFileInfo(job.source).completeBaseName());
    m_status = QStringLiteral("Generating English subtitles locally with the selected installed whisper.cpp model…");
    m_process.setProgram(executable);
    m_process.setArguments({
        QStringLiteral("-m"), job.modelPath,
        QStringLiteral("-f"), job.source,
        QStringLiteral("-tr"),
        QStringLiteral("-osrt"),
        QStringLiteral("-of"), outputBase
    });
    m_processTimeout.start(6 * 60 * 60 * 1000);
    m_process.start();
}

void SubtitleService::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_processTimeout.stop();
    if (m_active < 0) return;
    auto &job = m_jobs[m_active];
    const bool timedOut = m_timeoutPending;
    m_timeoutPending = false;
    if (job.cancelled || m_killPending) {
        m_killPending = false;
        job.state = QStringLiteral("Cancelled");
        job.error.clear();
        job.cues.clear();
        job.translatedCues.clear();
        cleanStaging(job);
        m_active = -1;
        m_status = QStringLiteral("Subtitle job cancelled. Completed outputs were retained.");
        emit changed();
        startNext();
        return;
    }
    if (timedOut) {
        failCurrent(QStringLiteral("Local subtitle tool timed out; source and completed outputs were left unchanged."));
        return;
    }
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        failCurrent(QStringLiteral("Local subtitle tool returned exit code %1: %2")
                    .arg(exitCode).arg(safeError(QString::fromUtf8(m_process.readAllStandardError()))));
        return;
    }
    if (job.mode == QStringLiteral("translate")) {
        auto text = QString::fromUtf8(m_process.readAllStandardOutput());
        while (text.endsWith(QLatin1Char('\n')) || text.endsWith(QLatin1Char('\r'))) text.chop(1);
        if (text.isEmpty() || text.toUtf8().size() > MaxCueTextBytes) {
            failCurrent(QStringLiteral("Local translation returned an empty or oversized cue."));
            return;
        }
        job.translatedBytes += text.toUtf8().size();
        if (job.translatedBytes > MaxSubtitleBytes) {
            failCurrent(QStringLiteral("Local translation exceeded the 8 MiB subtitle output limit."));
            return;
        }
        job.translatedCues.append(text);
        ++job.nextCue;
        startTranslationCue();
        emit changed();
        return;
    }
    const auto stagedPath = QDir(job.stagingDirectory).filePath(
        QFileInfo(job.source).completeBaseName() + QStringLiteral(".srt"));
    QFile generated(stagedPath);
    if (!generated.open(QIODevice::ReadOnly) || generated.size() > MaxSubtitleBytes) {
        failCurrent(QStringLiteral("Whisper completed but did not produce a readable SRT within the 8 MiB limit."));
        return;
    }
    const auto bytes = generated.readAll();
    const auto decoded = QString::fromUtf8(bytes);
    QVector<Cue> cues;
    QString error;
    if (decoded.toUtf8() != bytes || !parseSrt(decoded, &cues, &error)) {
        failCurrent(QStringLiteral("Whisper output was not a valid UTF-8 SRT: %1").arg(error));
        return;
    }
    finishCurrent(serializeSrt(cues));
}

void SubtitleService::failCurrent(const QString &message)
{
    if (m_active < 0) return;
    auto &job = m_jobs[m_active];
    job.state = QStringLiteral("Failed");
    job.error = message;
    job.cues.clear();
    job.translatedCues.clear();
    m_error = message;
    m_status = QStringLiteral("Subtitle job failed; source files were not changed.");
    cleanStaging(job);
    m_active = -1;
    emit changed();
    startNext();
}

void SubtitleService::finishCurrent(const QString &srtText)
{
    if (m_active < 0) return;
    auto &job = m_jobs[m_active];
    QVector<Cue> validatedCues;
    QString validationError;
    if (!parseSrt(srtText, &validatedCues, &validationError)) {
        failCurrent(QStringLiteral("Generated output is not a valid bounded SRT: %1").arg(validationError));
        return;
    }
    QString error;
    if (!writeOutput(job, srtText, &error)) {
        const auto originalOutput = job.output;
        const bool sourceDirectory = QFileInfo(originalOutput).absolutePath() == QFileInfo(job.source).absolutePath();
        if (sourceDirectory && !m_cacheRoot.isEmpty() && !error.contains(QStringLiteral("already exists"))) {
            const auto cachedOutput = outputPath(job.source, job.targetLanguage, true);
            job.output = cachedOutput;
            QString cacheError;
            if (!cachedOutput.isEmpty() && writeOutput(job, srtText, &cacheError)) {
                m_status = QStringLiteral("Media folder could not save the subtitle; output and provenance were saved in Isolde's managed cache.");
            } else {
                job.output = originalOutput;
                failCurrent(cacheError.isEmpty() ? error : cacheError);
                return;
            }
        } else {
            failCurrent(error);
            return;
        }
    }
    job.state = QStringLiteral("Ready for review");
    job.error.clear();
    job.cues.clear();
    job.translatedCues.clear();
    if (!m_status.startsWith(QStringLiteral("Media folder could not save")))
        m_status = QStringLiteral("Subtitle saved locally. Review it before loading it in Kodi.");
    cleanStaging(job);
    m_active = -1;
    emit changed();
    startNext();
}

void SubtitleService::cancel(const QString &jobId)
{
    const int index = jobIndex(jobId);
    if (index < 0 || m_jobs.at(index).state == QStringLiteral("Ready for review") ||
        m_jobs.at(index).state == QStringLiteral("Failed") ||
        m_jobs.at(index).state == QStringLiteral("Cancelled")) return;
    auto &job = m_jobs[index];
    job.cancelled = true;
    if (index == m_active) {
        m_killPending = true;
        const auto activeJobId = job.id;
        m_process.terminate();
        QTimer::singleShot(750, this, [this, index, activeJobId] {
            if (m_active == index && m_jobs.at(index).id == activeJobId &&
                m_process.state() != QProcess::NotRunning)
                m_process.kill();
        });
    } else {
        job.state = QStringLiteral("Cancelled");
        job.error.clear();
        job.cues.clear();
        job.translatedCues.clear();
    }
    m_status = QStringLiteral("Cancelling subtitle job…");
    emit changed();
}

bool SubtitleService::forgetJob(const QString &jobId)
{
    const int index = jobIndex(jobId);
    if (index < 0 || index == m_active ||
        (m_jobs.at(index).state != QStringLiteral("Ready for review") &&
         m_jobs.at(index).state != QStringLiteral("Failed") &&
         m_jobs.at(index).state != QStringLiteral("Cancelled"))) {
        m_error = QStringLiteral("Only a finished or cancelled job can be removed from the queue.");
        emit changed();
        return false;
    }
    cleanStaging(m_jobs.at(index));
    m_jobs.removeAt(index);
    if (m_active > index) --m_active;
    m_error.clear();
    m_status = QStringLiteral("Job record removed. Saved subtitle output and provenance were retained.");
    emit changed();
    return true;
}

QString SubtitleService::reviewText(const QString &jobId)
{
    const int index = jobIndex(jobId);
    if (index < 0 || m_jobs.at(index).state != QStringLiteral("Ready for review")) {
        m_error = QStringLiteral("Only a completed subtitle can be opened for review.");
        emit changed();
        return {};
    }
    QFile file(m_jobs.at(index).output);
    if (!file.open(QIODevice::ReadOnly) || file.size() > MaxSubtitleBytes) {
        m_error = QStringLiteral("Could not read the generated subtitle for review.");
        emit changed();
        return {};
    }
    m_error.clear();
    return QString::fromUtf8(file.readAll());
}

bool SubtitleService::saveReview(const QString &jobId, const QString &srtText)
{
    const int index = jobIndex(jobId);
    if (index < 0 || m_jobs.at(index).state != QStringLiteral("Ready for review")) {
        m_error = QStringLiteral("Only a completed subtitle can be reviewed and edited.");
        emit changed();
        return false;
    }
    QVector<Cue> cues;
    if (!parseSrt(srtText, &cues, &m_error)) {
        emit changed();
        return false;
    }
    QSaveFile file(m_jobs[index].output);
    const auto bytes = srtText.toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        m_error = QStringLiteral("Could not save subtitle edits atomically: %1").arg(file.errorString());
        emit changed();
        return false;
    }
    m_error.clear();
    m_status = QStringLiteral("Reviewed subtitle edits saved locally.");
    emit changed();
    return true;
}

void SubtitleService::cleanStaging(const Job &job) const
{
    if (!job.stagingDirectory.isEmpty() && job.stagingDirectory.startsWith(QDir(m_cacheRoot).absolutePath() + QDir::separator()))
        QDir(job.stagingDirectory).removeRecursively();
}
