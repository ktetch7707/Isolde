#pragma once

#include "app/AppRegistry.h"

#include <QCache>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QVariant>
#include <QVariantList>

class PageModel;
class StudioSession;

class HomeLayout final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList components READ components NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList categories READ categories NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList sections READ sections NOTIFY layoutChanged)
    Q_PROPERTY(bool brandVisible READ brandVisible NOTIFY layoutChanged)
    Q_PROPERTY(QString backgroundStart READ backgroundStart NOTIFY layoutChanged)
    Q_PROPERTY(QString backgroundEnd READ backgroundEnd NOTIFY layoutChanged)
    Q_PROPERTY(double tileRoundness READ tileRoundness NOTIFY layoutChanged)
    Q_PROPERTY(QVariantMap appearance READ appearance NOTIFY layoutChanged)
    Q_PROPERTY(QVariantMap composition READ composition NOTIFY layoutChanged)
    Q_PROPERTY(QVariantMap grid READ grid NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList placements READ placements NOTIFY layoutChanged)
    Q_PROPERTY(QVariantMap studioOptions READ studioOptions CONSTANT)
    Q_PROPERTY(QString homePageId READ homePageId NOTIFY layoutChanged)
    Q_PROPERTY(QString rootPageName READ rootPageName NOTIFY layoutChanged)
    Q_PROPERTY(bool rootPageVisible READ rootPageVisible NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList studioPages READ studioPages NOTIFY layoutChanged)
    Q_PROPERTY(QVariantMap studioAppearance READ studioAppearance NOTIFY layoutChanged)
    Q_PROPERTY(QVariantMap homeAppearance READ homeAppearance NOTIFY layoutChanged)

  public:
    // Layout, presentation and studio bounds (AGENTS.md: bound everything,
    // never clamp silently). Shared between HomeLayout.cpp and HomePages.cpp.
    static constexpr int MaxPages = 64;                          // pages per profile
    static constexpr int MaxPageNameLength = 80;                 // page / card name characters
    static constexpr int MaxCategoryNameLength = 60;             // category name characters
    static constexpr int DefaultHomeMargin = 24;                 // default home grid margin
    static constexpr int DefaultHomeGap = 16;                    // default home grid gap
    static constexpr int DefaultSidebarWidth = 200;              // default sidebar width (px)
    static constexpr int PresetLibrarySectionHeight = 550;       // preset library section height (px)
    static constexpr int AddLibrarySectionHeight = 500;          // add-widget library section height (px)
    static constexpr int DefaultLibraryColumns = 4;              // default library grid columns
    static constexpr int DefaultLibraryPadding = 12;             // default library section padding (px)
    static constexpr int DefaultLibraryRadius = 16;              // default library section radius (px)
    static constexpr double DefaultPanelOpacity = 0.3;           // default library panel opacity
    static constexpr int MaxTileSymbolLength = 3;                // tile symbol characters
    static constexpr int FirstIdSuffix = 2;                      // first suffix for generated unique ids
    static constexpr qint64 MaxLayoutSize = 4LL * 1024 * 1024;   // 4 MiB layout document cap
    static constexpr qint64 MaxArtworkSize = 20LL * 1024 * 1024; // 20 MiB artwork/file cap
    static constexpr int MaxLayoutCategories = 128;              // categories per layout
    static constexpr double MinTileOpacity = 0.2;                // theme-applied tile opacity floor
    static constexpr double MaxTileOpacity = 1.0;                // theme-applied tile opacity ceiling
    static constexpr double RoundnessNotSet = -1.0;              // sentinel: theme leaves roundness untouched
    static constexpr int MaxStudioDefaultsFileSize = 64 * 1024;  // studio-defaults.json cap
    static constexpr int MinMenuWidth = 200;                     // studio menu width floor (px)
    static constexpr int MaxMenuWidth = 480;                     // studio menu width ceiling (px)
    static constexpr int MaxPanelChildren = 32;                  // studio input panel child cap
    static constexpr int RequiredComponentPresets =
        5; // studio component presets (button, image, library, carousel, app-bar)
    static constexpr int RequiredCatalogEntries = 6; // studio catalog entries
    static constexpr int MaxInspectorFields = 160;   // studio inspector field cap
    // Appearance value ranges (displayed to the user in Settings / Skin Studio).
    static constexpr double MinTypographyScale = 0.8;
    static constexpr double MaxTypographyScale = 1.3;
    static constexpr int MinHomeColumns = 2;
    static constexpr int MaxHomeColumns = 6;
    static constexpr int MinSpacing = 8;
    static constexpr int MaxSpacing = 40;
    static constexpr double MinTextContrast = 4.5;
    static constexpr double MinFocusContrast = 3.0;

  public:
    QString
    homePageId() const
    {
        return m_homePageId;
    }
    QString
    rootPageName() const;
    bool
    rootPageVisible() const
    {
        return m_rootPage.value("visible").toBool(true);
    }
    QVariantList
    studioPages() const;
    QVariantMap
    studioAppearance() const;
    QVariantMap
    homeAppearance() const;
    Q_INVOKABLE QVariantMap
    resolvedSurfacePalette(const QVariantMap& appearance) const;
    Q_INVOKABLE QVariantMap
    surfaceAppearance(const QString& surface, const QString& originId = {}) const;
    bool
    load(const QString& defaultPath = {}, const QString& userPath = {});
    Q_INVOKABLE bool
    addAppTile(const QString& appId);
    Q_INVOKABLE bool
    addActionTile(const QString& name,
                  const QString& actionType,
                  const QString& target,
                  const QString& categoryId = {},
                  const QString& openWith = {});
    bool
    validateActionTileParams(const QString& name, const QString& actionType, const QString& target,
                             const QString& categoryId, const QString& openWith) const;
    bool
    validateActionTileTarget(const QString& openWith, const QString& actionType) const;
    bool
    validateCategoryIdExists(const QString& categoryId) const;
    bool
    validateActionTileBasic(const QString& cleanName, const QString& cleanTarget,
                            const QString& cleanActionType, const QString& cleanOpenWith, bool captureInput) const;
    QJsonObject
    buildActionTile(const QString& name, const QString& actionType, const QString& target,
                    const QString& categoryId, const QJsonObject& normalizedAction);
    Q_INVOKABLE bool
    removeAppTile(const QString& appId);
    Q_INVOKABLE bool
    moveItem(const QString& id, int offset);
    Q_INVOKABLE bool
    removeItem(const QString& id);
    Q_INVOKABLE bool
    addWidget(const QString& widgetType);
    Q_INVOKABLE bool
    setItemSize(const QString& id, const QString& size);
    Q_INVOKABLE bool
    setItemOpacity(const QString& id, double opacity);
    Q_INVOKABLE bool
    setTileOption(const QString& id, const QString& option, bool enabled);
    Q_INVOKABLE bool
    setTileAppearance(const QString& id, const QString& symbol, const QString& accent);
    Q_INVOKABLE bool
    setTileLogo(const QString& id, const QString& sourceUrl);
    Q_INVOKABLE bool
    clearTileLogo(const QString& id);
    Q_INVOKABLE QString
    addCategory(const QString& name);
    Q_INVOKABLE bool
    renameCategory(const QString& categoryId, const QString& name);
    Q_INVOKABLE bool
    removeCategory(const QString& categoryId);
    Q_INVOKABLE bool
    moveCategory(const QString& categoryId, int offset);
    Q_INVOKABLE bool
    assignItemCategory(const QString& itemId, const QString& categoryId);
    Q_INVOKABLE bool
    setBrandVisible(bool visible);
    Q_INVOKABLE bool
    setBackground(const QString& start, const QString& end);
    Q_INVOKABLE bool
    setTileRoundness(double roundness);
    Q_INVOKABLE bool
    applyTheme(const QVariantMap& theme, bool includeSuggestedArtwork = false);
    QVariantMap
    prepareThemePresentation(const QVariantMap& theme);
    bool
    validateThemeColors(const QVariantMap& theme, QString& startColor, QString& endColor,
                        double& roundness, double& tileOpacity);
    void
    applyThemeArtwork(const QVariantMap& theme, double tileOpacity, bool includeSuggestedArtwork);
    bool
    commitTheme(const QVariantMap& theme, const QString& startColor, const QString& endColor,
                double roundness, const QVariantMap& presentation);
    Q_INVOKABLE bool
    resetToDefaults();
    [[nodiscard]] QVariantList
    skins() const;
    // Expose asset-free starter designs for the new-page chooser.
    Q_INVOKABLE QVariantList pageStarterChoices() const;
    Q_INVOKABLE QString
    saveSkin(const QString& name);
    Q_INVOKABLE bool
    applySkin(const QString& id);
    Q_INVOKABLE QVariantList
    skinPageUpdates(const QString& id) const;
    bool
    validateSkinPageUpdateRecipe(const QJsonObject& recipe) const;
    QJsonObject
    findSkinPageLayout(const QJsonObject& skin, const QString& pageId) const;
    bool
    validateSkinPageArtwork(const QJsonObject& target) const;
    QVariantList
    buildSkinPageUpdateResults(const QJsonObject& skin, const QJsonArray& recipes,
                               const QJsonArray& homeComponents) const;
    Q_INVOKABLE bool
    applySkinPageUpdates(const QString& id);
    Q_INVOKABLE bool
    renameSkin(const QString& id, const QString& name);
    Q_INVOKABLE bool
    deleteSkin(const QString& id);
    QVariantList
    pages() const
    {
        return m_pages.toVariantList();
    }
    bool
    isPage() const
    {
        return m_owner != nullptr;
    }
    Q_INVOKABLE QString
    createPage(const QString& name, const QString& preset = "blank");
    Q_INVOKABLE bool
    renamePage(const QString& id, const QString& name);
    Q_INVOKABLE bool
    removePage(const QString& id);
    Q_INVOKABLE QObject*
    pageModel(const QString& id);
    Q_INVOKABLE bool
    linkPage(const QString& id, bool asTab);
    Q_INVOKABLE QString
    addLibraryWidget(const QString& name, const QString& kind = "movies");
    Q_INVOKABLE bool
    setItemAction(const QString& id, const QVariantList& actions);
    Q_INVOKABLE bool
    setItemName(const QString& id, const QString& name);
    Q_INVOKABLE bool
    addButton();

    Q_PROPERTY(QString wallpaperSource READ wallpaperSource NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList skins READ skins NOTIFY skinsChanged)
    Q_PROPERTY(QVariantList pages READ pages NOTIFY layoutChanged)
    Q_PROPERTY(bool isPage READ isPage CONSTANT)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

  public:
    explicit HomeLayout(AppRegistry* registry, QObject* parent = nullptr);
    [[nodiscard]] QVariantList
    items() const;
    [[nodiscard]] QVariantList
    components() const;
    void
    setCollections(const QVariantList& collections);
    void
    setCaptureInputsReady(bool ready);
    Q_INVOKABLE QVariantMap
    responsivePresentation(const QVariantMap& value, double width, double height, double unit = 1) const;
    Q_INVOKABLE QVariantList
    sourceChoices(const QString& itemId) const;
    Q_INVOKABLE bool
    rebindSource(const QString& itemId, const QString& sourceId);
    [[nodiscard]] AppRegistry*
    registry() const
    {
        return m_registry;
    }
    [[nodiscard]] QVariantList
    categories() const;
    [[nodiscard]] QVariantList
    sections() const;
    [[nodiscard]] bool
    brandVisible() const;
    [[nodiscard]] QString
    backgroundStart() const;
    [[nodiscard]] QString
    backgroundEnd() const;
    [[nodiscard]] double
    tileRoundness() const;
    [[nodiscard]] QVariantMap
    appearance() const;
    Q_INVOKABLE bool
    setAppearance(const QVariantMap& changes);
    [[nodiscard]] QVariantMap
    composition() const
    {
        return m_composition;
    }
    [[nodiscard]] QVariantList
    placements() const;
    Q_INVOKABLE QVariantMap
    grid() const;
    [[nodiscard]] QVariantMap
    studioOptions() const;
    Q_INVOKABLE bool
    setGrid(const QVariantMap& value);
    Q_INVOKABLE bool
    setItemPlacement(const QString& id, const QVariantMap& placement);
    Q_INVOKABLE bool
    clearItemPlacement(const QString& id);
    Q_INVOKABLE bool
    duplicateItem(const QString& id);
    Q_INVOKABLE bool
    setItemPresentation(const QString& id, const QVariantMap& value);
    Q_INVOKABLE bool
    setSectionPresentation(const QString& id, const QVariantMap& value);
    Q_INVOKABLE QVariantList
    gridCollisions(const QString& itemId, const QVariantMap& placement) const;
    [[nodiscard]] QString
    wallpaperSource() const;
    Q_INVOKABLE QString
    resolveArtwork(const QString& relative) const;
    Q_INVOKABLE QVariantList
    captureArtworkChoices() const;
    Q_INVOKABLE QString
    captureArtworkSource(int input) const;
    bool
    setCaptureInputArtwork(const QVariantMap& artwork);
    Q_INVOKABLE QVariantMap
    artworkAnimation(const QString& relative, const QString& owner);
    Q_INVOKABLE void
    releaseArtworkAnimation(const QString& owner);
    Q_INVOKABLE bool
    setWallpaper(const QString& sourceUrl);
    Q_INVOKABLE bool
    setComposition(const QVariantMap& value);
    Q_INVOKABLE bool
    setGalleryFavorite(const QString& appId, bool favorite);

    // T16 Studio bridge: committed bytes + atomic draft apply + focus recap.
    // committedDocument() exposes the current bytes for session.open();
    // applyStudioDraft() validates the candidate, swaps it in memory, and
    // persists through the existing atomic save() (temp → fsync → rename →
    // rollback), restoring the previous model on failure. focusOrigin()
    // restores the exact canvas origin after apply/recovery dialogs.
    Q_INVOKABLE QByteArray
    committedDocument() const;
    static bool
    validateStudioDocument(const QByteArray& data, QString* error = nullptr, const QVariantMap& defaults = {});
    Q_INVOKABLE QObject*
    beginStudio();
    Q_INVOKABLE bool
    pageAllowed(const QString& id) const;
    Q_INVOKABLE bool
    applyStudioDraft(const QByteArray& draft, const QString& focusComponentId = {});
    Q_INVOKABLE QString
    focusOrigin() const
    {
        return m_focusOrigin;
    }
    Q_INVOKABLE void
    setFocusOrigin(const QString& id)
    {
        m_focusOrigin = id;
    }

    [[nodiscard]] QString
    errorString() const;
    void
    setError(const QString& message);

  signals:
    void
    layoutChanged();
    void
    skinsChanged();
    void
    errorStringChanged();

  private:
    friend class PageModel;
    friend class StudioSession;
    bool
    loadDocument(const QByteArray& data, bool userDocument);
    bool
    validateDocumentStructure(const QByteArray& data, bool userDocument, QJsonArray& entries);
    bool
    loadAndValidatePages(const QJsonObject& object, QSet<QString>& pageIds, QJsonArray& pages);
    bool
    validateStudioAppearance(const QJsonObject& object, QVariantMap& editorAppearance);
    bool
    validateRootPage(const QJsonObject& object, const QJsonObject& rootPage, const QJsonArray& pages,
                     const QSet<QString>& pageIds, const QJsonValue& homePage);
    bool
    validateHomePage(const QJsonObject& object, const QJsonValue& homePage, const QSet<QString>& pageIds);
    bool
    validateComposition(const QJsonObject& object, QVariantMap& composition);
    QVariantMap
    buildAppearance(const QJsonObject& theme);
    bool
    validateShellReadability(const QJsonArray& pages, const QJsonValue& homePage,
                             const QVariantMap& presentation, const QColor& start, const QColor& end);
    bool
    extractThemeColors(const QJsonObject& theme, QColor& start, QColor& end);
    bool
    validateRootPageCondition(const QJsonObject& object, const QJsonObject& rootPage,
                              const QJsonArray& pages, const QSet<QString>& pageIds,
                              const QJsonValue& homePage);
    bool
    validateDocumentPrerequisites(const QJsonObject& object, const QJsonArray& pages,
                                  const QSet<QString>& pageIds, const QJsonValue& homePage);
    bool
    validateCategoriesAndComponents(
                                    const QJsonArray& categoryArray, const QJsonArray& entries,
                                    QJsonArray& components);
    bool
    validateCategoryItem(const QJsonObject& category, QSet<QString>& categoryIds) const;
    bool
    validateComponentItem(const QJsonObject& entry, const QSet<QString>& categoryIds,
                          QSet<QString>& ids, QJsonObject& item, QString& componentError) const;
    bool
    isValidComponentKind(const QString& kind) const;
    bool
    isValidComponentAction(const QString& kind, const QString& actionType, const QJsonObject& item) const;
    bool
    isValidComponentCategoryRef(const QJsonObject& item, const QSet<QString>& categoryIds) const;
    bool
    isValidComponentOpacity(const QJsonObject& item) const;
    bool
    validateCategories(const QJsonArray& categoryArray, QSet<QString>& categoryIds);
    bool
    save();
    QByteArray
    documentData() const;
    QString
    skinDirectory() const;
    QJsonObject
    skinSnapshot(const QString& id) const;
    QVariantMap
    componentPresentation(const QVariantMap& authored) const;
    bool
    applySkinDocument(QJsonObject document, const QJsonObject& assets, bool addMissingPages = false);
    bool
    applySkinAddMissingPages(QJsonObject& document, QJsonArray& mergedPages);
    QJsonObject
    applySkinWrapSinglePage(QJsonObject document, const QJsonArray& mergedPages, const QByteArray& previous);
    bool
    applySkinValidateArtwork(QJsonObject& document, const QJsonObject& assets) const;
    bool
    isValidSkin(const QJsonObject& skin);
    bool
    storePage(const QString& id, const QJsonObject& document);
    void
    syncPages();
    QString
    pageKey(const QString& id) const;
    bool
    isPageId(const QString& id) const;
    QSet<QString>
    computeHiddenIds() const;
    bool
    appendItem(const QJsonValue& value, QVariantList& result, const QSet<QString>& hidden) const;
    void
    setItemKindFields(QVariantMap& item) const;
    void
    setItemAppFields(QVariantMap& item) const;
    void
    setItemActionFields(QVariantMap& item) const;
    void
    setItemLogo(QVariantMap& item, const QVariantList& actionList) const;
    void
    setItemUnavailableReason(QVariantMap& item, const QJsonObject& object, const QVariantList& actionList) const;
    void
    unavailableReasonFromSchema(QVariantMap& item, const QVariantList& actionList) const;
    void
    unavailableReasonFromSwitchInput(QVariantMap& item, const QVariantList& actionList) const;
    void
    unavailableReasonFromCapability(QVariantMap& item, const QJsonObject& object) const;
    QString
    unavailableReasonValue(const QVariantMap& item) const;
    void
    resolveOriginInfo(const QString& surface, const QString& originId, const QVariantMap& home,
                      QVariantMap& origin, QVariantMap& originTheme, QString& originName) const;
    QVariantMap
    surfacePolicy(const QVariantMap& originTheme, const QString& surface) const;
    QVariantMap
    buildSurfaceAppearance(const QVariantMap& home, const QVariantMap& origin,
                           const QVariantMap& originTheme, const QString& originName,
                           const QString& surface, const QVariantMap& policy) const;
    QString
    sourceForPage(const QString& surface) const;
    bool
    validateNumericKeyDefaults(const QString& key, const QVariantMap& defaults);
    bool
    validateNumericValueType(const QVariant& value);
    double
    numericRangeLow(const QString& key);
    double
    numericRangeHigh(const QString& key);
    bool
    isValidNumber(double number, bool ok, double low, double high, const QString& key);
    bool
    validateSurfaceColors(QVariantMap values);
    bool
    validateFocusColor(QVariantMap values);
    bool
    validateStudioAppIconStyles(const QJsonObject& styles);
    bool
    validateStudioArtworkCleanup(const QJsonObject& cleanup);
    bool
    validateStudioReferenceViewport(const QJsonObject& viewport);
    bool
    validateStudioColorPicker(const QJsonObject& picker);
    bool
    validateStudioDesktop(const QJsonObject& desktop);
    bool
    validateStudioThemeGroups(const QJsonArray& groups);
    bool
    validateStudioSchemes(const QJsonArray& schemes);
    bool
    validateStudioStylePresets(const QJsonArray& presets);
    bool
    validateStudioTrialDurations(const QJsonArray& trials);
    bool
    validateStudioMenuWidth(const QJsonValue& value);
    bool
    validateStudioSectionHeight(const QJsonValue& value);
    bool
    validateStudioAppPicker(const QJsonObject& picker);
    bool
    validateStudioWidgetSpans(const QJsonObject& spans);
    bool
    validateStudioHeaderDefaults(const QJsonObject& defaults);
    bool
    validateStudioSettings(const QJsonObject& object);
    bool
    validateDesktopStringKeys(const QJsonObject& obj, const QStringList& keys);
    bool
    validateDesktopNumericKeys(const QJsonObject& obj, const QStringList& keys);
    void
    appendSourceChoicesForType(QVariantList& choices, const QString& type) const;
    void
    appendHdmiInputChoices(QVariantList& choices) const;
    void
    appendCollectionChoices(QVariantList& choices) const;
    void
    appendAppChoices(QVariantList& choices, const QString& type) const;
    bool
    sourceIsCompatible(const QString& itemId, const QString& sourceId) const;
    bool
    rebindItemAt(qsizetype i, QJsonObject item, const QString& sourceId);
    QJsonObject
    validateStudioCatalog(const QVariantList& catalog, const QVariantMap& presets);
    bool
    validateStudioActions(const QVariantList& actions);
    bool
    validateStudioActionCategories(const QVariantList& categories);
    bool
    validateStudioGridModes(const QVariantList& modes, const QVariantMap& defaultGrid);
    bool
    validateStudioOverlayKeys(const QVariantList& overlayKeys, const QVariantMap& guide);
    bool
    validateStudioComponentVariants(const QJsonObject& object, const QJsonObject& presets) const;
    bool
    validateStudioVariantFamily(const QString& familyKey, const QJsonArray& variants, const QJsonObject& presets) const;
    bool
    validateStudioVariant(const QJsonObject& variant, const QString& familyKey, const QJsonObject& presets,
                          QSet<QString>& variantsSeen) const;
    bool
    validateStudioPresets(const QJsonObject& presets) const;
    bool
    validateStudioShapeChoices(const QJsonObject& object, const QJsonObject& presets) const;
    bool
    validateStudioReplacementChoices(const QJsonObject& object) const;
    bool
    validateStudioDefaults(const QJsonObject& object, QVariantMap& guide);
    bool
    validateStudioBasicSettings(const QJsonObject& object);
    bool
    validateStudioThemeSettings(const QJsonObject& object);
    bool
    validateStudioLayoutSettings(const QJsonObject& object);
    bool
    validateStudioContentSettings(const QJsonObject& object, const QJsonObject& presets);
    bool
    finalizeStudioOptions(QVariantMap& result, QVariantMap guide) const;
    bool
    validateStudioPanelPreset(const QJsonObject& object) const;
    bool
    validateStudioActionsList(const QJsonArray& actions) const;
    bool
    validateStudioCatalogIds(const QJsonObject& object, const QJsonObject& presets) const;
    bool
    validateStudioActionCategoryTypes(const QJsonArray& categories) const;
    bool
    validateStudioActionCategory(const QJsonObject& category, const QStringList& pickerTypes) const;
    bool
    validateStudioGridModeIds(const QJsonArray& modes) const;
    bool
    applyStudioDefaults(QVariantMap& result, const QVariantMap& guide) const;
    bool
    validateStudioActions(const QVariantList& actions, const QVariantMap& listings);
    bool
    validateStudioActionCategories(const QVariantList& categories, const QVariantMap& listings);
    QJsonObject
    previousItem(qsizetype i) const;
    void
    setAppId(QVariantMap& first, QJsonObject& item, const QString& sourceId);
    QVariantList
    resolveCategoryItems(const QVariantList& resolved, const QJsonObject& category) const;
    QVariantList
    resolveUncategorized(const QVariantList& resolved) const;
    void
    appendRootPage(QVariantList& result) const;
    QJsonArray m_pages;
    QString m_homePageId;
    QJsonObject m_rootPage;
    QVariantMap m_studioAppearance;
    QHash<QString, PageModel*> m_pageModels;
    HomeLayout* m_owner = nullptr;
    QString m_pageId;

    AppRegistry* m_registry;
    QVariantList m_registryItems;
    QJsonArray m_items;
    QJsonArray m_categories;
    bool m_brandVisible = false;
    QString m_backgroundStart = QStringLiteral("#111A32");
    QString m_backgroundEnd = QStringLiteral("#080C18");
    const QString m_initialBackgroundStart = m_backgroundStart;
    const QString m_initialBackgroundEnd = m_backgroundEnd;
    double m_tileRoundness = 0.18;
    QVariantMap m_appearance;
    QSet<QString> m_themeKeys;
    QVariantMap m_defaultTheme;
    QVariantMap m_composition;
    QString m_defaultPath;
    mutable QString m_optionsPath;
    mutable QByteArray m_optionsBytes;
    mutable QVariantMap m_optionsCache;
    mutable bool m_optionsCached = false;
    QString m_userPath;
    QString m_errorString;
    QVariantList m_collections;
    bool m_captureInputsReady = false;
    QString m_focusOrigin;
    bool m_draftOnly = false;
    QHash<QString, QString> m_assetPaths;
    QSet<QString> m_animationOwners;
    mutable QCache<QString, QString> m_gifStills{8 * 1024 * 1024};
    QString
    managedArtworkSource(const QString& relative) const;
    StudioSession* m_studio = nullptr;
};
