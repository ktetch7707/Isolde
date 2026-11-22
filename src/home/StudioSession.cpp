#include "app/AppArtwork.h"
#include "home/ComponentSchema.h"
#include "home/HomeLayout.h"
#include "home/StudioSession.h"

#include <QBuffer>
#include <QColor>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <algorithm>
#include <functional>

StudioSession::StudioSession(QObject* parent) : QObject(parent)
{
    connect(this,
            &StudioSession::changed,
            this,
            [this]
            {
                if (m_live && !m_live->m_draftOnly && m_profileId == qApp->property("isoldeProfile").toString())
                    qApp->setProperty("isoldeStudioDirty", m_active && isDirty());
                if (m_model && !m_draft.isEmpty() && m_model->documentData() != m_draft)
                {
                    m_model->loadDocument(m_draft, false);
                    emit m_model->layoutChanged();
                }
            });
}

QObject*
StudioSession::model() const
{
    return m_model;
}

StudioSession::~StudioSession()
{
    invalidate();
    delete m_assetStage;
    delete m_referenceStage;
}

bool
StudioSession::bind(HomeLayout* layout)
{
    if (!layout || !editingPermitted())
        return false;
    m_live = layout;
    m_profilePath = layout->m_userPath;
    m_profileId = qApp->property("isoldeProfile").toString();
    QFile disk(m_profilePath);
    m_diskExisted = disk.exists();
    m_diskReadable = !m_diskExisted || (disk.size() <= kMaxDocumentBytes && disk.open(QIODevice::ReadOnly));
    m_diskBaseline = m_diskReadable && m_diskExisted ? disk.readAll() : QByteArray();
    if (m_model)
        m_model->deleteLater();
    m_model = new HomeLayout(layout->registry(), this);
    m_model->m_draftOnly = true;
    m_model->m_userPath = layout->m_userPath;
    m_model->m_defaultPath = layout->m_defaultPath;
    m_model->m_defaultTheme = layout->m_defaultTheme;
    m_model->m_collections = layout->m_collections;
    m_model->m_captureInputsReady = layout->m_captureInputsReady;
    m_model->m_assetPaths = layout->m_assetPaths;
    m_active = open(layout->committedDocument());
    connect(layout, &QObject::destroyed, this, &StudioSession::invalidate);
    return m_active;
}

void
StudioSession::invalidate()
{
    if (m_live && !m_live->m_draftOnly && m_profileId == qApp->property("isoldeProfile").toString())
        qApp->setProperty("isoldeStudioDirty", false);
    m_active = false;
    clearReferenceImage();
    m_editorLocked = false;
    m_live.clear();
    m_stagedAssets.clear();
    m_iconOriginals.clear();
    delete m_assetStage;
    m_assetStage = nullptr;
    m_diskBaseline.clear();
    m_profilePath.clear();
    m_profileId.clear();
    cancelPreview();
    m_draft.clear();
    m_committed.clear();
    m_undo.clear();
    m_redo.clear();
    if (m_model)
    {
        m_model->m_items = {};
        m_model->m_assetPaths.clear();
        m_model->m_animationOwners.clear();
        m_model->m_gifStills.clear();
        m_model->m_categories = {};
        m_model->m_pages = {};
        m_model->m_collections = {};
        m_model->m_composition.clear();
        m_model->m_homePageId.clear();
        m_model->m_rootPage = {};
        m_model->m_studioAppearance.clear();
        m_model->m_userPath.clear();
        m_model->m_defaultPath.clear();
        m_model->syncPages();
        emit m_model->layoutChanged();
    }
    setError(QStringLiteral("The profile edit session has closed."));
    emit changed();
}

bool
StudioSession::open(const QByteArray& committedDocument)
{
    if (!validDraft(committedDocument))
        return false;
    applyCommittedDocument(committedDocument);
    return true;
}

void
StudioSession::applyCommittedDocument(const QByteArray& committedDocument)
{
    m_committed = committedDocument;
    m_draft = committedDocument;
    m_undo.clear();
    m_redo.clear();
    setError({});
    emit changed();
}

bool
StudioSession::pushEdit(const QByteArray& candidateDraft)
{
    if (!editingPermitted())
        return false;
    if (m_committed.isEmpty())
    {
        setError(QStringLiteral("Open a page before editing."));
        return false;
    }
    if (candidateDraft == m_draft)
        return true;
    if (!validDraft(candidateDraft))
        return false;
    qsizetype retained = m_draft.size();
    for (const auto& entry : m_undo)
        retained += entry.size();
    if (m_undo.size() >= kMaxUndoSteps || retained > kMaxHistoryBytes)
    {
        setError(QStringLiteral("History budget reached. Continue with a new history baseline or cancel this edit."));
        return false;
    }
    m_undo.push(m_draft);
    m_draft = candidateDraft;
    m_redo.clear();
    setError({});
    emit changed();
    return true;
}

bool
StudioSession::undo()
{
    if (!canUndoRedo(m_undo.isEmpty(), "Nothing to undo."))
        return false;
    m_redo.push(m_draft);
    m_draft = m_undo.pop();
    setError({});
    emit changed();
    return true;
}

bool
StudioSession::redo()
{
    if (!canUndoRedo(m_redo.isEmpty(), "Nothing to redo."))
        return false;
    m_undo.push(m_draft);
    m_draft = m_redo.pop();
    setError({});
    emit changed();
    return true;
}

