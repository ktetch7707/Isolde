#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QStack>
#include <QString>
#include <QPointer>
#include <QVariantMap>

class HomeLayout;
class QTemporaryDir;

/**
 * @brief T16 — shared draft/undo transaction foundation for Page Studio.
 *
 * StudioSession holds an opaque canonical document snapshot (Home layout or
 * single-page document) plus the last committed bytes. Edits push the prior
 * draft onto a bounded undo stack; nothing here touches disk or live models —
 * the caller applies draft()->committed() through the existing atomic
 * HomeLayout/PageModel save pipeline (validate → temp → fsync → rename →
 * rollback), preserving profile isolation and the standing atomic-save gate.
 *
 * T16.2 commands: allocateId() (central deterministic identity) and
 * canonicalize() (deterministic serialization so remote/pointer/inspector
 * replays produce identical layout bytes from identical fixture identities).
 * T16.4 revisions: open() records the profile revision the draft was based
 * on; isStaleAgainst() reports when the applied document moved underneath
 * the draft (dirty-exit / revision-conflict dialog input), without any
 * cross-profile reads — the caller supplies the current revision bytes.
 *
 * Bounds (AGENTS.md §9): documents over 4 MiB or with more than 1024
 * components are refused, never silently clamped.
 */
class StudioSession final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool dirty READ isDirty NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(int changeCount READ changeCount NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)
    Q_PROPERTY(QVariantMap document READ document NOTIFY changed)
    Q_PROPERTY(QObject *model READ model NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QVariantList availableActions READ actionChoices NOTIFY changed)
    Q_PROPERTY(QObject *proposalModel READ proposalModel NOTIFY changed)

public:
    static constexpr int kMaxUndoSteps = 100;
    static constexpr qsizetype kMaxHistoryBytes = 32 * 1024 * 1024;
    static constexpr qsizetype kMaxDocumentBytes = 4 * 1024 * 1024;
    static constexpr qsizetype kMaxComponents = 1024;

    explicit StudioSession(QObject *parent = nullptr);
    ~StudioSession() override;
    bool bind(HomeLayout *layout);
    Q_INVOKABLE void invalidate();
    bool active() const { return m_active; }
    QVariantMap document() const { return QJsonDocument::fromJson(m_draft).object().toVariantMap(); }
    QObject *model() const;
    QObject *proposalModel() const;
    Q_INVOKABLE bool previewCommand(const QString &operation, const QVariantMap &arguments, const QString &pageId = {});
    Q_INVOKABLE bool acceptPreview();
    Q_INVOKABLE void cancelPreview();
    Q_INVOKABLE bool execute(const QString &operation, const QVariantMap &arguments, const QString &pageId = {});
    Q_INVOKABLE bool apply(const QString &focusId = {});
    Q_INVOKABLE QVariantMap componentPrototype(const QString &type, const QVariantMap &configuration = {});
    Q_INVOKABLE QVariantList artworkChoices() const;
    Q_INVOKABLE QString stageArtwork(const QString &source);
    Q_INVOKABLE QVariantList actionChoices() const;
    Q_INVOKABLE QVariantList sourceChoices(const QString &id, const QString &pageId = {}) const;
    Q_INVOKABLE QVariantMap placementProposal(const QString &id, const QVariantMap &placement,
                                               const QString &strategy, const QString &pageId = {}, const QVariantMap &options = {});
    Q_INVOKABLE QVariantMap additionProposal(const QString &type, const QVariantMap &placement,
                                              const QString &strategy, const QString &pageId = {}, const QString &categoryId = {}, const QString &parentId = {}, const QVariantMap &configuration = {});

    /** Capture the live committed document. Clears undo/redo history. */
    Q_INVOKABLE bool open(const QByteArray &committedDocument);
    /** Propose a new draft. Invalid drafts are refused with errorString set. */
    Q_INVOKABLE bool pushEdit(const QByteArray &candidateDraft);
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();
    // Explicit consent to release history; the draft and applied baseline survive.
    Q_INVOKABLE void continueWithNewHistoryBaseline() { m_undo.clear(); m_redo.clear(); setError({}); emit changed(); }
    /** Drop the draft and history, returning to the committed document. */
    Q_INVOKABLE void revert();
    /** Caller persists draft() itself, then reports success here. */
    Q_INVOKABLE void markApplied();

    [[nodiscard]] bool isDirty() const;
    [[nodiscard]] bool canUndo() const { return !m_undo.isEmpty(); }
    [[nodiscard]] bool canRedo() const { return !m_redo.isEmpty(); }
    [[nodiscard]] int changeCount() const { return m_undo.size(); }
    [[nodiscard]] QByteArray draft() const { return m_draft; }
    [[nodiscard]] QByteArray committed() const { return m_committed; }
    [[nodiscard]] QString errorString() const { return m_error; }

    /**
     * T16.2 — centrally allocate one deterministic identity for a new
     * component. Returns empty when the candidate base is unsafe or already
     * taken by the draft's ids/pages (caller shows the error and retries
     * with a different base; nothing is clamped).
     */
    Q_INVOKABLE QString allocateId(const QString &baseId);
    /**
     * T16.2 — deterministic canonical serialization: parses both inputs and
     * re-emits both in the same canonical (sorted-key, compact) form, so
     * byte comparison is meaningful regardless of producer key order.
     * Returns true and sets both outputs; false on invalid input.
     * (C++-only: out-parameters are not QML-callable.)
     */
    bool canonicalize(const QByteArray &first, const QByteArray &second,
                      QByteArray &firstOut, QByteArray &secondOut);
    /**
     * T16.4 — true when the draft is dirty AND the supplied current applied
     * bytes differ from the baseline this draft was opened from: another
     * writer moved the document underneath the draft (revision conflict).
     * The caller passes the current profile revision; no cross-profile
     * reads happen here. A clean draft is never stale (nothing to lose).
     */
    Q_INVOKABLE bool isStaleAgainst(const QByteArray &currentApplied) const;

signals:
    void changed();

private:
    void configureWorker(HomeLayout &worker, bool assetPaths = false) const;
    bool validNewAction(const QVariantMap &value);
    bool validDraft(const QByteArray &data);
    void setError(const QString &message);

    QByteArray m_committed;
    QByteArray m_draft;
    QStack<QByteArray> m_undo;
    QStack<QByteArray> m_redo;
    QString m_error;
    QPointer<HomeLayout> m_live;
    QTemporaryDir *m_assetStage = nullptr;
    QHash<QString,QString> m_stagedAssets;
    HomeLayout *m_model = nullptr;
    QString m_profilePath;
    bool m_active = false;
    HomeLayout *m_proposalModel = nullptr;
    QByteArray m_proposed;
    QByteArray m_proposalBaseline;
    QByteArray m_diskBaseline;
    bool m_diskExisted = false;
    bool m_diskReadable = true;
};
