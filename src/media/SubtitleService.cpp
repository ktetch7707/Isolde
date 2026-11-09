#include "media/SubtitleService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <algorithm>
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

SubtitleService::SubtitleService(QObject *parent, const QString &cacheRoot, const QString &localPythonExecutable)
    : QObject(parent), m_cacheRoot(cacheRoot.isEmpty()
          ? (QStandardPaths::writableLocation(QStandardPaths::CacheLocation).isEmpty()
                ? QString() : QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).filePath(QStringLiteral("subtitles")))
          : cacheRoot),
      m_pythonExecutable(localPythonExecutable.isEmpty() ? QString() : QFileInfo(localPythonExecutable).absoluteFilePath())
{
    m_processTimeout.setSingleShot(true);
    m_languagePackTimeout.setSingleShot(true);
    connect(&m_languagePackTimeout, &QTimer::timeout, this, [this] {
        if (m_languageProcess.state() == QProcess::NotRunning) return;
        m_languagePackTimedOut = true;
        m_languageProcess.kill();
    });
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
    connect(&m_languageProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &SubtitleService::finishLanguagePackProcess);
    connect(&m_languageProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_languagePackAction.isEmpty()) return;
        m_languagePackTimeout.stop();
        m_languagePackStatus = QStringLiteral("The local Argos language-pack helper could not start: %1")
            .arg(m_languageProcess.errorString());
        m_languagePackAction.clear();
        emit changed();
    });
    refreshLanguagePacks();
}

