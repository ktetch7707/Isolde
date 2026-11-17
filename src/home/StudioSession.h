#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QStack>
#include <QString>
#include <QVariantMap>

class HomeLayout;
class QTemporaryDir;

/**
 * @brief T16 — shared draft/undo transaction foundation for Skin Studio.
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
class StudioSession final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool dirty READ isDirty NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(int changeCount READ changeCount NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)
    Q_PROPERTY(QVariantMap document READ document NOTIFY changed)
    Q_PROPERTY(QObject* model READ model NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QVariantList availableActions READ actionChoices NOTIFY changed)
    Q_PROPERTY(QObject* proposalModel READ proposalModel NOTIFY changed)
    Q_PROPERTY(QVariantList changeSummary READ changeSummary NOTIFY changed)
    Q_PROPERTY(QVariantList healthFindings READ healthFindings NOTIFY changed)
    Q_PROPERTY(bool editorLocked READ editorLocked NOTIFY changed)
  public:
    static constexpr int kMaxUndoSteps = 100;
    static constexpr qsizetype kMaxHistoryBytes = 32 * 1024 * 1024;
    static constexpr qsizetype kMaxDocumentBytes = 4 * 1024 * 1024;
    static constexpr qsizetype kMaxComponents = 1024;

    explicit StudioSession(QObject* parent = nullptr);
    ~StudioSession() override;
    bool
    bind(HomeLayout* layout);
    Q_INVOKABLE void
    invalidate();
    bool
    active() const
    {
        return m_active;
    }
    QVariantMap
    document() const
    {
        return QJsonDocument::fromJson(m_draft).object().toVariantMap();
    }
    QObject*
    model() const;
    QObject*
    proposalModel() const;
    Q_INVOKABLE bool
    previewCommand(const QString& operation, const QVariantMap& arguments, const QString& pageId = {});
    Q_INVOKABLE bool
    acceptPreview();
    Q_INVOKABLE bool
    acceptColorPreview(const QString& color);
    Q_INVOKABLE void
    cancelPreview();
    Q_INVOKABLE bool
    execute(const QString& operation, const QVariantMap& arguments, const QString& pageId = {});
    Q_INVOKABLE bool
    apply(const QString& focusId = {});
    bool
    validateApplyPreconditions();
    bool
    stageArtworkFiles(QStringList& created, const QString& artDirectory, bool hadArtDirectory);
    bool
    stageSingleArtifact(const QString& key, const QString& sourcePath, const QString& serialized,
                        QStringList& created, const QString& profileDir);
    bool
    shouldKeepArtifact(const QString& key, const QString& serialized);
    bool
    finalizeApply(const QString& focusId, QStringList& created, const QString& artDirectory, bool hadArtDirectory);
    QVariantList
    changeSummary() const;
    QVariantList
    healthFindings() const;
    bool
    editorLocked() const
    {
        return m_editorLocked;
    }
    Q_INVOKABLE void
    setEditorLocked(bool locked);
    Q_INVOKABLE QVariantList
    versions();
    Q_INVOKABLE bool
    saveVersion(const QString& name);
    Q_INVOKABLE bool
    deleteVersion(const QString& id);
    Q_INVOKABLE bool
    renameVersion(const QString& id, const QString& name);
    Q_INVOKABLE bool
    resetVersions();
    Q_INVOKABLE bool
    previewDefaults(const QString& scope, const QString& pageId = {});
    Q_INVOKABLE bool
    previewVersion(const QString& id, const QString& scope, const QString& pageId = {});
    Q_INVOKABLE bool
    reloadApplied();
    Q_INVOKABLE bool
    previewSavedDesign();
    Q_INVOKABLE QVariantList
    designChoices() const;
    Q_INVOKABLE QVariantList
    reusableChoices(const QString& kind) const;
    Q_INVOKABLE QVariantMap
    previewReuse(const QString& operation, const QVariantMap& arguments, const QString& pageId = {});
    // Design/import previews share the normal proposal model and one-step Undo.
    // Page scope preserves every other page; interface scope reports additions,
    // replacements and removals before acceptPreview. Only apply() publishes assets.
    Q_INVOKABLE QVariantMap
    previewDesign(const QString& id, const QString& scope, const QString& pageId = {});
    Q_INVOKABLE QVariantMap
    previewImport(const QString& path,
                  const QString& scope,
                  const QString& pageId = {},
                  const QVariantMap& appRemap = {});
    Q_INVOKABLE bool
    exportDesign(const QString& path, const QString& scope, const QString& pageId = {});
    Q_INVOKABLE QVariantMap
    browseDesignFiles(const QString& directory = {}, int page = 0) const;
    Q_PROPERTY(QString referenceSource READ referenceSource NOTIFY changed)
    // Editor-only image state: never serialized, exported or copied to profile artwork.
    QString
    referenceSource() const
    {
        return m_referenceSource;
    }
    Q_INVOKABLE bool
    setReferenceImage(const QString& path);
    Q_INVOKABLE void
    clearReferenceImage();
    Q_INVOKABLE QVariantMap
    componentPrototype(const QString& type, const QVariantMap& configuration = {});
    Q_INVOKABLE QVariantMap
    replacementProposal(const QString& id, const QString& type, const QString& pageId = {});
    Q_INVOKABLE QVariantList
    artworkChoices() const;
    Q_INVOKABLE QVariantMap
    browseArtwork(const QString& directory = {}, int page = 0);
    Q_INVOKABLE QString
    artworkPreview(const QString& source);
    Q_INVOKABLE QString
    stageArtwork(const QString& source);
    Q_INVOKABLE QVariantMap
    cleanIconArtwork(const QString& source, int strength = 1, const QString& background = {});
    Q_INVOKABLE QVariantList
    actionChoices() const;
    Q_INVOKABLE QVariantList
    sourceChoices(const QString& id, const QString& pageId = {}) const;
    Q_INVOKABLE QVariantMap
    placementProposal(const QString& id,
                      const QVariantMap& placement,
                      const QString& strategy,
                      const QString& pageId = {},
                      const QVariantMap& options = {});
    Q_INVOKABLE QVariantMap
    additionProposal(const QString& type,
                     const QVariantMap& placement,
                     const QString& strategy,
                     const QString& pageId = {},
                     const QString& categoryId = {},
                     const QString& parentId = {},
                     const QVariantMap& configuration = {},
                     bool enableScrolling = false);

    /** Capture the live committed document. Clears undo/redo history. */
    Q_INVOKABLE bool
    open(const QByteArray& committedDocument);
    /** Propose a new draft. Invalid drafts are refused with errorString set. */
    Q_INVOKABLE bool
    pushEdit(const QByteArray& candidateDraft);
    Q_INVOKABLE bool
    undo();
    Q_INVOKABLE bool
    redo();
    // Explicit consent to release history; the draft and applied baseline survive.
    Q_INVOKABLE void
    continueWithNewHistoryBaseline()
    {
        m_undo.clear();
        m_redo.clear();
        setError({});
        emit changed();
    }
    /** Drop the draft and history, returning to the committed document. */
    Q_INVOKABLE void
    revert();
    /** Caller persists draft() itself, then reports success here. */
    Q_INVOKABLE void
    markApplied();

    [[nodiscard]] bool
    isDirty() const;
    [[nodiscard]] bool
    canUndo() const
    {
        return !m_undo.isEmpty();
    }
    [[nodiscard]] bool
    canRedo() const
    {
        return !m_redo.isEmpty();
    }
    [[nodiscard]] int
    changeCount() const
    {
        return m_undo.size();
    }
    [[nodiscard]] QByteArray
    draft() const
    {
        return m_draft;
    }
    [[nodiscard]] QByteArray
    committed() const
    {
        return m_committed;
    }
    [[nodiscard]] QString
    errorString() const
    {
        return m_error;
    }

    /**
     * T16.2 — centrally allocate one deterministic identity for a new
     * component. Returns empty when the candidate base is unsafe or already
     * taken by the draft's ids/pages (caller shows the error and retries
     * with a different base; nothing is clamped).
     */
    Q_INVOKABLE QString
    allocateId(const QString& baseId);
    /**
     * T16.2 — deterministic canonical serialization: parses both inputs and
     * re-emits both in the same canonical (sorted-key, compact) form, so
     * byte comparison is meaningful regardless of producer key order.
     * Returns true and sets both outputs; false on invalid input.
     * (C++-only: out-parameters are not QML-callable.)
     */
    bool
    canonicalize(const QByteArray& first, const QByteArray& second, QByteArray& firstOut, QByteArray& secondOut);
    /**
     * T16.4 — true when the draft is dirty AND the supplied current applied
     * bytes differ from the baseline this draft was opened from: another
     * writer moved the document underneath the draft (revision conflict).
     * The caller passes the current profile revision; no cross-profile
     * reads happen here. A clean draft is never stale (nothing to lose).
     */
    Q_INVOKABLE bool
    isStaleAgainst(const QByteArray& currentApplied) const;

  signals:
    void
    changed();

  private:
    bool
    currentProfile() const;
    bool
    profileStorageSafe() const;
    bool
    diskRevisionUnchanged() const;
    bool
    editingPermitted();
    QByteArray
    readAppliedDocument();
    bool
    writeVersions(const QJsonArray& entries, QJsonObject assets);
    QJsonObject
    stageDesignArtwork(QJsonObject document, const QJsonObject& assets, bool* valid);
    bool
    proposeDocument(const QByteArray& bytes);
    bool
    archiveVersion(const QString& name, const QByteArray& document);
    bool
    readVersions(QJsonArray& entries, QJsonObject* assets = nullptr);
    void
    configureWorker(HomeLayout& worker, bool assetPaths = false) const;
    QVariantMap
    proposeDesign(QJsonObject document, const QJsonObject& assets, const QString& scope, const QString& pageId);
    bool
    validNewAction(const QVariantMap& value);
    bool
    validDraft(const QByteArray& data);
    void
    setError(const QString& message);

    // Operation dispatch for execute() — each helper handles one edit command.
    bool
    executeReplaceComponent(const QVariantMap& a, const QString& pageId);
    bool
    executeRestorePageDefaults(const QVariantMap& a, const QString& pageId);
    bool
    executeStudioAppearance(const QVariantMap& a);
    bool
    executeRenameDuplicatePage(const QString& operation, const QVariantMap& a, const QString& pageId,
                               QJsonObject document, QJsonArray pages, QJsonObject rootMeta);
    bool
    validatePageExists(const QVariantMap& a, const QJsonObject& document, const QJsonArray& pages, QString& id,
                       int& index, QJsonObject& rootMetaLocal);
    bool
    validatePageName(const QVariantMap& a, QString& name);
    bool
    renamePageRoot(QJsonObject& rootMetaLocal, const QString& name);
    bool
    renamePageEntry(QJsonArray& pages, int index, const QString& name);
    QString
    collectPageOrder(const QVariantMap& a);
    bool
    duplicatePage(const QVariantMap& a, QJsonObject document, QJsonArray pages, QJsonObject rootMeta,
                  QStringList& order, int index, const QString& id);
    QString
    remapDuplicateIdentity(const QString& old, QHash<QString, QString>& copiedIds, QSet<QString>& allocated);
    void
    remapLayoutIds(QJsonObject& layout, QHash<QString, QString>& copiedIds, QSet<QString>& allocated);
    void
    remapComponentReferences(QJsonArray& components, const QHash<QString, QString>& copiedIds);
    void
    remapNavigationTabs(QJsonObject& composition, QHash<QString, QString>& copiedIds, QSet<QString>& allocated);
    QString
    allocateCopyPageId(QSet<QString>& allocated);
    bool
    executeRemovePage(const QVariantMap& a, QJsonObject document, QJsonArray pages, QJsonObject rootMeta);
    bool
    validateRemovePagePreconditions(const QVariantMap& a, const QJsonObject& document, const QJsonArray& pages,
                                    const QString& id, int& index, bool& remap, QString& replacement,
                                    QJsonObject& rootMetaLocal);
    QJsonObject
    repairPageReferences(const QJsonObject& document, const QString& id, bool remap,
                         const QString& replacement, int& links, bool& invalidOverride);
    QJsonValue
    repairPageValue(const QJsonValue& value, const QString& id, bool remap, const QString& replacement,
                    int& links, bool& invalidOverride);
    QJsonObject
    repairPageObject(QJsonObject object, const QString& id, bool remap, const QString& replacement, int& links,
                     bool& invalidOverride);
    void
    normalizePageLayouts(QJsonObject& document, QJsonArray& pages);
    int
    findPageIndex(const QJsonArray& pages, const QString& id);
    void
    storePageOrder(QStringList& order, QJsonArray& pages, QJsonObject& rootMeta);
    bool
    executeSetHomePage(const QVariantMap& a);
    bool
    executeAddResolvedPlaced(const QString& operation, const QVariantMap& a, const QString& pageId);
    bool
    executeBatch(const QVariantMap& a, const QString& pageId);

    // Page-scoped layout operation helpers.
    bool
    executePageRepairSource(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageAssignSection(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageNavigation(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    int
    findTabIndex(const QJsonArray& tabs, const QString& id);
    bool
    handleNavigationAdd(QJsonArray& tabs, const QVariantMap& a);
    bool
    handleNavigationRemove(QJsonArray& tabs, int index);
    bool
    handleNavigationMove(QJsonArray& tabs, int index, const QVariantMap& a);
    bool
    handleNavigationPatch(QJsonArray& tabs, int index, const QVariantMap& a);
    void
    updateInitialTab(QJsonObject& composition, const QJsonArray& tabs);
    bool
    isTabPresent(const QJsonArray& tabs, const QString& tabId);
    bool
    executePagePatch(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageScrolling(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageSetApps(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    int
    findAppBarIndex(const QJsonArray& entries, const QString& id);
    QSet<QString>
    computeRemovedIds(const QJsonArray& entries, const QString& id);
    bool
    validateRemovedUnlocked(const QJsonArray& entries, const QSet<QString>& removed);
    bool
    validateSelectedApps(const QVariantMap& a, bool all, const QJsonArray& previous, QSet<QString>& seen);
    QJsonObject
    buildAppMembership(const QJsonArray& entries, int rootIndex, bool all, const QVariantList& selected);
    QJsonArray
    filterAndUpdateComponents(const QJsonArray& entries, const QSet<QString>& removed, const QString& id,
                              const QJsonObject& membership);
    bool
    executePagePatchComponent(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    QSet<QString>
    resolvePatchTargets(const QVariantMap& a);
    bool
    validatePatchValues(const QVariantMap& values);
    bool
    applyPatchToComponent(QJsonObject& item, const QVariantMap& values, QString& error);
    bool
    executePageSection(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageBrand(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageArrange(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    validateArrangeSelection(const QVariantMap& a, QString& mode, QList<int>& indices);
    QSet<QString>
    collectArrangeIds(const QVariantList& selection);
    bool
    findArrangeTargets(const QJsonArray& entries, const QSet<QString>& ids, QList<int>& indices,
                       QString& parent, QString& category, int& low, int& high, int& total,
                       const QString& axis, const QString& span);
    void
    applyArrangePositions(QJsonArray& entries, const QList<int>& indices, const QString& mode,
                          const QString& axis, const QString& span, int low, int high, int total);
    bool
    executePageGroup(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    validateGroupSelection(const QVariantList& selection, QSet<QString>& ids);
    bool
    findGroupTargets(const QJsonArray& entries, const QSet<QString>& ids, QString& parent, QString& category,
                     int& left, int& top, int& right, int& bottom, int& found);
    bool
    checkGroupBoundsOverlap(const QJsonArray& entries, const QSet<QString>& ids, const QVariantMap& bounds,
                            const QString& parent, const QString& category);
    QJsonObject
    createGroupObject(const QJsonArray& entries, const QJsonObject& document, const QVariantMap& bounds,
                      const QString& parent, const QString& category, const QString& identity);
    QJsonArray
    buildGroupedResult(const QJsonArray& entries, const QSet<QString>& ids, const QJsonObject& group,
                       const QVariantMap& bounds, const QString& parent, const QString& category);
    bool
    executePageUngroup(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    reparentUngroupedChildren(QJsonArray& result, const QJsonArray& entries, const QJsonObject& group,
                              const QJsonObject& outer, const QString& parent, const QString& category);
    bool
    executePageChildLayout(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePagePlace(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    validatePlacementChange(const QVariantMap& change, const QSet<QString>& touched);
    QJsonObject
    applyPlacementChange(QJsonObject entry, const QVariantMap& change, const HomeLayout& worker);
    bool
    checkCategoryEligible(const QVariantMap& change, QJsonObject& entry, const HomeLayout& worker);
    bool
    checkPlacementLocked(const QJsonObject& entry, const QVariantMap& change, const HomeLayout& worker);
    bool
    executePageInsertRelative(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    findInsertionGroup(const QVariantMap& a, const QJsonArray& entries, QString& parent, QJsonObject& group,
                       int& selected, QString& axis, QString& span);
    bool
    applyInsertionOrder(QJsonArray& entries, const QList<int>& order, const QList<int>& gaps,
                        const QString& axis, const QString& span, int from, int to, bool before);
    bool
    executePageOrderRelative(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageConvertGrid(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageState(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageRenameItem(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageRemoveItem(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageDuplicateItem(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    QJsonObject
    findComponentToDuplicate(const QJsonArray& entries, const QString& id);
    bool
    findFreePlacement(QVariantMap& shape, const QJsonArray& entries, const QJsonObject& composition,
                      const QJsonArray& categories, const QString& parent, const QString& category, bool sections);
    QHash<QString, QString>
    remapDuplicateIds(const QJsonArray& entries, const QHash<QString, QString>& remap, QSet<QString>& allocated,
                      const QString& id);
    bool
    executePageReorderItem(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePagePresentationItem(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageAppearanceItem(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageCompositionItem(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageGridItem(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageActionItem(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageAddWidget(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageAddPlacedItem(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageAddButton(QJsonObject& document, HomeLayout& worker);
    bool
    executePageNewPage(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageRenamePageTop(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);
    bool
    executePageRemovePageTop(const QVariantMap& a, QJsonObject& document, HomeLayout& worker);

    // Post-processing shared by all page-scoped operations.
    bool
    finalizePageOperation(QJsonObject& root, QJsonObject& document, QJsonArray& pages, int pageIndex,
                          const QString& operation, const QVariantMap& a, QJsonObject& result);

    QByteArray m_committed;
    QByteArray m_draft;
    QStack<QByteArray> m_undo;
    QStack<QByteArray> m_redo;
    QString m_error;
    QPointer<HomeLayout> m_live;
    QTemporaryDir* m_assetStage = nullptr;
    QHash<QString, QString> m_stagedAssets;
    QHash<QString, QString> m_iconOriginals;
    HomeLayout* m_model = nullptr;
    QString m_profilePath;
    QString m_profileId;
    bool m_active = false;
    HomeLayout* m_proposalModel = nullptr;
    QByteArray m_proposed;
    QByteArray m_proposalBaseline;
    QByteArray m_diskBaseline;
    bool m_diskExisted = false;
    bool m_diskReadable = true;
    QTemporaryDir* m_referenceStage = nullptr;
    QString m_referenceSource;
    bool m_editorLocked = false;
};