bool
StudioSession::canUndoRedo(bool isEmpty, const char* message)
{
    if (!editingPermitted())
        return false;
    if (isEmpty)
    {
        setError(QString::fromUtf8(message));
        return false;
    }
    return true;
}

void
StudioSession::revert()
{
    cancelPreview();
    m_draft = m_committed;
    m_undo.clear();
    m_redo.clear();
    setError({});
    emit changed();
}

void
StudioSession::markApplied()
{
    m_committed = m_draft;
    m_undo.clear();
    m_redo.clear();
    setError({});
    emit changed();
}

bool
StudioSession::isDirty() const
{
    return !m_committed.isEmpty() && m_draft != m_committed;
}

QString
StudioSession::allocateId(const QString& baseId)
{
    QString base = baseId.trimmed();
    if (!ComponentSchema::safeId(base))
    {
        setError(
            QStringLiteral("That name cannot be used as an identity — use letters, digits, dot, dash or underscore."));
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(m_draft);
    QSet<QString> taken;
    if (doc.isObject())
    {
        const auto collect = [&taken](const QJsonObject& object)
        {
            for (const auto& key : {"components", "pages", "categories"})
                for (const auto& entry : object.value(key).toArray())
                    taken.insert(entry.toObject().value("id").toString());
        };
        collect(doc.object());
        for (const auto& page : doc.object().value("pages").toArray())
            collect(page.toObject().value("layout").toObject());
    }
    if (!taken.contains(base))
        return base;
    const QString copyBase = base + QStringLiteral("-copy");
    if (!ComponentSchema::safeId(copyBase))
    {
        setError(QStringLiteral(
            "The generated identity exceeds its length limit. Rename the component before duplicating."));
        return {};
    }
    if (!taken.contains(copyBase))
        return copyBase;
    int suffix = 2;
    while (taken.contains(copyBase + QString::number(suffix)))
        ++suffix;
    return copyBase + QString::number(suffix);
}

namespace
{
QJsonValue
canonicalValue(const QJsonValue& value)
{
    if (value.isObject())
    {
        QJsonObject sorted;
        QStringList keys;
        const auto object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it)
            keys.append(it.key());
        std::sort(keys.begin(), keys.end());
        for (const QString& key : keys)
            sorted.insert(key, canonicalValue(object.value(key)));
        return sorted;
    }
    if (value.isArray())
    {
        QJsonArray out;
        for (const QJsonValue& entry : value.toArray())
            out.append(canonicalValue(entry));
        return out;
    }
    return value;
}
} // namespace

bool
StudioSession::canonicalize(const QByteArray& first,
                            const QByteArray& second,
                            QByteArray& firstOut,
                            QByteArray& secondOut)
{
    const QJsonDocument firstDoc = QJsonDocument::fromJson(first);
    const QJsonDocument secondDoc = QJsonDocument::fromJson(second);
    if (firstDoc.isNull() || secondDoc.isNull())
    {
        setError(QStringLiteral("Cannot compare drafts — one is not valid JSON."));
        return false;
    }
    const auto emitDoc = [](const QJsonDocument& doc)
    {
        if (doc.isObject())
            return QJsonDocument(canonicalValue(doc.object()).toObject()).toJson(QJsonDocument::Compact);
        if (doc.isArray())
            return QJsonDocument(canonicalValue(doc.array()).toArray()).toJson(QJsonDocument::Compact);
        return doc.toJson(QJsonDocument::Compact);
    };
    firstOut = emitDoc(firstDoc);
    secondOut = emitDoc(secondDoc);
    return true;
}

bool
StudioSession::isStaleAgainst(const QByteArray& currentApplied) const
{
    if (!isDirty())
        return false;
    if (currentApplied.isEmpty())
        return true;
    QByteArray mine, theirs;
    if (!canonicalizeCommitted(m_committed, currentApplied, mine, theirs))
        return true;
    return mine != theirs;
}

bool
StudioSession::canonicalizeCommitted(const QByteArray& committed,
                                     const QByteArray& currentApplied,
                                     QByteArray& mine,
                                     QByteArray& theirs) const
{
    StudioSession* self = const_cast<StudioSession*>(this);
    return self->canonicalize(committed, currentApplied, mine, theirs);
}

bool
StudioSession::validDraft(const QByteArray& data)
{
    if (data.isEmpty())
    {
        setError(QStringLiteral("The draft document is empty — nothing was written."));
        return false;
    }
    if (data.size() > kMaxDocumentBytes)
    {
        setError(QStringLiteral("The draft exceeds 4 MiB and was refused."));
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject())
    {
        setError(QStringLiteral("The draft is not a valid layout document."));
        return false;
    }
    const QJsonObject object = doc.object();
    if (object.value(QStringLiteral("schemaVersion")).toDouble(-1) != 2)
    {
        setError(QStringLiteral("Unknown layout version — draft refused."));
        return false;
    }
    const QJsonArray components = object.value(QStringLiteral("components")).toArray();
    if (components.size() > kMaxComponents)
    {
        setError(QStringLiteral("The draft exceeds 1024 components and was refused."));
        return false;
    }
    QString error;
    if (!HomeLayout::validateStudioDocument(data, &error, m_live ? m_live->m_defaultTheme : QVariantMap{}))
    {
        setError(error);
        return false;
    }
    return true;
}

void
StudioSession::setError(const QString& message)
{
    if (m_error == message)
        return;
    m_error = message;
    emit changed();
}