SubtitleService::~SubtitleService()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
    if (m_languageProcess.state() != QProcess::NotRunning) {
        m_languagePackTimeout.stop();
        m_languageProcess.kill();
        m_languageProcess.waitForFinished(1000);
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
            {QStringLiteral("provenance"), job.provenance},
            {QStringLiteral("qualityReport"), job.qualityReport}
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
    if (job.output.isEmpty() || QFileInfo::exists(job.output) ||
        QFileInfo::exists(job.output + QStringLiteral(".isolde.json"))) {
        if (error) *error = QStringLiteral("A subtitle already exists at the selected output path; no file was overwritten.");
        return false;
    }
    const QFileInfo outputInfo(job.output);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        if (error) *error = QStringLiteral("Could not create the subtitle output directory.");
        return false;
    }
    QFile reservation(job.output + QStringLiteral(".isolde-reserve"));
    if (!reservation.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        if (error) *error = QStringLiteral("Another subtitle operation is using this output path; nothing was overwritten.");
        return false;
    }
    reservation.close();
    [[maybe_unused]] const auto releaseReservation = qScopeGuard([&reservation] { QFile::remove(reservation.fileName()); });
    if (QFileInfo::exists(job.output) || QFileInfo::exists(job.output + QStringLiteral(".isolde.json"))) {
        if (error) *error = QStringLiteral("A subtitle already exists at the selected output path; no file was overwritten.");
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
        {QStringLiteral("offlineOnly"), job.mode != QStringLiteral("download")},
        {QStringLiteral("repository"), job.mode == QStringLiteral("download") ? QStringLiteral("OpenSubtitles.com") : QString()},
        {QStringLiteral("reviewRequired"), true},
        {QStringLiteral("qualityReport"), QJsonObject::fromVariantMap(job.qualityReport)}
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

QVariantMap SubtitleService::evaluateQuality(const Job &job, const QVector<Cue> &outputCues) const
{
    const bool compareTimestamps = job.mode == QStringLiteral("translate");
    int emptyCues = 0;
    for (const auto &cue : outputCues)
        if (cue.text.trimmed().isEmpty()) ++emptyCues;

    int unchangedCues = 0;
    int shortCues = 0;
    bool timestampsPreserved = compareTimestamps && job.cues.size() == outputCues.size();
    if (compareTimestamps) {
        const auto count = std::min(job.cues.size(), outputCues.size());
        for (qsizetype i = 0; i < count; ++i) {
            if (job.cues.at(i).header != outputCues.at(i).header)
                timestampsPreserved = false;
            const auto sourceText = job.cues.at(i).text.simplified();
            const auto targetText = outputCues.at(i).text.simplified();
            if (!sourceText.isEmpty() && sourceText.compare(targetText, Qt::CaseInsensitive) == 0)
                ++unchangedCues;
            if (sourceText.size() >= 8 && targetText.size() < 3)
                ++shortCues;
        }
    }

    QStringList warnings;
    if (emptyCues) warnings.append(QStringLiteral("empty-cues"));
    if (compareTimestamps && !timestampsPreserved)
        warnings.append(QStringLiteral("cue-timing-or-count-changed"));
    if (unchangedCues) warnings.append(QStringLiteral("possible-untranslated-cues"));
    if (shortCues) warnings.append(QStringLiteral("suspiciously-short-cues"));
    const auto unchangedPercent = job.mode == QStringLiteral("translate") && !job.cues.isEmpty()
        ? unchangedCues * 100 / job.cues.size() : 0;
    return {
        {QStringLiteral("assessment"), QStringLiteral("Structural and heuristic checks only; language quality is not certified.")},
        {QStringLiteral("reviewRequired"), true},
        {QStringLiteral("languageQualityCertified"), false},
        {QStringLiteral("sourceCueCount"), job.mode == QStringLiteral("translate") ? job.cues.size() : -1},
        {QStringLiteral("outputCueCount"), outputCues.size()},
        {QStringLiteral("emptyCueCount"), emptyCues},
        {QStringLiteral("unchangedCueCount"), unchangedCues},
        {QStringLiteral("unchangedCuePercent"), unchangedPercent},
        {QStringLiteral("shortOutputCueCount"), shortCues},
        {QStringLiteral("timestampsCompared"), compareTimestamps},
        {QStringLiteral("timestampsPreserved"), timestampsPreserved},
        {QStringLiteral("timestampCheck"), !compareTimestamps ? QStringLiteral("not-applicable")
            : timestampsPreserved ? QStringLiteral("passed") : QStringLiteral("failed")},
        {QStringLiteral("warnings"), warnings}
    };
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
    job.qualityReport = evaluateQuality(job, validatedCues);
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
    QFile originalFile(m_jobs[index].output);
    if (!originalFile.open(QIODevice::ReadOnly) || originalFile.size() > MaxSubtitleBytes) {
        m_error = QStringLiteral("Could not preserve the current subtitle before saving the review.");
        emit changed();
        return false;
    }
    const auto originalBytes = originalFile.readAll();
    originalFile.close();
    QFile provenanceFile(m_jobs[index].output + QStringLiteral(".isolde.json"));
    if (!provenanceFile.open(QIODevice::ReadOnly) || provenanceFile.size() > 64 * 1024) {
        m_error = QStringLiteral("Could not read the subtitle provenance before saving the review.");
        emit changed();
        return false;
    }
    QJsonParseError provenanceError;
    auto provenanceDocument = QJsonDocument::fromJson(provenanceFile.readAll(), &provenanceError);
    provenanceFile.close();
    if (provenanceError.error != QJsonParseError::NoError || !provenanceDocument.isObject()) {
        m_error = QStringLiteral("Subtitle provenance is invalid; the reviewed output was not changed.");
        emit changed();
        return false;
    }
    auto reviewJob = m_jobs[index];
    reviewJob.mode = QStringLiteral("review");
    const auto previousQuality = m_jobs[index].qualityReport;
    auto reviewQuality = evaluateQuality(reviewJob, cues);
    reviewQuality.insert(QStringLiteral("reviewedByUser"), true);
    m_jobs[index].qualityReport = reviewQuality;
    auto provenance = provenanceDocument.object();
    provenance.insert(QStringLiteral("reviewedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    provenance.insert(QStringLiteral("qualityReport"), QJsonObject::fromVariantMap(reviewQuality));
    QSaveFile file(m_jobs[index].output);
    const auto bytes = srtText.toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        m_jobs[index].qualityReport = previousQuality;
        m_error = QStringLiteral("Could not save subtitle edits atomically: %1").arg(file.errorString());
        emit changed();
        return false;
    }
    const auto provenanceBytes = QJsonDocument(provenance).toJson(QJsonDocument::Indented);
    QSaveFile updatedProvenance(m_jobs[index].output + QStringLiteral(".isolde.json"));
    if (!updatedProvenance.open(QIODevice::WriteOnly) ||
        updatedProvenance.write(provenanceBytes) != provenanceBytes.size() ||
        !updatedProvenance.commit()) {
        QSaveFile rollback(m_jobs[index].output);
        const bool restored = rollback.open(QIODevice::WriteOnly) &&
            rollback.write(originalBytes) == originalBytes.size() && rollback.commit();
        m_error = restored
            ? QStringLiteral("Could not persist review provenance; subtitle edits were rolled back.")
            : QStringLiteral("Could not persist review provenance or restore the original subtitle. Check storage before using this file.");
        m_jobs[index].qualityReport = previousQuality;
        emit changed();
        return false;
    }
    m_error.clear();
    m_status = QStringLiteral("Reviewed subtitle edits saved locally.");
    emit changed();
    return true;
}

QString SubtitleService::importOnlineSubtitle(const QString &title, const QString &language,
                                             const QString &srtText, int repositoryFileId)
{
    if (m_jobs.size() >= MaxQueuedJobs) {
        m_error = QStringLiteral("Subtitle queue is full (maximum 16 jobs). Complete or forget a finished job before importing another.");
        emit changed();
        return {};
    }
    m_error.clear();
    const auto normalizedTitle = title.trimmed();
    const auto normalizedLanguage = language.trimmed().toLower();
    const auto bytes = srtText.toUtf8();
    QVector<Cue> cues;
    QString validationError;
    if (normalizedTitle.isEmpty() || normalizedTitle.size() > 200 ||
        !validLanguage(normalizedLanguage) || repositoryFileId <= 0 ||
        bytes.isEmpty() || bytes.size() > MaxSubtitleBytes ||
        QString::fromUtf8(bytes).toUtf8() != bytes || !parseSrt(srtText, &cues, &validationError)) {
        m_error = validationError.isEmpty()
            ? QStringLiteral("Downloaded subtitle metadata or UTF-8 SRT is invalid.")
            : validationError;
        emit changed();
        return {};
    }
    if (m_cacheRoot.isEmpty()) {
        m_error = QStringLiteral("No managed local cache is available to store the downloaded subtitle.");
        emit changed();
        return {};
    }
    QString safeTitle = normalizedTitle;
    safeTitle.replace(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}._-]+")), QStringLiteral("-"));
    safeTitle.remove(QRegularExpression(QStringLiteral("^\\.+")));
    if (safeTitle.size() > 64) safeTitle.truncate(64);
    if (safeTitle.isEmpty()) safeTitle = QStringLiteral("subtitle");
    const auto directory = QDir(m_cacheRoot).filePath(QStringLiteral("online-subtitles"));
    if (!QDir().mkpath(directory)) {
        m_error = QStringLiteral("Could not create the managed online subtitle cache.");
        emit changed();
        return {};
    }
    const auto base = safeTitle + QLatin1Char('.') + normalizedLanguage;
    QString output = QDir(directory).filePath(base + QStringLiteral(".srt"));
    for (int version = 1; QFileInfo::exists(output) || QFileInfo::exists(output + QStringLiteral(".isolde.json")); ++version) {
        if (version > 10000) {
            m_error = QStringLiteral("Could not find a non-overwriting name for the downloaded subtitle.");
            emit changed();
            return {};
        }
        output = QDir(directory).filePath(base + QStringLiteral(".isolde-%1.srt").arg(version));
    }
    Job job;
    job.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    job.mode = QStringLiteral("download");
    job.source = QStringLiteral("OpenSubtitles.com file %1 · %2").arg(repositoryFileId).arg(normalizedTitle);
    job.sourceLanguage = normalizedLanguage;
    job.targetLanguage = normalizedLanguage;
    job.output = output;
    job.sourceHash = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    job.provenance = QStringLiteral("User-requested OpenSubtitles.com API download; local review required");
    job.qualityReport = evaluateQuality(job, cues);
    QString writeError;
    if (!writeOutput(job, srtText, &writeError)) {
        m_error = writeError;
        m_status = QStringLiteral("Downloaded subtitle was not stored; existing files were left unchanged.");
        emit changed();
        return {};
    }
    job.state = QStringLiteral("Ready for review");
    m_jobs.append(job);
    m_error.clear();
    m_status = QStringLiteral("OpenSubtitles download saved in Isolde's managed cache. Review it before loading in Kodi.");
    emit changed();
    return job.id;
}

void SubtitleService::cleanStaging(const Job &job) const
{
    if (!job.stagingDirectory.isEmpty() && job.stagingDirectory.startsWith(QDir(m_cacheRoot).absolutePath() + QDir::separator()))
        QDir(job.stagingDirectory).removeRecursively();
}

void SubtitleService::refreshLanguagePacks()
{
    if (m_languageProcess.state() != QProcess::NotRunning) return;
    const auto python = m_pythonExecutable.isEmpty()
        ? QStandardPaths::findExecutable(QStringLiteral("python3")) : m_pythonExecutable;
    if (python.isEmpty()) {
        m_languagePackStatus = QStringLiteral("Python 3 is unavailable; installed Argos language packs could not be inspected.");
        emit changed();
        return;
    }
    static const QString script = QStringLiteral(
        "import json\n"
        "import argostranslate.translate as t\n"
        "langs=t.get_installed_languages()\n"
        "pairs=[]\n"
        "for source in langs:\n"
        " for target in langs:\n"
        "  if source.code != target.code and source.get_translation(target) is not None:\n"
        "   pairs.append({'source':source.code,'target':target.code})\n"
        "print(json.dumps({'pairs':pairs},ensure_ascii=False))\n");
    m_languagePackAction = QStringLiteral("refresh");
    m_languagePackTimedOut = false;
    m_languagePackStatus = QStringLiteral("Checking locally installed Argos language pairs…");
    m_languageProcess.start(python, {QStringLiteral("-c"), script});
    m_languagePackTimeout.start(30000);
    emit changed();
}

bool SubtitleService::installLanguagePack(const QString &localPackagePath)
{
    const QFileInfo package(localPackagePath);
    if (!package.isFile() || package.isSymLink() ||
        package.suffix().compare(QStringLiteral("argosmodel"), Qt::CaseInsensitive) != 0 ||
        package.size() < 4 || package.size() > 512LL * 1024 * 1024) {
        m_languagePackStatus = QStringLiteral("Select a local .argosmodel package between 4 bytes and 512 MiB. Downloads are not supported.");
        emit changed();
        return false;
    }
    if (m_languageProcess.state() != QProcess::NotRunning) {
        m_languagePackStatus = QStringLiteral("Wait for the current local language-pack operation to finish.");
        emit changed();
        return false;
    }
    QFile input(package.absoluteFilePath());
    if (!input.open(QIODevice::ReadOnly) || input.read(4) != QByteArrayLiteral("PK\003\004")) {
        m_languagePackStatus = QStringLiteral("The selected file is not a readable local Argos package archive.");
        emit changed();
        return false;
    }
    const auto python = m_pythonExecutable.isEmpty()
        ? QStandardPaths::findExecutable(QStringLiteral("python3")) : m_pythonExecutable;
    if (python.isEmpty()) {
        m_languagePackStatus = QStringLiteral("Python 3 is unavailable; the local Argos package was not installed.");
        emit changed();
        return false;
    }
    static const QString script = QStringLiteral(
        "import sys\n"
        "import argostranslate.package as p\n"
        "p.install_from_path(sys.argv[1])\n");
    m_languagePackAction = QStringLiteral("install");
    m_languagePackTimedOut = false;
    m_languagePackStatus = QStringLiteral("Installing the explicitly selected local Argos language package; no network access is used.");
    m_languageProcess.start(python, {QStringLiteral("-c"), script, package.absoluteFilePath()});
    m_languagePackTimeout.start(10 * 60 * 1000);
    emit changed();
    return true;
}

void SubtitleService::finishLanguagePackProcess(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_languagePackTimeout.stop();
    const auto action = m_languagePackAction;
    m_languagePackAction.clear();
    if (m_languagePackTimedOut) {
        m_languagePackTimedOut = false;
        m_languagePackStatus = QStringLiteral("Local Argos language-pack operation timed out; no network request was made.");
        emit changed();
        return;
    }
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        auto detail = QString::fromUtf8(m_languageProcess.readAllStandardError()).trimmed();
        if (detail.size() > 512) detail = detail.left(512);
        m_languagePackStatus = action == QStringLiteral("install")
            ? QStringLiteral("Local Argos package installation failed; no network request was made. %1").arg(safeError(detail))
            : QStringLiteral("Could not inspect installed local Argos language packs. %1").arg(safeError(detail));
        emit changed();
        return;
    }
    if (action == QStringLiteral("install")) {
        m_languagePackStatus = QStringLiteral("Local Argos language package installed. Refreshing available offline translation pairs…");
        emit changed();
        refreshLanguagePacks();
        return;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(m_languageProcess.readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
        !document.object().value(QStringLiteral("pairs")).isArray()) {
        m_languagePackStatus = QStringLiteral("Argos returned an invalid installed-language response.");
        emit changed();
        return;
    }
    QVariantList pairs;
    const auto array = document.object().value(QStringLiteral("pairs")).toArray();
    if (array.size() > 512) {
        m_languagePackStatus = QStringLiteral("Argos reported too many installed language pairs; results were refused.");
        emit changed();
        return;
    }
    for (const auto &value : array) {
        if (!value.isObject()) continue;
        const auto pair = value.toObject();
        const auto source = pair.value(QStringLiteral("source")).toString();
        const auto target = pair.value(QStringLiteral("target")).toString();
        if (!validLanguage(source) || !validLanguage(target) || source == target) continue;
        pairs.append(QVariantMap{
            {QStringLiteral("source"), source},
            {QStringLiteral("target"), target}
        });
    }
    m_languagePacks = pairs;
    m_languagePackStatus = pairs.isEmpty()
        ? QStringLiteral("No installed Argos translation pairs found. Select and import a local .argosmodel package; this tool never downloads packs.")
        : QStringLiteral("%1 installed offline Argos translation pair(s).").arg(pairs.size());
    emit changed();
}
