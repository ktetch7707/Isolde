#pragma once
#include <QVariantList>
#include <QVariantMap>
class AppRegistry;

/// Data contracts only. No process launching, networking or Kodi mutations.
/// Every validator returns true only for documents that satisfy the layout
/// bounds declared in ComponentSchema.cpp; values are never clamped silently.
namespace ComponentSchema {

/// Validates the Settings navigation document.
/// @brief Validates a Settings navigation document: exact category set and route table, bounded labels and descriptions.
/// @details Ensures the navigation map has exactly schemaVersion=1 and categories list, that every category has a unique
/// id from the allowed set, a non-empty name/description within length limits, and either a route or rows. Every route
/// across all categories must be unique and drawn from the known route set.
/// @why The Settings navigation is shared state consumed by the Settings page; malformed entries would cause missing
/// navigation items or ambiguous routing, so it must be validated strictly before being stored.
/// @param value The navigation document to validate.
/// @return true if the document is valid, false otherwise.
bool
validSettingsNavigation(const QVariantMap& value);

/// Validates a source policy document.
/// @brief Validates a source policy document: bounded duration, known mode, unique page/slot ids, known event types,
/// boolean flags and optional detected source/appliance sub-maps.
/// @details Iterates every field in the policy map and dispatches to the appropriate field validator. Ensures
/// schemaVersion and duration are valid numbers, mode is one of off/compact/widget, pages/slots are unique safe ids,
/// events/types are known media event kinds, boolean flags are actually booleans, and optional detected source /
/// appliance sub-maps pass their own validation.
/// @why Source policies define how Isolde interprets incoming media events; invalid policies could cause incorrect
/// source detection or unbounded data, so they must be validated before storage.
/// @param value The source policy document to validate.
/// @return true if the document is valid, false otherwise.
bool
validSourcePolicy(const QVariantMap& value);

/// Validates a numeric source policy field.
/// @brief Validates a numeric source policy field (schemaVersion or duration).
/// @details Checks that the value is a numeric type (Int, UInt, LongLong, ULongLong, or Double), is finite, and is a
/// whole number. For schemaVersion, requires exactly 1. For duration, requires a value between 5 and 60 inclusive.
/// @why Numeric fields in source policies must be exact to avoid misinterpretation of timing or schema compatibility.
/// @param value The variant value to validate.
/// @param key The field name being validated ("schemaVersion" or "duration").
/// @return true if the value is a valid number within bounds for the given key.
bool
validSourcePolicyNumeric(const QVariant& value, const QString& key);

/// Validates the source policy mode field.
/// @brief Validates the source policy mode field ("off", "compact" or "widget").
/// @details Checks that the value is a QString and that it matches one of the three known modes: "off",
/// "compact", or "widget".
/// @why The mode determines how sources are displayed in the home UI; an unknown mode would cause rendering errors.
/// @param value The mode value to validate.
/// @return true if the mode is one of the allowed values, false otherwise.
bool
validSourcePolicyMode(const QVariant& value);

/// Validates a source policy page or slot id list.
/// @brief Validates a source policy page or slot id list: unique safe ids.
/// @details Checks that the value is a QVariantList, its size is within bounds (64 for pages/slots), each element is a
/// QString, all ids are unique, and each id passes safeId validation (except empty ids allowed for "pages").
/// @why Page and slot ids are used as lookup keys; duplicates or unsafe ids would cause lookup failures or
/// inconsistent state.
/// @param value The variant list to validate.
/// @param key The field name being validated ("pages" or "slots").
/// @return true if the list is valid, false otherwise.
bool
validSourcePolicyIdList(const QVariant& value, const QString& key);

/// Validates a source policy event type list.
/// @brief Validates a source policy event type list: known media event kinds.
/// @details Checks that the value is a QVariantList with size at most 4, each element is a QString, all values are
/// unique, and each matches one of the known media event kinds ("movie-disc", "game-disc", "media-folder",
/// "file-source").
/// @why Event types determine which media events trigger source actions; unknown types would cause silent failures.
/// @param value The variant list to validate.
/// @param key The field name being validated ("events" or "types").
/// @return true if the list is valid, false otherwise.
bool
validSourcePolicyEventTypeList(const QVariant& value, const QString& key);

/// Validates a source policy boolean flag field.
/// @brief Validates a source policy boolean flag field.
/// @details Checks that the variant holds a QMetaType::Bool value.
/// @why Boolean flags in source policies control behavior; a non-boolean value would be misinterpreted.
/// @param value The variant value to validate.
/// @return true if the value is a boolean, false otherwise.
bool
validSourcePolicyBool(const QVariant& value);

/// Validates a detected source sub-map (id, type, containment).
bool
validSourcePolicyDetectedSource(const QVariantMap& value);

/// Validates an appliance sub-map (id, type, distribution mode).
bool
validSourcePolicyAppliance(const QVariantMap& value);

/// Dispatches a source policy field to its field-specific validator.
bool
validSourcePolicyField(const QString& key, const QVariant& v);

/// Validates a detected source field.
/// @brief Validates a detected source field (map wrapper).
/// @details Checks that the variant is a QVariantMap and delegates to validSourcePolicyDetectedSource for full
/// validation of id, type, and containment fields.
/// @why Detected sources represent physical media inputs; invalid identities could cause incorrect source mapping.
/// @param v The variant value to validate.
/// @return true if the value is a valid detected source map, false otherwise.
bool
validSourcePolicyDetectedSourceField(const QVariant& v);

/// Validates an appliance field.
/// @brief Validates an appliance field (map wrapper).
/// @details Checks that the variant holds a QVariantMap and delegates to validSourcePolicyAppliance for full
/// validation of id, type, and distribution mode fields.
/// @why Appliance definitions map physical devices to Isolde; invalid maps would cause incorrect device handling.
/// @param v The variant value to validate.
/// @return true if the value is a valid appliance map, false otherwise.
bool
validSourcePolicyApplianceField(const QVariant& v);

/// Validates child layout keys only.
/// @brief Validates child layout keys only (known key set).
/// @details Checks that the map contains only keys from the known set: schemaVersion, mode, columns, rows, gap,
/// padding, visibleColumns, wrap. Any unknown key causes validation failure.
/// @why Unknown keys in child layout maps could indicate malformed data or future fields not yet supported.
/// @param value The map to validate.
/// @return true if all keys are known, false otherwise.
bool
validChildLayoutKeys(const QVariantMap& value);

/// Validates child layout schemaVersion.
/// @brief Validates child layout schemaVersion (must be numeric 1).
/// @details Checks that the map contains a schemaVersion field that is numeric (not QString or Bool) and equals 1.
/// @why The schemaVersion identifies the layout format version; only version 1 is currently supported.
/// @param value The child layout map.
/// @return true if schemaVersion is present and equals 1, false otherwise.
bool
validChildLayoutVersion(const QVariantMap& value);

/// Validates child layout mode ("grid", "row" or "column").
bool
validChildLayoutMode(const QVariantMap& value);

/// Validates a child layout dimension field.
/// @brief Validates a child layout dimension field (columns, rows, gap or padding).
/// @details Checks that the map contains the key, the value is numeric (not QString or Bool), finite, within bounds
/// (columns: 4-16 for grid or 4-128 for row, rows: 4-10, gap/padding: 0-100), and for columns/rows that it is a whole
/// number.
/// @why Dimensions must be whole numbers within platform limits to avoid layout overflow or rendering issues.
/// @param value The child layout map.
/// @param key The dimension field name ("columns", "rows", "gap", or "padding").
/// @return true if the dimension is valid, false otherwise.
bool
validChildLayoutDimension(const QVariantMap& value, const QString& key);

/// Validates child layout dimension type.
/// @brief Validates child layout dimension type (not string or bool).
/// @details Checks that the value's meta type is not QString and not Bool.
/// @why Dimensions must be numeric types; strings or booleans would be misinterpreted as layout measurements.
/// @param value The variant map containing the dimension.
/// @param key The dimension field name.
/// @return true if the type is numeric, false otherwise.
bool
validChildLayoutDimensionType(const QVariantMap& value, const QString& key);

/// Validates child layout dimension range.
/// @brief Validates child layout dimension range (bounded to grid limits).
/// @details Parses the value as a double and checks it is finite, within the key-specific bounds
/// (columns: 4-16 grid/4-128 row, rows: 4-10, gap/padding: 0-100), and whole-number for columns/rows.
/// @why Out-of-range dimensions would cause layouts that overflow the screen or violate grid constraints.
/// @param value The child layout map.
/// @param key The dimension field name.
/// @return true if the dimension is within bounds, false otherwise.
bool
validChildLayoutDimensionRange(const QVariantMap& value, const QString& key);

/// Validates child layout wrap field.
/// @brief Validates child layout wrap field (boolean or absent).
/// @details If present, checks that the value is a QMetaType::Bool.
/// @why Wrap controls text wrapping behavior; non-boolean values would be ambiguous.
/// @param value The child layout map.
/// @return true if wrap is absent or boolean, false otherwise.
bool
validChildLayoutWrap(const QVariantMap& value);

/// Validates child layout visibleColumns field.
/// @brief Validates child layout visibleColumns field (row mode only).
/// @details Checks that if visibleColumns is present, it is numeric (not QString or Bool), finite, a whole number,
/// between MinChildColumns and the columns value, and only applies when mode is "row".
/// @why visibleColumns limits how many items show per row; invalid values would cause broken row layouts.
/// @param value The child layout map.
/// @return true if visibleColumns is valid or absent, false otherwise.
bool
validChildLayoutVisibleColumns(const QVariantMap& value);

/// Validates child layout mode consistency.
/// @brief Validates child layout mode/columns/rows consistency.
/// @details Checks that row mode has rows=1 and column mode has columns=1.
/// @why Row mode only makes sense with a single row, and column mode only with a single column.
/// @param value The child layout map.
/// @return true if mode is consistent with columns/rows, false otherwise.
bool
validChildLayoutModeConsistency(const QVariantMap& value);

/// Shell policies are layout data. Missing policies inherit the opening page;
/// Settings always uses designated Home, including its explicit custom policy.
bool
validSettingsNavigation(const QVariantMap& value);

/// Validates settings navigation schemaVersion (must be numeric 1).
bool
validSettingsNavigationVersion(const QVariantMap& value);

/// Validates settings navigation categories list (exact set of 10 categories).
bool
validSettingsNavigationCategories(const QVariantMap& value);

/// Validates a single settings navigation category (id, name, description,
/// rows or route).
bool
validSettingsNavigationCategory(const QVariantMap& category, QSet<QString>& seen,
                                QSet<QString>& seenRoutes);

/// Validates a settings navigation row (name, description, unique route).
bool
validSettingsNavigationRow(const QVariantMap& row, QSet<QString>& seenRoutes);

/// Validates a settings navigation label (non-empty, bounded size).
bool
validSettingsNavigationLabel(const QVariant& label, int limit);

/// Versioned native-channel definitions and bounded nested rule trees.
QVariantMap
guideChannel(const QVariantMap& value, QString* error = nullptr);

/// Validates a guide rule document (bounded fields).
QVariantMap
guideRule(const QVariantMap& value, QString* error = nullptr);

/// Validates a guide options document (bounded fields).
QVariantMap
guideOptions(const QVariantMap& value, QString* error = nullptr);

/// Validates a game registration document (title, platform, runtime, root,
/// target, optional steam app id).
QVariantMap
gameRegistration(const QVariantMap& value, QString* error = nullptr);

/// Validates game registration key set (exact six keys).
bool
gameRegistrationKeys(const QVariantMap& input);

/// Validates game registration schemaVersion (numeric 1).
bool
gameRegistrationVersion(const QVariantMap& input);

/// Validates game registration text fields (title, platform, runtimeId, root,
/// target).
bool
gameRegistrationTextFields(const QVariantMap& input);

/// Validates game registration title/platform fields.
bool
gameRegistrationTitlePlatform(const QVariantMap& input);

/// Validates a steam app id target.
bool
gameRegistrationSteam(const QVariantMap& input, QString* error);

/// Validates game registration root/target pair.
bool
gameRegistrationRootTarget(const QVariantMap& input);

// Logical page extent is independent of the bounded TV viewport grid.
inline constexpr int MaxScrollableRows = 10240;

// Validates a component/section presentation map. When section is true the
// map is a page section; when container is true it is a group container.
// Allowed state keys: normal, focused, selected, pressed, disabled, unavailable.
bool
validPresentation(const QVariantMap& value, bool section = false, bool container = false);

/// Dispatches a presentation field key to its field-specific validator.
bool
validPresentationField(const QString& key, const QVariant& v, const QVariantMap& value,
                       bool section, bool container);

// Validates a child layout map (grid/row/column mode with bounded columns,
// rows, gap and padding).
bool
validChildLayout(const QVariantMap& value);

// Validates a page composition document: navigation tabs, sections, grid,
// background and header settings.
bool
validComposition(const QVariantMap& value);

/// Validates composition keys only.
/// @brief Validates composition keys only (known key set).
/// @details Checks that every key in the map is from the known set of composition fields: schemaVersion, enabled,
/// navigation, initialTab, margin, gap, fit, showProfile, columns, wallpaper, headerMode, identity,
/// headerControls, showClock, designHeight, pageNavigation, sidebarWidth, moviePage, showsPage, grid,
/// sectionHeight, background, verticalScrolling, header, designLibrary, appGallery, captureInputArtwork,
/// settingsCategoryOrder.
/// @why Unknown keys in composition maps would indicate malformed data or unsupported features.
/// @param value The composition map.
/// @return true if all keys are known, false otherwise.
bool
validCompositionKeys(const QVariantMap& value);

/// Validates settingsCategoryOrder (exact category set, unique).
bool
validCompositionSettingsCategoryOrder(const QVariantMap& value);

/// Validates captureInputArtwork (bounded artwork map).
bool
validCompositionCaptureInputArtwork(const QVariantMap& value);

/// Validates appGallery (bounded gallery presentation map).
bool
validCompositionAppGallery(const QVariantMap& value);

/// Validates composition header map with geometry bounds.
/// @brief Validates composition header map with geometry bounds.
/// @details Checks that header is a QVariantMap, schemaVersion is numeric 1, all known header fields (height, gap,
/// padding, titleSize, clockSize, dateSize, clockWidth, controlSize, controlWidth, iconSize, radius, opacity,
/// focusWidth) are within bounds, position/showTitle/showDate are booleans, colors are valid opaque colors, and
/// geometry is sufficient for readable controls.
/// @why Header geometry must leave enough space for readable controls; invalid geometry would cause obscured UI.
/// @param header The header map to validate.
/// @param value The parent composition map for context.
/// @return true if the header is valid, false otherwise.
bool
validCompositionHeader(const QVariantMap& header, const QVariantMap& value);

/// Validates composition background map.
/// @brief Validates composition background map (mode, dim, artFit, tint).
/// @details Checks that background is a QVariantMap, mode is one of solid/gradient/image, dim is numeric 0-1,
/// tint is a valid opaque color, and remaining fields pass validPresentation validation.
/// @why Background appearance affects visual legibility; invalid values would cause rendering errors.
/// @param value The composition map containing the background field.
/// @return true if background is valid, false otherwise.
bool
validCompositionBackground(const QVariantMap& value);

/// Validates composition background keys only.
/// @brief Validates composition background keys only.
/// @details Checks that every key in the background map is from the known set: mode, dim, artFit, artFocalX,
/// artFocalY, artZoom, artAspectLocked, imageCrop, tintColor.
/// @why Unknown background keys would indicate malformed data or unsupported features.
/// @param background The background map to validate.
/// @return true if all keys are known, false otherwise.
bool
validCompositionBackgroundKeys(const QVariantMap& background);

/// Validates composition background mode.
/// @brief Validates composition background mode ("solid", "gradient", "image").
/// @details Checks that the value is a QString and matches one of "solid", "gradient", or "image".
/// @why The background mode determines how background art is rendered; an unknown mode would cause rendering errors.
/// @param background The background map.
/// @return true if mode is valid, false otherwise.
bool
validCompositionBackgroundMode(const QVariantMap& background);

/// Validates composition background tint color.
/// @brief Validates composition background tint color (opaque named color).
/// @details Checks that the value is a QString and represents a valid QColor with alpha 255 (fully opaque).
/// @why Tint colors must be fully opaque to maintain visual consistency of background overlays.
/// @param tint The tint color value to validate.
/// @return true if the tint is a valid opaque color, false otherwise.
bool
validCompositionBackgroundTint(const QVariant& tint);

/// Validates composition background dim value.
/// @brief Validates composition background dim value (0–1).
/// @details Checks that the value is numeric (not QString or Bool), finite, and between 0 and 1 inclusive.
/// @why Dim values outside [0,1] would cause background dimming to be invisible or excessive.
/// @param dim The dim value to validate.
/// @return true if dim is within [0,1], false otherwise.
bool
validCompositionBackgroundDim(const QVariant& dim);

/// Validates composition pageNavigation.
/// @brief Validates composition pageNavigation ("left", "top" or "none").
/// @details Checks that the value is a QString and matches one of "left", "top", or "none".
/// @why Page navigation position affects UI layout; an unknown position would cause navigation to be misplaced.
/// @param value The composition map.
/// @return true if pageNavigation is valid, false otherwise.
bool
validCompositionPageNavigation(const QVariantMap& value);

/// Validates composition sidebarWidth.
/// @brief Validates composition sidebarWidth (120–400).
/// @details Checks that the value is numeric (not QString or Bool) and between 120 and 400 inclusive.
/// @why Sidebar width affects sidebar layout; out-of-range values would cause layout overflow.
/// @param value The composition map.
/// @return true if sidebarWidth is valid, false otherwise.
bool
validCompositionSidebarWidth(const QVariantMap& value);

/// Validates composition moviePage/showsPage.
/// @brief Validates composition moviePage/showsPage (safe ids).
/// @details Checks that if present, the value is a QString and passes safeId validation.
/// @why Movie/show page IDs are used as lookup keys; unsafe ids would cause lookup failures.
/// @param value The composition map.
/// @return true if moviePage/showsPage are valid or absent, false otherwise.
bool
validCompositionMovieShowsPage(const QVariantMap& value);

/// Validates composition schemaVersion, columns and grid map.
/// @brief Validates composition schemaVersion, columns and grid map.
/// @details Checks that schemaVersion is numeric 1, columns (if present) is 1 or 2, and grid (if present) passes
/// validGrid validation.
/// @why These fields define the composition grid structure; invalid values would break layout rendering.
/// @param value The composition map.
/// @return true if version, columns, and grid are valid, false otherwise.
bool
validCompositionVersionColumnsGrid(const QVariantMap& value);

/// Validates composition sectionHeight (bounded).
bool
validCompositionSectionHeight(const QVariantMap& value);

/// Validates composition wallpaper.
/// @brief Validates composition wallpaper (artwork path pattern).
/// @details Checks that the value is a QString matching the artwork path pattern "^artwork/[A-Za-z0-9][A-Za-z0-9._-]*\\.(png|jpg|jpeg)$".
/// @why Wallpaper paths must follow the managed artwork naming convention to ensure files can be located.
/// @param value The composition map.
/// @return true if wallpaper path is valid, false otherwise.
bool
validCompositionWallpaper(const QVariantMap& value);

/// Validates composition headerMode.
/// @brief Validates composition headerMode ("navigation" or "identity").
/// @details Checks that the value is a QString and matches one of "navigation" or "identity".
/// @why Header mode determines header behavior; an unknown mode would cause header rendering errors.
/// @param value The composition map.
/// @return true if headerMode is valid, false otherwise.
bool
validCompositionHeaderModeIdentity(const QVariantMap& value);

/// Validates composition headerControls.
/// @brief Validates composition headerControls (unique, includes settings+edit).
/// @details Checks that headerControls is a QVariantList, non-empty, size at most MaxHeaderControls, contains only
/// known control IDs ("sources", "search", "settings", "edit"), no duplicates, and contains both "settings" and "edit".
/// @why Header controls are user-facing buttons; invalid controls would cause broken or missing header actions.
/// @param value The composition map.
/// @return true if headerControls is valid, false otherwise.
bool
validCompositionHeaderControls(const QVariantMap& value);

/// Validates composition identity.
/// @brief Validates composition identity (bounded name string).
/// @details Checks that the value is a QString, length at most MaxNameLength, and contains no null characters.
/// @why Identity strings are used as display names; invalid values would cause display errors.
/// @param value The composition map.
/// @return true if identity is valid, false otherwise.
bool
validCompositionIdentity(const QVariantMap& value);

/// Validates composition verticalScrolling.
/// @brief Validates composition verticalScrolling (grid mode screen).
/// @details Checks that if verticalScrolling is true, enabled is true, grid is present, and grid mode is "screen".
/// @why Vertical scrolling only makes sense with screen-mode grids; invalid combinations would cause scrolling issues.
/// @param value The composition map.
/// @return true if verticalScrolling is valid or absent, false otherwise.
bool
validCompositionVerticalScrolling(const QVariantMap& value);

/// Validates composition boolean keys.
/// @brief Validates composition boolean keys (enabled, fit, showProfile, showClock, verticalScrolling).
/// @details Checks that if present, each of enabled, fit, showProfile, showClock, verticalScrolling has metaType Bool.
/// @why Boolean keys control UI behavior; non-boolean values would be misinterpreted.
/// @param value The composition map.
/// @return true if all boolean keys are valid, false otherwise.
bool
validCompositionBoolKeys(const QVariantMap& value);

/// Validates composition margin/gap.
/// @brief Validates composition margin/gap (bounded 0–100).
/// @details Checks that if margin/gap are present, they are numeric (not QString or Bool), finite, and between 0 and 100.
/// @why Margin/gap values outside [0,100] would cause layout spacing issues.
/// @param value The composition map.
/// @return true if margin/gap are valid or absent, false otherwise.
bool
validCompositionMarginGap(const QVariantMap& value);

/// Validates composition navigation tabs.
/// @brief Validates composition navigation tabs (unique ids, bounded names).
/// @details Checks that navigation is a QVariantList with size at most MaxNavigationTabs, each tab has a unique safe id,
/// a non-empty trimmed name within MaxNameLength, valid keys (id, name, actions, sections), valid actions, and valid sections.
/// @why Navigation tabs are user-facing menu items; invalid tabs would cause broken navigation.
/// @param value The composition map.
/// @return true if navigation tabs are valid, false otherwise.
bool
validCompositionNavigationTabs(const QVariantMap& value);

/// Validates composition navigation tab list type and count.
/// @brief Validates composition navigation tab list type and count.
/// @details Checks that navigation is a QVariantList with size at most MaxNavigationTabs.
/// @why Navigation tabs are bounded to prevent excessive menu items.
/// @param value The composition map.
/// @return true if navigation tab list type and count are valid, false otherwise.
bool
validCompositionNavigationTabsType(const QVariantMap& value);

/// Validates a single navigation tab.
/// @brief Validates a single navigation tab (id, name, actions, sections).
/// @details Checks that id is safe and unique, name is non-empty trimmed within MaxNameLength, keys are from
/// {id, name, actions, sections}, actions pass validation, and sections pass validation.
/// @why Navigation tabs are user-facing menu items; invalid tabs would cause broken navigation.
/// @param tab The tab map to validate.
/// @param ids Set of already-seen tab ids for uniqueness checking.
/// @return true if the tab is valid, false otherwise.
bool
validNavigationTab(const QVariantMap& tab, QSet<QString>& ids);

/// Validates navigation tab keys only.
/// @brief Validates navigation tab keys only.
/// @details Checks that every key in the tab map is from the known set: id, name, actions, sections.
/// @why Unknown keys in tab maps would indicate malformed data.
/// @param tab The tab map to validate.
/// @return true if all keys are known, false otherwise.
bool
validNavigationTabKeys(const QVariantMap& tab);

/// Validates navigation tab actions.
/// @brief Validates navigation tab actions (non-empty validated list).
/// @details Checks that if actions is present, it is a non-empty QVariantList and passes actions validation.
/// @why Navigation tabs need actions to be meaningful; empty actions would create broken tabs.
/// @param tab The tab map to validate.
/// @return true if actions are valid or absent, false otherwise.
bool
validNavigationTabActions(const QVariantMap& tab);

/// Validates navigation tab sections.
/// @brief Validates navigation tab sections (safe ids).
/// @details Checks that if sections is present, it is a QVariantList and each element passes safeId validation.
/// @why Section ids in tabs are used as lookup keys; unsafe ids would cause lookup failures.
/// @param tab The tab map to validate.
/// @return true if sections are valid or absent, false otherwise.
bool
validNavigationTabSections(const QVariantMap& tab);

/// Validates composition initialTab.
/// @brief Validates composition initialTab (references an existing tab id).
/// @details Checks that if initialTab is present, its value references an existing tab id from the navigation tabs.
/// @why The initial tab must reference an existing tab; otherwise the initial view would be undefined.
/// @param value The composition map.
/// @return true if initialTab is valid or absent, false otherwise.
bool
validCompositionInitialTab(const QVariantMap& value);

/// Validates composition optional fields.
/// @brief Validates composition optional fields (settingsCategoryOrder, captureInputArtwork, appGallery, designLibrary).
/// @details Checks settingsCategoryOrder, captureInputArtwork, appGallery, and designLibrary if present.
/// @why Optional fields add composition features; invalid values would break those features.
/// @param value The composition map.
/// @return true if all optional fields are valid, false otherwise.
bool
validCompositionOptionalFields(const QVariantMap& value);

/// Validates composition layout fields.
/// @brief Validates composition layout fields (header, background, pageNavigation, sidebarWidth).
/// @details Checks header, background, pageNavigation, and sidebarWidth if present.
/// @why Layout fields define page presentation; invalid values would cause broken layouts.
/// @param value The composition map.
/// @return true if layout fields are valid, false otherwise.
bool
validCompositionLayout(const QVariantMap& value);

/// Validates composition grid fields.
/// @brief Validates composition grid fields (moviePage, showsPage, version, columns, grid, sectionHeight, wallpaper).
/// @details Checks moviePage/showsPage safe ids, version/columns/grid via versionColumnsGrid, sectionHeight bounds,
/// and wallpaper path pattern.
/// @why Grid fields define page grid structure; invalid values would cause broken grids.
/// @param value The composition map.
/// @return true if grid fields are valid, false otherwise.
bool
validCompositionGrid(const QVariantMap& value);

/// Validates composition identity fields.
/// @brief Validates composition identity fields (headerMode, identity, headerControls).
/// @details Checks headerMode, identity, and headerControls if present.
/// @why Identity fields define page identity and header controls; invalid values would cause broken headers.
/// @param value The composition map.
/// @return true if identity fields are valid, false otherwise.
bool
validCompositionIdentity(const QVariantMap& value);

/// Validates composition scrolling fields.
/// @brief Validates composition scrolling fields (verticalScrolling, bool keys, margin, gap).
/// @details Checks verticalScrolling, boolean keys, and margin/gap if present.
/// @why Scrolling fields affect page scrolling behavior; invalid values would cause broken scrolling.
/// @param value The composition map.
/// @return true if scrolling fields are valid, false otherwise.
bool
validCompositionScrolling(const QVariantMap& value);

/// Validates a placement grid.
/// @brief Validates a placement grid: columns 4-16, rows 3-10, gap/margin 0-100.
/// @details Checks that the map contains only known keys (columns, rows, gap, margin, snap, overlap, mode),
/// mode is "screen" or "sections" if present, columns/rows are whole numbers within bounds, gap/margin are within
/// bounds, and snap/overlap are booleans if present.
/// @why Grid structure determines page layout; invalid grids would cause broken layouts.
/// @param value The grid map.
/// @return true if the grid is valid, false otherwise.
bool
validGrid(const QVariantMap& value);

/// Validates composition grid keys only.
/// @brief Validates composition grid keys only.
/// @details Checks that every key in the grid map is from the known set: columns, rows, gap, margin, snap, overlap, mode.
/// @why Unknown keys in grid maps would indicate malformed data.
/// @param value The grid map.
/// @return true if all keys are known, false otherwise.
bool
validGridKeys(const QVariantMap& value);

/// Validates composition grid mode.
/// @brief Validates composition grid mode ("screen" or "sections").
/// @details Checks that the value is a QString and matches "screen" or "sections".
/// @why Grid mode determines how items are laid out; an unknown mode would cause rendering errors.
/// @param value The grid map.
/// @return true if mode is valid, false otherwise.
bool
validGridMode(const QVariantMap& value);

/// Validates composition grid columns or rows field.
/// @brief Validates composition grid columns or rows field.
/// @details Checks that the value is numeric (not QString or Bool), finite, a whole number, and within bounds
/// (columns: 4-16, rows: 3-10).
/// @why Columns/rows define grid dimensions; invalid values would cause overflowing grids.
/// @param value The grid map.
/// @param key The field name ("columns" or "rows").
/// @return true if columns/rows are valid, false otherwise.
bool
validGridColumnsRows(const QVariantMap& value, const QString& key);

/// Validates composition grid gap/margin field.
/// @brief Validates composition grid gap/margin field.
/// @details Checks that if gap/margin is present, it is numeric (not QString or Bool), finite, and between 0 and 100.
/// @why Gap/margin values outside [0,100] would cause layout spacing issues.
/// @param value The grid map.
/// @return true if gap/margin are valid or absent, false otherwise.
bool
validGridGapMargin(const QVariantMap& value);

/// Validates composition grid boolean keys.
/// @brief Validates composition grid boolean keys (snap, overlap).
/// @details Checks that if present, snap and overlap have metaType Bool.
/// @why Boolean keys control grid behavior; non-boolean values would be misinterpreted.
/// @param value The grid map.
/// @return true if boolean keys are valid or absent, false otherwise.
bool
validGridBoolKeys(const QVariantMap& value);

/// Returns the input grid filled with screen-mode defaults.
/// @brief Returns the input grid filled with screen-mode defaults (8 columns x 5 rows, gap 12, margin 20)
/// for any missing keys.
/// @details Fills in missing columns, rows, gap, margin with default values for screen-mode grids.
/// @param value The grid map to normalize.
/// @return The normalized grid map with defaults filled in.
QVariantMap
normalizedGrid(const QVariantMap& value);

/// Validates a single placement.
/// @brief Validates a single placement: keys x/y/width/height, integers, non-empty positive size, contained within maxColumns x maxRows.
/// @details Checks that the map has exactly keys x/y/width/height, each is a whole number within bounds
/// (x/width: 0-maxColumns, y/height: 0-maxRows), width and height are positive, and x+width <= maxColumns,
/// y+height <= maxRows.
/// @why Placements define item positions on grids; invalid placements would cause overlapping or off-screen items.
/// @param value The placement map.
/// @param maxColumns Maximum columns in the grid.
/// @param maxRows Maximum rows in the grid.
/// @return true if the placement is valid, false otherwise.
bool
validPlacement(const QVariantMap& value, int maxColumns, int maxRows);

/// Validates placement keys only.
/// @brief Validates placement keys only (x, y, width, height).
/// @details Checks that the map has exactly keys x/y/width/height.
/// @why Placement maps must have exactly these four keys to define a position and size.
/// @param value The placement map.
/// @return true if keys are exactly x/y/width/height, false otherwise.
bool
validPlacementKeys(const QVariantMap& value);

/// Validates a single placement dimension field.
/// @brief Validates a single placement dimension field.
/// @details Checks that the value for the given key is numeric (not QString or Bool), finite, a whole number,
/// within bounds (x/width: 0-maxColumns, y/height: 0-maxRows).
/// @why Dimensions must be whole numbers within grid bounds to avoid overflow.
/// @param value The placement map.
/// @param key The dimension field name ("x", "y", "width", or "height").
/// @param maxColumns Maximum columns in the grid.
/// @param maxRows Maximum rows in the grid.
/// @return true if the dimension is valid, false otherwise.
bool
validPlacementDimension(const QVariantMap& value, const QString& key, int maxColumns, int maxRows);

/// True when two placements overlap as axis-aligned rectangles.
/// @brief True when two placements overlap as axis-aligned rectangles.
/// @details Checks if two placement maps overlap by comparing their x/y/width/height rectangles.
/// @why Overlapping placements would cause items to cover each other on screen.
/// @param a The first placement map.
/// @param b The second placement map.
/// @return true if the placements overlap, false otherwise.
bool
placementsOverlap(const QVariantMap& a, const QVariantMap& b);

/// True when every placed item fits the grid and none overlap.
/// @brief True when every placed item fits the grid and none overlap (unless overlap is enabled on the grid).
/// @details Checks that all items in the list fit within the grid bounds and none overlap (unless the grid has
/// overlap enabled).
/// @why Items that don't fit or overlap would cause broken layouts.
/// @param items The list of placement items.
/// @param grid The grid map defining bounds.
/// @return true if all items fit and don't overlap, false otherwise.
bool
placementsFit(const QJsonArray& items, const QVariantMap& grid);

/// Computes the effective grid for a category.
/// @brief Computes the effective grid for a category: category presentation override applied on top of the composition grid.
/// @details Merges category-specific presentation overrides with the base composition grid to produce an effective grid.
/// @param composition The composition map.
/// @param categories The categories array.
/// @param categoryId The category id to compute the grid for.
/// @return The effective grid map for the category.
QVariantMap
sectionGrid(const QVariantMap& composition, const QJsonArray& categories, const QString& categoryId);

/// Validates every placement in a document against its composition grid and category sections.
/// @brief Validates every placement in a document against its composition grid and category sections, including nested containers.
/// @details Checks that all items in the list fit within their respective grids and none overlap.
/// @why Items that don't fit or overlap would cause broken layouts.
/// @param items The list of placement items.
/// @param composition The composition map.
/// @param categories The categories array.
/// @return true if all placements fit, false otherwise.
bool
layoutPlacementsFit(const QJsonArray& items, const QVariantMap& composition, const QJsonArray& categories);

/// Validates the full component hierarchy.
/// @brief Validates the full component hierarchy: unique ids, valid parents, bounded nesting depth and placements that fit each container grid. Sets error on failure and returns false.
/// @details Validates component identities (unique safe ids, builds parent/child maps), parent references
/// (existing container parents, no cycles, bounded nesting depth), container placements, layout, child fit,
/// root-level placement and section fit.
/// @param items The list of component items.
/// @param composition The composition map.
/// @param categories The categories array.
/// @param error Output error message on failure.
/// @return true if the hierarchy fits, false otherwise.
bool
hierarchyFits(const QJsonArray& items,
              const QVariantMap& composition,
              const QJsonArray& categories,
              QString* error = nullptr);

/// Validates component identities.
/// @brief Validates component identities: unique safe ids, builds parent/child maps.
/// @details Iterates all items to build id maps, checks for unique safe ids, builds parent/child maps,
/// marks containers, checks for self-referencing parents and bounded nesting depth.
/// @why Unique ids and valid parent references are essential for correct hierarchy rendering.
/// @param byId Output map of id to component object.
/// @param children Output map of parent id to child array.
/// @param roots Output array of root items.
/// @param containers Output flag indicating if containers exist.
/// @param items The list of component items.
/// @param error Output error message on failure.
/// @return true if identities are valid, false otherwise.
bool
hierarchyFitsValidateIds(QHash<QString, QJsonObject>& byId, QHash<QString, QJsonArray>& children,
                         QJsonArray& roots, bool& containers, const QJsonArray& items, QString* error);

/// Validates component parent references: existing container parents, no
/// cycles, bounded nesting depth.
bool
hierarchyFitsValidateParents(const QHash<QString, QJsonObject>& byId,
                             const QHash<QString, QJsonArray>& children,
                             QString* error);

/// Validates component containers: placement, layout, child fit.
bool
hierarchyFitsValidateContainers(const QHash<QString, QJsonObject>& byId,
                                const QHash<QString, QJsonArray>& children,
                                const QVariantMap& composition, const QJsonArray& categories,
                                QString* error);

/// Validates a single container: family-specific child rules, placement,
/// layout, grid fit.
bool
hierarchyFitsValidateContainer(const QJsonObject& item,
                               const QHash<QString, QJsonArray>& children,
                               const QVariantMap& composition,
                               const QJsonArray& categories,
                               const QString& id,
                               const std::function<bool(const QString&)>& fail);

/// Validates root-level components: placement, section fit, library exclusivity.
bool
hierarchyFitsValidateRoots(const QHash<QString, QJsonObject>& byId, const QJsonArray& items,
                           const QVariantMap& composition, const QJsonArray& categories,
                           QString* error);

// Computes the grid for a nested container's children, or the section grid
// when parentId is empty.
QVariantMap
componentGrid(const QJsonArray& items,
              const QVariantMap& composition,
              const QJsonArray& categories,
              const QString& parentId,
              const QString& categoryId);

/// Normalizes and validates a component map; legacy mode accepts v1
/// documents. Returns an empty map and sets error on rejection.
QVariantMap
component(const QVariantMap& input, bool legacy, QString* error = nullptr);

/// Resolves the component type from legacy widget mapping.
QVariantMap
componentResolveLegacyType(const QVariantMap& input, bool legacy);

/// Validates component type and shape fields.
bool
componentValidateTypeAndShape(const QVariantMap& input, const QString& type, QString* error);

/// Validates component name field.
bool
componentValidateName(const QVariantMap& input, QString* error);

/// Validates component family field.
bool
componentValidateFamily(const QVariantMap& input, const QString& type, QString* error);

/// Validates component appCollection field.
bool
componentValidateAppCollection(const QVariantMap& input, const QString& type, QString* error);

/// Validates app collection shape (key count, mode, ids).
bool
componentValidateAppCollectionShape(const QVariantMap& collection, QString* error);

/// Validates app collection showStore field (boolean).
bool
componentValidateAppCollectionShowStore(const QVariantMap& collection, QString* error);

/// Validates app collection section field.
bool
componentValidateAppCollectionSection(const QVariantMap& collection, QString* error);

/// Validates app collection ids (unique safe ids).
bool
componentValidateAppCollectionIds(const QVariantMap& collection, QString* error);

/// Validates app collection mode ("all" requires empty ids).
bool
componentValidateAppCollectionMode(const QVariantMap& collection, QString* error);

/// Validates component accessibility map (name/hint bounded strings).
bool
componentValidateAccessibility(const QVariantMap& input, QString* error);

/// Validates component parentId field (safe id or empty).
bool
componentValidateParentId(const QVariantMap& input, QString* error);

/// Validates component children field (must be absent).
bool
componentValidateChildren(const QVariantMap& input, QString* error);

/// Validates component requiredCapability field (container +
/// capture-inputs only).
bool
componentValidateRequiredCapability(const QVariantMap& input, const QString& type,
                                    QString* error);

/// Validates component size field ("compact", "standard", "wide").
bool
componentValidateSize(const QVariantMap& input, QString* error);

/// Validates component opacity field (numeric, bounded).
bool
componentValidateOpacity(const QVariantMap& input, QString* error);

/// Validates component boolean fields (hidden, locked, showTitle,
/// showType, showLogo, logoFill).
bool
componentValidateBooleans(const QVariantMap& input, QString* error);

/// Validates component accent field (opaque named color).
bool
componentValidateAccent(const QVariantMap& input, QString* error);

/// Validates component symbol field (bounded string).
bool
componentValidateSymbol(const QVariantMap& input, QString* error);

/// Validates component linkedDesign/designNode fields (safe ids).
bool
componentValidateLinkedDesign(const QVariantMap& input, QString* error);

/// Validates component placement field.
bool
componentValidatePlacement(const QVariantMap& input, QString* error);

/// Validates component presentation field.
bool
componentValidatePresentation(const QVariantMap& input, const QString& type,
                              QString* error);

/// Validates component animateArtwork field (image only).
bool
componentValidateAnimateArtwork(const QVariantMap& input, const QString& type,
                                QString* error);

/// Validates component customLogo field (artwork path pattern).
bool
componentValidateCustomLogo(const QVariantMap& input, QString* error);

/// Groups basic component validations: type/shape + name.
bool
componentValidateBasic(const QVariantMap& input, const QString& type, QString* error);

/// Groups structural component validations: family, appCollection,
/// accessibility, parentId, children.
bool
componentValidateStructure(const QVariantMap& input, const QString& type, QString* error);

/// Groups capability component validations: requiredCapability, size,
/// opacity.
bool
componentValidateCapabilities(const QVariantMap& input, const QString& type, QString* error);

/// Groups all component validations into a single gate.
bool
componentValidateAll(const QVariantMap& input, const QString& type, QString* error);

/// Builds a library source map for button/image/library component types.
QVariantMap
componentBuildLibrarySource(const QVariantMap& input, const QVariantMap& style, QString* error);

/// Builds a library source map specifically for library component type.
QVariantMap
componentBuildLibrarySourceLibrary(const QVariantMap& input, const QVariantMap& style,
                                   QString* error);

/// Validates library guide field (channels/all/empty).
bool
componentValidateLibraryGuide(bool guide, const QVariantMap& style, QString* error);

/// Validates library music field (albums/all/empty).
bool
componentValidateLibraryMusic(bool music, const QVariantMap& style, QString* error);

/// Validates library history field (history/watched/empty).
bool
componentValidateLibraryHistory(bool history, const QVariantMap& style, QString* error);

/// Validates library views fields (libraryKind/libraryView/libraryValue).
bool
componentValidateLibraryViews(const QVariantMap& style, QString* error);

/// Validates library games fields (supported views, platform name bound).
bool
componentValidateLibraryGames(const QVariantMap& style, QString* error);

/// Builds a shell source map for clock/text/shape/container component types.
QVariantMap
componentBuildShellSource(const QVariantMap& input, const QString& type, QString* error);

/// Builds a launcher source map from normalized actions.
QVariantMap
componentBuildLauncherSource(const QVariantMap& input, const QVariantList& normalizedList,
                             const QString& actionType, QString* error);

/// Human-readable unavailability reason for an action, or empty when
/// available. Missing apps, collections and incompatible open-with handlers
/// are reported; shell routes never require an Open With app.
QString
unavailable(const QVariantMap& action, const AppRegistry* apps, const QVariantList& collections);

/// Returns empty for shell routes (open-page, open-apps, open-search,
/// app-drawer, root-page).
QString
unavailableShellRoutes(const QString& type);

/// Validates launch-app target against app registry.
QString
unavailableLaunchApp(const QVariantMap& value, const AppRegistry* apps);

/// Validates open-collection/play-movie/open-show target against collections.
QString
unavailableCollection(const QVariantMap& value, const QVariantList& collections);

/// Validates open-* target against app registry openWith handler.
QString
unavailableOpenWith(const QVariantMap& value, const AppRegistry* apps);

} // namespace ComponentSchema
