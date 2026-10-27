#include "app/FdroidProvider.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <memory>

FdroidProvider::FdroidProvider(QObject *parent) : StoreProvider(parent),
    m_profile(qApp->property("isoldeProfile").toString()) {}

FdroidProvider::~FdroidProvider() { cancel(); }

void FdroidProvider::cancel()
{
    if (m_process) {
        auto *process = m_process.data();
        m_process = nullptr;
        process->disconnect(this);
        process->kill();
        process->waitForFinished(1000);
        process->deleteLater();
    }
    m_results.clear();
    m_next = m_previous = false;
    m_status.clear();
    emit resultsChanged();
}

QVariantList FdroidProvider::search(const QString &query)
{
    requestSearch(query);
    return m_results;
}

void FdroidProvider::requestSearch(const QString &query)
{
    if (query.size() > 128) { emit providerError(tr("Use a search of at most 128 characters.")); return; }
    m_query = query.trimmed();
    start(0);
}

void FdroidProvider::requestPage(int direction)
{
    if (m_process || (direction != 1 && direction != -1)) return;
    if ((direction == 1 && m_next) || (direction == -1 && m_previous)) start(m_page + direction);
}

void FdroidProvider::start(int page)
{
    cancel();
    if (qApp->property("isoldeProfile").toString() != m_profile) {
        emit providerError(tr("The active profile changed. Reopen the Store.")); return;
    }
    const auto profile = m_profile.isEmpty() ? QStringLiteral("default") : m_profile;
    if (profile != "default" && !QRegularExpression("^[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}$").match(profile).hasMatch()) {
        emit providerError(tr("Invalid Store profile.")); return;
    }
    QString helper;
    const auto base = QCoreApplication::applicationDirPath();
    for (const auto &path : {base + "/../share/isolde/fdroid-store.py", base + "/share/isolde/fdroid-store.py"})
        if (QFileInfo(path).isFile()) { helper = QFileInfo(path).absoluteFilePath(); break; }
    const auto python = QStandardPaths::findExecutable("python3");
    if (helper.isEmpty() || python.isEmpty()) {
        emit providerError(tr("F-Droid browsing support is missing. Repair the Isolde installation.")); return;
    }
    const auto cache = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
        .filePath("profiles/" + profile + "/fdroid");
    auto *process = new QProcess(this);
    m_process = process;
    m_status = tr("Loading and verifying F-Droid apps…");
    emit resultsChanged();
    auto output = std::make_shared<QByteArray>();
    connect(process, &QProcess::readyReadStandardOutput, this, [process, output] {
        *output += process->readAllStandardOutput();
        if (output->size() > 1024*1024) process->kill();
    });
    connect(process, &QProcess::readyReadStandardError, this, [process] { process->readAllStandardError(); });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (m_process != process || error != QProcess::FailedToStart) return;
        m_process = nullptr; m_status.clear(); process->deleteLater();
        emit providerError(tr("F-Droid could not start. Repair the Isolde installation."));
        emit resultsChanged();
    });
    connect(process, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this,
        [this, process, output, page](int code, QProcess::ExitStatus exit) {
        if (m_process != process) return;
        m_process = nullptr; m_status.clear();
        *output += process->readAllStandardOutput();
        process->deleteLater();
        if (qApp->property("isoldeProfile").toString() != m_profile) return;
        const auto document = output->size() <= 1024*1024 ? QJsonDocument::fromJson(*output).object() : QJsonObject{};
        const auto rows = document.value("results").toArray();
        if (code || exit != QProcess::NormalExit || !document.value("results").isArray() || rows.size() > 50) {
            emit providerError(document.value("error").toString(tr("F-Droid could not load or verify the repository. Search again to retry.")));
            emit resultsChanged(); return;
        }
        m_page = page;
        m_results = document.value("results").toVariant().toList();
        m_next = document.value("hasNext").toBool();
        m_previous = document.value("hasPrevious").toBool();
        m_status = tr("F-Droid · Page %1 · %2 matching apps").arg(page + 1).arg(document.value("total").toInt());
        emit resultsChanged();
    });
    auto *timer = new QTimer(process);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, process, &QProcess::kill);
    timer->start(150000);
    process->setProgram(python);
    process->setArguments({helper, "--cache", cache, "--query", m_query, "--page", QString::number(page)});
    process->start();
}

QVariantMap FdroidProvider::result(const QString &storeId) const
{
    for (const auto &value : m_results) {
        const auto map = value.toMap();
        if (map.value("storeId").toString() == storeId) return map;
    }
    return {};
}

QUrl FdroidProvider::artifactUrl(const QString &storeId) const { return QUrl(result(storeId).value("downloadUrl").toString()); }
QByteArray FdroidProvider::artifactHash(const QString &storeId) const { return QByteArray::fromHex(result(storeId).value("sha256").toByteArray()); }
QString FdroidProvider::provenanceText(const QString &storeId) const { return result(storeId).value("provenance").toString(); }
