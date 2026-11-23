#pragma once
#include <QVariantList>
#include <QVariantMap>
class AppRegistry;

/** Data contracts only. No process launching, networking or Kodi mutations.
 * @brief Data contracts only.
 * @details No process launching, networking or Kodi mutations. Every validator returns true only for documents that satisfy the layout bounds declared in ComponentSchema.cpp; values are never clamped silently.
 * @why Keeps the schema module pure: it validates data contracts without side effects, process launching, networking, or Kodi mutations.
 */
namespace ComponentSchema {

bool validRecentStudioColors(const QVariant& value);

inline constexpr int MaxComponentNestingDepth = 4;
inline constexpr double MinOpacity = 0.0;
inline constexpr double MaxOpacity = 1.0;
inline constexpr double MinHdmiInput = 1.0;
inline constexpr double MaxHdmiInput = 8.0;
inline constexpr int MaxRecentStudioColors = 8;
inline constexpr int MaxPageDataClientKeySize = 330;
inline constexpr int MaxPageDataClientIdLength = 160;
inline constexpr int MaxPageDataClients = 128;
inline constexpr int MaxPageDataArtwork = 512;

bool validPresentationNumericHeight(double number, bool section);
bool validPresentationNumericProgress(double number, bool section);
bool validPresentationNumericOpacity(double number);
bool validPresentationNumericSize(double number);
bool validPresentationNumericContentWidth(double number, bool section);
bool validPresentationImageCrop(const QVariantMap& presentation);
bool validPresentationNumericField(const QVariantMap& presentation, const QString& key, bool section);
bool validPresentationNumericColumns(double number, bool section);
bool validPresentationNumericSpan(double number, bool section);
bool validCompositionHeaderKeys(const QVariantMap& header);
inline constexpr double MinSectionHeight = 60.0;
inline constexpr double MaxSectionHeight = 800.0;

bool validCompositionHeaderGeometry(const QVariantMap& header, const QVariantMap& value);
bool validCompositionHeaderColor(const QVariant& entry);
bool validCompositionHeaderCustomLogo(const QVariant& entry);
bool validCompositionHeaderControlDisplay(const QVariant& entry);
bool validCompositionHeaderTitleAlignment(const QVariant& entry);
bool validCompositionHeaderTimeFormat(const QVariant& entry);
bool validCompositionHeaderDateFormat(const QVariant& entry);
bool validCompositionHeaderOrder(const QVariant& entry);
bool validCompositionHeaderNumber(const QVariant& entry, const QPair<double, double>& bounds);
bool validCompositionHeaderPosition(const QVariant& entry);
bool validCompositionHeaderBool(const QVariant& entry);
bool validCompositionAppGalleryBool(const QVariant& val);
bool validCompositionAppGalleryTitle(const QVariant& val);
bool validCompositionAppGalleryFavoriteIds(const QVariant& val);
bool validCompositionHeaderField(const QString& key, const QVariant& entry);
bool validCompositionAppGalleryLayout(const QVariant& val);
bool validCompositionAppGalleryColumns(const QVariant& val);
bool validCompositionAppGalleryDensity(const QVariant& val);
bool validCompositionAppGallerySort(const QVariant& val);
bool validCompositionAppGalleryField(const QString& key, const QVariant& val);

QVariantMap normalizeConfiguration(const QVariant& value, QString* error);

bool validSurfaceThemes(const QVariant& value);
bool readableSurfaceThemes(const QVariantMap& home, const QVariantMap& origin);
QVariantMap surfacePalette(const QVariantMap& appearance);

QVariantMap editorActionCaptureSwitchInput(const QVariantMap& input, QString* error);
QVariantMap editorActionCapturePreviewDraft(const QVariantMap& input, QString* error);
QVariantMap editorActionCaptureControl(const QVariantMap& input, const QString& type, QString* error);
QVariantMap editorActionAudioOutputSelect(const QVariantMap& input, QString* error);
QVariantMap editorActionCaptureArtworkApply(const QVariantMap& input, QString* error);
QVariantMap editorActionCaptureApply(const QVariantMap& input, QString* error);
QVariantMap actionSettings(const QVariantMap& input, QString* error);
QVariantMap actionSwitchInput(const QVariantMap& input, QString* error);
QVariantMap actionDesktopMode(const QVariantMap& input, QString* error);
QVariantMap actionWait(const QVariantMap& input, QString* error);
QVariantMap actionOpenTarget(const QVariantMap& input, const QString& type, QString* error);
QVariantList actions(const QVariantList& input, QString* error = nullptr);
QVariantMap action(const QVariantMap& input, QString* error = nullptr);
bool validPresentationGuideTiming(const QVariantMap& presentation);
bool validPresentationGuideBool(const QVariantMap& presentation);
bool validPresentationLibraryChrome(const QVariantMap& value, bool section);
bool validPresentationHeadingFlags(const QVariantMap& value, bool section, bool container);
bool validPresentationTrailingAction(const QVariantMap& value, bool section);
bool validPresentationGuideFilter(const QVariant& value);
bool validPresentationGuideGroup(const QVariant& value);
bool validPresentationGuideChannels(const QVariant& value);
bool validPresentationGuideSurface(const QVariant& value);
bool validPresentationStyleId(const QVariantMap& presentation);
bool validPresentationResponsive(const QVariantMap& value, bool section);
bool validPresentationGuideOverlay(const QVariantMap& presentation);
bool validPresentationGuideField(const QString& key, const QVariant& value);
bool validPresentationGuideArtSource(const QVariant& value);
bool validPresentationGuideVariant(const QVariant& value);
bool validPresentationLibraryLayout(const QVariant& value);
bool validPresentationLibraryCardStyle(const QVariant& value);
bool validPresentationLibraryValue(const QVariant& value);
bool validPresentationLibraryColumns(const QVariant& value);
bool validPresentationShortString(const QVariantMap& presentation);
bool validPresentationSurfaceColor(const QVariantMap& presentation);
bool validPresentationGuideFields(const QVariantMap& presentation);
bool validPresentationLibraryField(const QString& key, const QVariant& value);
bool validPresentationLibraryKind(const QVariant& value);
bool validPresentationLibraryView(const QVariant& value);
bool validPresentationArtFocal(const QVariantMap& presentation);
bool validPresentationArtFocalType(const QVariant& value);
bool validPresentationArtFocalBounds(const QString& key, double minimum, double& maximum, double& maxValue);
bool validPresentationArtFocalInteger(const QString& key, double number);
bool validPresentationBorderColor(const QVariantMap& presentation);
bool validPresentationLibraryFields(const QVariantMap& presentation, bool section);
bool validPresentationBodyColor(const QVariantMap& presentation);
bool validPresentationBodyMetrics(const QVariantMap& presentation);
bool validPresentationBoolToggle(const QVariantMap& value, bool section, bool container);
bool validPresentationSourceVariant(const QVariantMap& presentation);
bool validPresentationBoolFlag(const QVariantMap& presentation, const QString& key);
bool validPresentationStringField(const QVariantMap& presentation, const QString& key);
bool validPresentationIconStyle(const QVariantMap& presentation);
bool validPresentationIconGlow(const QVariantMap& presentation);
bool validPresentationArtFit(const QVariantMap& presentation);
bool validPresentationBodyAlign(const QVariantMap& presentation);
bool validPresentationVariant(const QVariantMap& presentation);
bool validPresentationFill(const QVariantMap& presentation);
bool validPresentationShape(const QVariantMap& presentation);
bool validPresentationSourceMaximum(const QVariantMap& presentation);
bool validPresentationHistoryApps(const QVariantMap& presentation, bool section);
bool validPresentationHistorySourceBadges(const QVariantMap& presentation, bool section);
bool validPresentationStates(const QVariantMap& states);
bool validPresentationLayout(const QVariantMap& layout, bool container);
bool validPresentationGrid(const QVariantMap& grid, bool section);
QVariantMap editorAction(const QVariantMap& input, QString* error = nullptr);
QVariantMap actionLaunchGame(const QVariantMap& input, QString* error);
QVariantMap actionGuide(const QVariantMap& input, const QString& type, QString* error);
QVariantMap actionLaunchApp(const QVariantMap& input, QString* error);
QVariantMap actionCollection(const QVariantMap& input, const QString& type, QString* error);
QVariantMap actionOpenPage(const QVariantMap& input, QString* error);
QVariantMap actionShellRoute(const QVariantMap& input, const QString& type, QString* error);

QString effectiveCategory(const QJsonArray& items, const QString& id);

bool positionLocked(const QJsonArray& items, const QString& id);
bool positionLockedWalk(const QJsonArray& items, QString current, QSet<QString> visited, int depth);
bool positionLockedShouldStop(const QString& current, int depth);
bool positionLockedStopResult(const QString& current, int depth);
bool positionLockedContinue(const QJsonArray& items, const QJsonObject& item,
                            QSet<QString> visited, int depth);
bool positionLockedCycleCheck(const QSet<QString>& visited, const QString& current);
bool positionLockedFindItem(const QJsonArray& items, const QString& current, QJsonObject& item);
bool positionLockedIsLocked(const QJsonObject& item);

/** Validates a safe component identity: starts with alphanumeric, bounded length, rejects "." and "..".
 * @brief Validates a component identity string.
 * @details Checks that the id starts with an alphanumeric character, is within MaxComponentIdLength, and is not "." or "..".
 * @why Component ids are used as keys in maps and file paths; unsafe ids could cause collisions or path traversal.
 * @param id The identity string to validate.
 * @return true if the id is valid, false otherwise.
 */
bool safeId(const QString& id);

/** Validates the Settings navigation document.
 * @brief Validates a Settings navigation document: exact category set and route table, bounded labels and descriptions.
 * @details Ensures the navigation map has exactly schemaVersion=1 and categories list, that every category has a unique id from the allowed set, a non-empty name/description within length limits, and either a route or rows. Every route across all categories must be unique and drawn from the known route set.
 * @why The Settings navigation is shared state consumed by the Settings page; malformed entries would cause missing navigation items or ambiguous routing, so it must be validated strictly before being stored.
 * @param value The navigation document to validate.
 * @return true if the document is valid, false otherwise.
 */
bool validSettingsNavigation(const QVariantMap& value);

/** Validates a source policy document.
 * @brief Validates a source policy document: bounded duration, known mode, unique page/slot ids, known event types, boolean flags and optional detected source/appliance sub-maps.
 * @details Iterates every field in the policy map and dispatches to the appropriate field validator. Ensures schemaVersion and duration are valid numbers, mode is one of off/compact/widget, pages/slots are unique safe ids, events/types are known media event kinds, boolean flags are actually booleans, and optional detected source / appliance sub-maps pass their own validation.
 * @why Source policies define how Isolde interprets incoming media events; invalid policies could cause incorrect source detection or unbounded data, so they must be validated before storage.
 * @param value The source policy document to validate.
 * @return true if the document is valid, false otherwise.
 */
bool validSourcePolicy(const QVariantMap& value);

/** Validates a numeric source policy field.
 * @brief Validates a numeric source policy field (schemaVersion or duration).
 * @details Checks that the value is a numeric type (Int, UInt, LongLong, ULongLong, or Double), is finite, and is a whole number. For schemaVersion, requires exactly 1. For duration, requires a value between 5 and 60 inclusive.
 * @why Numeric fields in source policies must be exact to avoid misinterpretation of timing or schema compatibility.
 * @param value The variant value to validate.
 * @param key The field name being validated ("schemaVersion" or "duration").
 * @return true if the value is a valid number within bounds for the given key.
 */
bool validSourcePolicyNumeric(const QVariant& value, const QString& key);

/** Validates the source policy mode field.
 * @brief Validates the source policy mode field ("off", "compact" or "widget").
 * @details Checks that the value is a QString and that it matches one of the three known modes: "off", "compact", or "widget".
 * @why The mode determines how sources are displayed in the home UI; an unknown mode would cause rendering errors.
 * @param value The mode value to validate.
 * @return true if the mode is one of the allowed values, false otherwise.
 */
bool validSourcePolicyMode(const QVariant& value);

/** Validates a source policy page or slot id list.
 * @brief Validates a source policy page or slot id list: unique safe ids.
 * @details Checks that the value is a QVariantList, its size is within bounds (64 for pages/slots), each element is a QString, all ids are unique, and each id passes safeId validation (except empty ids allowed for "pages").
 * @why Page and slot ids are used as lookup keys; duplicates or unsafe ids would cause lookup failures or inconsistent state.
 * @param value The variant list to validate.
 * @param key The field name being validated ("pages" or "slots").
 * @return true if the list is valid, false otherwise.
 */
bool validSourcePolicyIdList(const QVariant& value, const QString& key);

/** Validates a source policy event type list.
 * @brief Validates a source policy event type list: known media event kinds.
 * @details Checks that the value is a QVariantList with size at most 4, each element is a QString, all values are unique, and each matches one of the known media event kinds ("movie-disc", "game-disc", "media-folder", "file-source").
 * @why Event types determine which media events trigger source actions; unknown types would cause silent failures.
 * @param value The variant list to validate.
 * @param key The field name being validated ("events" or "types").
 * @return true if the list is valid, false otherwise.
 */
bool validSourcePolicyEventTypeList(const QVariant& value, const QString& key);

/** Validates a source policy boolean flag field.
 * @brief Validates a source policy boolean flag field.
 * @details Checks that the variant holds a QMetaType::Bool value.
 * @why Boolean flags in source policies control behavior; a non-boolean value would be misinterpreted.
 * @param value The variant value to validate.
 * @return true if the value is a boolean, false otherwise.
 */
bool validSourcePolicyBool(const QVariant& value);

/** Validates a detected source sub-map (id, type, containment).
 * @brief Validates a detected source sub-map: id, type, containment.
 * @details Checks that the value is a QVariantMap, contains id (safeId), type (one of movie-disc/game-disc/media-folder/file-source), and optional containment fields.
 * @why Detected sources represent physical media inputs; invalid identities could cause incorrect source mapping.
 * @param value The variant map to validate.
 * @return true if the detected source sub-map is valid, false otherwise.
 */
bool validSourcePolicyDetectedSource(const QVariantMap& value);

/** Validates an appliance sub-map (id, type, distribution mode).
 * @brief Validates an appliance sub-map: id, type, distribution mode.
 * @details Checks that the value is a QVariantMap, contains id (safeId), type (one of whiteware/AV appliance/climate appliance), and optional distributionMode and requiresSetup fields.
 * @why Appliance definitions map physical devices to Isolde; invalid maps would cause incorrect device handling.
 * @param value The variant map to validate.
 * @return true if the appliance sub-map is valid, false otherwise.
 */
bool validSourcePolicyAppliance(const QVariantMap& value);

/** Dispatches a source policy field to its field-specific validator.
 * @brief Dispatches a source policy field to its field-specific validator.
 * @details Routes each key in the source policy map to the appropriate field-specific validator based on the key name.
 * @why Centralizes field dispatch logic so new fields can be added in one place.
 * @param key The field name to dispatch.
 * @param v The variant value to validate.
 * @return true if the field is valid, false otherwise.
 */
bool validSourcePolicyField(const QString& key, const QVariant& v);

/** Validates a detected source field.
 * @brief Validates a detected source field (map wrapper).
 * @details Checks that the variant is a QVariantMap and delegates to validSourcePolicyDetectedSource for full validation of id, type, and containment fields.
 * @why Detected sources represent physical media inputs; invalid identities could cause incorrect source mapping.
 * @param v The variant value to validate.
 * @return true if the value is a valid detected source map, false otherwise.
 */
bool validSourcePolicyDetectedSourceField(const QVariant& v);

/** Validates an appliance field.
 * @brief Validates an appliance field (map wrapper).
 * @details Checks that the variant holds a QVariantMap and delegates to validSourcePolicyAppliance for full validation of id, type, and distribution mode fields.
 * @why Appliance definitions map physical devices to Isolde; invalid maps would cause incorrect device handling.
 * @param v The variant value to validate.
 * @return true if the value is a valid appliance map, false otherwise.
 */
bool validSourcePolicyApplianceField(const QVariant& v);

/** Validates child layout keys only.
 * @brief Validates child layout keys only (known key set).
 * @details Checks that the map contains only keys from the known set: schemaVersion, mode, columns, rows, gap, padding, visibleColumns, wrap. Any unknown key causes validation failure.
 * @why Unknown keys in child layout maps could indicate malformed data or future fields not yet supported.
 * @param value The map to validate.
 * @return true if all keys are known, false otherwise.
 */
bool validChildLayoutKeys(const QVariantMap& value);

/** Validates child layout schemaVersion.
 * @brief Validates child layout schemaVersion (must be numeric 1).
 * @details Checks that the map contains a schemaVersion field that is numeric (not QString or Bool) and equals 1.
 * @why The schemaVersion identifies the layout format version; only version 1 is currently supported.
 * @param value The child layout map.
 * @return true if schemaVersion is present and equals 1, false otherwise.
 */
bool validChildLayoutVersion(const QVariantMap& value);

/** Validates child layout mode.
 * @brief Validates child layout mode ("grid", "row" or "column").
 * @details Checks that the value is a QString and matches one of the three known modes: "grid", "row", "column".
 * @why The mode determines how children are laid out within a child layout; an unknown mode would cause rendering errors.
 * @param value The child layout map.
 * @return true if the mode is one of the allowed values, false otherwise.
 */
bool validChildLayoutMode(const QVariantMap& value);

/** Validates a child layout dimension field.
 * @brief Validates a child layout dimension field (columns, rows, gap or padding).
 * @details Checks that the map contains the key, the value is numeric (not QString or Bool), finite, within bounds (columns: 4-16 for grid or 4-128 for row, rows: 4-10, gap/padding: 0-100), and for columns/rows that it is a whole number.
 * @why Dimensions must be whole numbers within platform limits to avoid layout overflow or rendering issues.
 * @param value The child layout map.
 * @param key The dimension field name ("columns", "rows", "gap", or "padding").
 * @return true if the dimension is valid, false otherwise.
 */
bool validChildLayoutDimension(const QVariantMap& value, const QString& key);

/** Validates child layout dimension type.
 * @brief Validates child layout dimension type (not string or bool).
 * @details Checks that the value's meta type is not QString and not Bool.
 * @why Dimensions must be numeric types; strings or booleans would be misinterpreted as layout measurements.
 * @param value The variant map containing the dimension.
 * @param key The dimension field name.
 * @return true if the type is numeric, false otherwise.
 */
bool validChildLayoutDimensionType(const QVariantMap& value, const QString& key);

/** Validates child layout dimension range.
 * @brief Validates child layout dimension range (bounded to grid limits).
 * @details Parses the value as a double and checks it is finite, within the key-specific bounds (columns: 4-16 grid/4-128 row, rows: 4-10, gap/padding: 0-100), and whole-number for columns/rows.
 * @why Out-of-range dimensions would cause layouts that overflow the screen or violate grid constraints.
 * @param value The child layout map.
 * @param key The dimension field name.
 * @return true if the dimension is within bounds, false otherwise.
 */
bool validChildLayoutDimensionRange(const QVariantMap& value, const QString& key);

/** Validates child layout wrap field.
 * @brief Validates child layout wrap field (boolean or absent).
 * @details If present, checks that the value is a QMetaType::Bool.
 * @why Wrap controls text wrapping behavior; non-boolean values would be ambiguous.
 * @param value The child layout map.
 * @return true if wrap is absent or boolean, false otherwise.
 */
bool validChildLayoutWrap(const QVariantMap& value);

/** Validates child layout visibleColumns field.
 * @brief Validates child layout visibleColumns field (row mode only).
 * @details Checks that if visibleColumns is present, it is numeric (not QString or Bool), finite, a whole number, between MinChildColumns and the columns value, and only applies when mode is "row".
 * @why visibleColumns limits how many items show per row; invalid values would cause broken row layouts.
 * @param value The child layout map.
 * @return true if visibleColumns is valid or absent, false otherwise.
 */
bool validChildLayoutVisibleColumns(const QVariantMap& value);

/** Validates child layout mode consistency.
 * @brief Validates child layout mode/columns/rows consistency.
 * @details Checks that row mode has rows=1 and column mode has columns=1.
 * @why Row mode only makes sense with a single row, and column mode only with a single column.
 * @param value The child layout map.
 * @return true if mode is consistent with columns/rows, false otherwise.
 */
bool validChildLayoutModeConsistency(const QVariantMap& value);

/** Shell policies are layout data. Missing policies inherit the opening page; Settings always uses designated Home, including its explicit custom policy.
 * @brief Validates settings navigation state.
 * @details Ensures the settings navigation document has valid schemaVersion, categories list, and valid category entries.
 * @why Settings navigation is shared state consumed by the Settings page; malformed entries would cause missing navigation items or ambiguous routing.
 * @param value The settings navigation map.
 * @return true if the state is valid, false otherwise.
 */
bool validSettingsNavigation(const QVariantMap& value);

/** Validates settings navigation schemaVersion (must be numeric 1).
 * @brief Validates settings navigation schemaVersion (must be numeric 1).
 * @details Checks that the map contains a schemaVersion field that is numeric (not QString or Bool) and equals 1.
 * @why The schemaVersion identifies the settings navigation format version; only version 1 is currently supported.
 * @param value The settings navigation map.
 * @return true if schemaVersion is present and equals 1, false otherwise.
 */
bool validSettingsNavigationVersion(const QVariantMap& value);

/** Validates settings navigation categories list (exact set of 10 categories).
 * @brief Validates settings navigation categories list (exact set of 10 categories).
 * @details Checks that the categories array contains exactly 10 entries, each with id, name, description, and route fields.
 * @why Settings navigation categories define the available settings categories; an incorrect count would break navigation.
 * @param value The settings navigation map.
 * @return true if categories are valid, false otherwise.
 */
bool validSettingsNavigationCategories(const QVariantMap& value);

/** Validates a single settings navigation category (id, name, description, rows or route).
 * @brief Validates a single settings navigation category (id, name, description, rows or route).
 * @details Checks that each category has a unique safe id, a non-empty name/description within length limits, and either a route or rows. Every route across all categories must be unique and drawn from the known route set.
 * @param category The category map to validate.
 * @param seen Set of already-seen category ids for uniqueness checking.
 * @param seenRoutes Set of already-seen route ids for uniqueness checking.
 * @return true if the category is valid, false otherwise.
 */
bool validSettingsNavigationCategory(const QVariantMap& category, QSet<QString>& seen,
                                     QSet<QString>& seenRoutes);

/** Validates a settings navigation row (name, description, unique route).
 * @brief Validates a settings navigation row (name, description, unique route).
 * @details Checks that each row has a non-empty trimmed name/description within length limits and a valid route from the known route set.
 * @param row The row map to validate.
 * @param seenRoutes Set of already-seen route ids for uniqueness checking.
 * @return true if the row is valid, false otherwise.
 */
bool validSettingsNavigationRow(const QVariantMap& row, QSet<QString>& seenRoutes);

/** Validates a settings navigation label (non-empty, bounded size).
 * @brief Validates a settings navigation label (non-empty, bounded size).
 * @details Checks that the value is a non-empty QString with length within limits and no null characters.
 * @param label The label value to validate.
 * @param limit The maximum allowed length.
 * @return true if the label is valid, false otherwise.
 */
bool validSettingsNavigationLabel(const QVariant& label, int limit);

/** Versioned native-channel definitions and bounded nested rule trees.
 * @brief Validates a guide channel document: versioned native-channel definitions and bounded nested rule trees.
 * @details Checks that the channel definition map contains valid id, number, groups, rule, mediaKinds, and optional fields.
 * @why Channel definitions define how media is mapped to channels; invalid definitions would cause incorrect channel behavior.
 * @param value The channel definition map.
 * @param error Output error message on failure.
 * @return A valid channel map if validation succeeds, empty map otherwise.
 */
QVariantMap guideChannel(const QVariantMap& value, QString* error = nullptr);

/** Validates a guide rule document (bounded fields).
 * @brief Validates a guide rule document (bounded fields).
 * @details Checks that the rule map contains valid fields within bounds.
 * @param value The guide rule map.
 * @param error Output error message on failure.
 * @return A valid guide rule map if validation succeeds, empty map otherwise.
 */
QVariantMap guideRule(const QVariantMap& value, QString* error = nullptr);

/** Validates a guide options document (bounded fields).
 * @brief Validates a guide options document (bounded fields).
 * @details Checks that the options map contains valid fields within bounds.
 * @param value The guide options map.
 * @param error Output error message on failure.
 * @return A valid guide options map if validation succeeds, empty map otherwise.
 */
QVariantMap guideOptions(const QVariantMap& value, QString* error = nullptr);
bool validateGuideOptionsBasics(const QVariantMap& value, QString* error);
bool validateGuideOverlayPresentation(const QVariantMap& value, QVariantMap& presentation, QString* error);
bool validateGuideToolbar(const QVariantMap& value, QString* error);
bool validateGuideSources(const QVariantMap& value, QString* error);

/** Validates a game registration document (title, platform, runtime, root, target, optional steam app id).
 * @brief Validates a game registration document: title, platform, runtime, root, target, optional steam app id.
 * @details Checks that the game registration contains valid title, platform, runtime, root, and target fields. For steam runtime, validates app id.
 * @why Game registration defines how games are registered; invalid registrations would cause incorrect game behavior.
 * @param value The game registration map.
 * @param error Output error message on failure.
 * @return A valid game registration map if validation succeeds, empty map otherwise.
 */
QVariantMap gameRegistration(const QVariantMap& value, QString* error = nullptr);

/** Validates game registration key set (exact six keys).
 * @brief Validates game registration key set (exact six keys).
 * @details Checks that the game registration map contains exactly the six required keys: schemaVersion, title, platform, runtimeId, root, target.
 * @why Game registration requires exactly these keys for proper validation.
 * @param input The game registration map.
 * @return true if the key set is valid, false otherwise.
 */
bool gameRegistrationKeys(const QVariantMap& input);

/** Validates game registration schemaVersion (numeric 1).
 * @brief Validates game registration schemaVersion (must be numeric 1).
 * @details Checks that the schemaVersion field is numeric and equals 1.
 * @param input The game registration map.
 * @return true if schemaVersion is valid, false otherwise.
 */
bool gameRegistrationVersion(const QVariantMap& input);

/** Validates game registration text fields (title, platform, runtimeId, root, target).
 * @brief Validates game registration text fields (title, platform, runtimeId, root, target).
 * @details Checks that all text fields are valid strings within length limits.
 * @param input The game registration map.
 * @return true if text fields are valid, false otherwise.
 */
bool gameRegistrationTextFields(const QVariantMap& input);

/** Validates game registration title/platform fields.
 * @brief Validates game registration title/platform fields.
 * @details Checks that title and platform fields are valid strings within length limits.
 * @param input The game registration map.
 * @return true if title/platform fields are valid, false otherwise.
 */
bool gameRegistrationTitlePlatform(const QVariantMap& input);

/** Validates a steam app id target.
 * @brief Validates a steam app id target.
 * @details Checks that the target field is a valid steam app id.
 * @param input The game registration map.
 * @param error Output error message on failure.
 * @return true if the steam app id is valid, false otherwise.
 */
bool gameRegistrationSteam(const QVariantMap& input, QString* error);

/** Validates game registration root/target pair.
 * @brief Validates game registration root/target pair.
 * @details Checks that root and target fields are valid.
 * @param input The game registration map.
 * @return true if root/target pair is valid, false otherwise.
 */
bool gameRegistrationRootTarget(const QVariantMap& input);

/** Logical page extent is independent of the bounded TV viewport grid.
 */
inline constexpr int MaxScrollableRows = 10240;

/** Validates a component/section presentation map.
 * @brief Validates a component/section presentation map. When section is true the map is a page section; when container is true it is a group container. Allowed state keys: normal, focused, selected, pressed, disabled, unavailable.
 * @details Checks that the presentation map contains valid fields for the given section/container context.
 * @why Presentation maps define how components are visually presented; invalid presentations would cause rendering errors.
 * @param value The presentation map.
 * @param section Whether this is a page section.
 * @param container Whether this is a group container.
 * @return true if the presentation map is valid, false otherwise.
 */
bool validPresentation(const QVariantMap& value, bool section = false, bool container = false);

/** Dispatches a presentation field key to its field-specific validator.
 * @brief Dispatches a presentation field key to its field-specific validator.
 * @details Routes each key in the presentation map to the appropriate field-specific validator based on the key name.
 * @param key The field name to dispatch.
 * @param v The variant value to validate.
 * @param value The parent value map for context.
 * @param section Whether this is a page section.
 * @param container Whether this is a group container.
 * @return true if the field is valid, false otherwise.
 */
bool validPresentationField(const QString& key, const QVariant& v, const QVariantMap& value,
                            bool section, bool container);

/** Validates a child layout map (grid/row/column mode with bounded columns, rows, gap and padding).
 * @brief Validates a child layout map: grid/row/column mode with bounded columns, rows, gap and padding.
 * @details Checks all layout fields: schemaVersion, mode, columns, rows, gap, padding, visibleColumns, wrap.
 * @param value The child layout map.
 * @return true if the layout is valid, false otherwise.
 */
bool validChildLayout(const QVariantMap& value);

/** Validates a page composition document: navigation tabs, sections, grid, background and header settings.
 * @brief Validates a page composition document: navigation tabs, sections, grid, background and header settings.
 * @details Checks all composition fields: schemaVersion, enabled, navigation, initialTab, margin, gap, fit, showProfile, columns, wallpaper, headerMode, identity, headerControls, showClock, designHeight, pageNavigation, sidebarWidth, moviePage, showsPage, grid, sectionHeight, background, verticalScrolling, header, designLibrary, appGallery, captureInputArtwork, settingsCategoryOrder.
 * @param value The composition map.
 * @return true if the composition is valid, false otherwise.
 */
bool validComposition(const QVariantMap& value);

/** Validates composition keys only.
 * @brief Validates composition keys only (known key set).
 * @details Checks that every key in the map is from the known set of composition fields.
 * @param value The composition map.
 * @return true if all keys are known, false otherwise.
 */
bool validCompositionKeys(const QVariantMap& value);

/** Validates settingsCategoryOrder (exact category set, unique).
 * @brief Validates settingsCategoryOrder (exact category set, unique).
 * @details Checks that the settingsCategoryOrder array contains exactly the expected set of category ids with no duplicates.
 * @param value The composition map.
 * @return true if settingsCategoryOrder is valid, false otherwise.
 */
bool validCompositionSettingsCategoryOrder(const QVariantMap& value);

/** Validates captureInputArtwork (bounded artwork map).
 * @brief Validates captureInputArtwork (bounded artwork map).
 * @details Checks that the captureInputArtwork map contains valid artwork fields.
 * @param value The composition map.
 * @return true if captureInputArtwork is valid, false otherwise.
 */
bool validCompositionCaptureInputArtwork(const QVariantMap& value);

/** Validates appGallery (bounded gallery presentation map).
 * @brief Validates appGallery (bounded gallery presentation map).
 * @details Checks that the appGallery map contains valid gallery presentation fields.
 * @param value The composition map.
 * @return true if appGallery is valid, false otherwise.
 */
bool validCompositionAppGallery(const QVariantMap& value);

/** Validates composition header map with geometry bounds.
 * @brief Validates composition header map with geometry bounds.
 * @details Checks that header is a QVariantMap, schemaVersion is numeric 1, all known header fields are within bounds, position/showTitle/showDate are booleans, colors are valid opaque colors, and geometry is sufficient for readable controls.
 * @param header The header map to validate.
 * @param value The parent composition map for context.
 * @return true if the header is valid, false otherwise.
 */
bool validCompositionHeader(const QVariantMap& header, const QVariantMap& value);

/** Validates composition background map (mode, dim, artFit, tint).
 * @brief Validates composition background map (mode, dim, artFit, tint).
 * @details Checks that background is a QVariantMap, mode is one of solid/gradient/image, dim is numeric 0-1, tint is a valid opaque color, and remaining fields pass validPresentation validation.
 * @param value The composition map containing the background field.
 * @return true if background is valid, false otherwise.
 */
bool validCompositionBackground(const QVariantMap& value);

/** Validates composition background keys only.
 * @brief Validates composition background keys only.
 * @details Checks that every key in the background map is from the known set: mode, dim, artFit, artFocalX, artFocalY, artZoom, artAspectLocked, imageCrop, tintColor.
 * @param background The background map to validate.
 * @return true if all keys are known, false otherwise.
 */
bool validCompositionBackgroundKeys(const QVariantMap& background);

/** Validates composition background mode.
 * @brief Validates composition background mode ("solid", "gradient", "image").
 * @details Checks that the value is a QString and matches one of "solid", "gradient", or "image".
 * @param background The background map.
 * @return true if mode is valid, false otherwise.
 */
bool validCompositionBackgroundMode(const QVariantMap& background);

/** Validates composition background tint color.
 * @brief Validates composition background tint color (opaque named color).
 * @details Checks that the value is a QString and represents a valid QColor with alpha 255 (fully opaque).
 * @param tint The tint color value to validate.
 * @return true if the tint is a valid opaque color, false otherwise.
 */
bool validCompositionBackgroundTint(const QVariant& tint);

/** Validates composition background dim value.
 * @brief Validates composition background dim value (0–1).
 * @details Checks that the value is numeric (not QString or Bool), finite, and between 0 and 1 inclusive.
 * @param dim The dim value to validate.
 * @return true if dim is within [0,1], false otherwise.
 */
bool validCompositionBackgroundDim(const QVariant& dim);

/** Validates composition pageNavigation ("left", "top" or "none").
 * @brief Validates composition pageNavigation ("left", "top" or "none").
 * @details Checks that the value is a QString and matches one of "left", "top", or "none".
 * @param value The composition map.
 * @return true if pageNavigation is valid, false otherwise.
 */
bool validCompositionPageNavigation(const QVariantMap& value);

/** Validates composition sidebarWidth (120–400).
 * @brief Validates composition sidebarWidth (120–400).
 * @details Checks that the value is numeric (not QString or Bool) and between 120 and 400 inclusive.
 * @param value The composition map.
 * @return true if sidebarWidth is valid, false otherwise.
 */
bool validCompositionSidebarWidth(const QVariantMap& value);

/** Validates composition moviePage/showsPage (safe ids).
 * @brief Validates composition moviePage/showsPage (safe ids).
 * @details Checks that if present, the value is a QString and passes safeId validation.
 * @param value The composition map.
 * @return true if moviePage/showsPage are valid or absent, false otherwise.
 */
bool validCompositionMovieShowsPage(const QVariantMap& value);

/** Validates composition schemaVersion, columns and grid map.
 * @brief Validates composition schemaVersion, columns and grid map.
 * @details Checks that schemaVersion is numeric 1, columns (if present) is 1 or 2, and grid (if present) passes validGrid validation.
 * @param value The composition map.
 * @return true if version, columns, and grid are valid, false otherwise.
 */
bool validCompositionVersionColumnsGrid(const QVariantMap& value);

/** Validates composition sectionHeight (bounded).
 * @brief Validates composition sectionHeight (bounded).
 * @details Checks that sectionHeight is numeric and within bounds.
 * @param value The composition map.
 * @return true if sectionHeight is valid or absent, false otherwise.
 */
bool validCompositionSectionHeight(const QVariantMap& value);

/** Validates composition wallpaper (artwork path pattern).
 * @brief Validates composition wallpaper (artwork path pattern).
 * @details Checks that the value is a QString matching the artwork path pattern "^artwork/[A-Za-z0-9][A-Za-z0-9._-]*\\.(png|jpg|jpeg)$".
 * @param value The composition map.
 * @return true if wallpaper path is valid, false otherwise.
 */
bool validCompositionWallpaper(const QVariantMap& value);

/** Validates composition headerMode ("navigation" or "identity").
 * @brief Validates composition headerMode ("navigation" or "identity").
 * @details Checks that the value is a QString and matches one of "navigation" or "identity".
 * @param value The composition map.
 * @return true if headerMode is valid, false otherwise.
 */
bool validCompositionHeaderModeIdentity(const QVariantMap& value);

/** Validates composition headerControls (unique, includes settings+edit).
 * @brief Validates composition headerControls (unique, includes settings+edit).
 * @details Checks that headerControls is a QVariantList, non-empty, size at most MaxHeaderControls, contains only known control IDs ("sources", "search", "settings", "edit"), no duplicates, and contains both "settings" and "edit".
 * @param value The composition map.
 * @return true if headerControls is valid, false otherwise.
 */
bool validCompositionHeaderControls(const QVariantMap& value);

/** Validates composition identity (bounded name string).
 * @brief Validates composition identity (bounded name string).
 * @details Checks that the value is a QString, length at most MaxNameLength, and contains no null characters.
 * @param value The composition map.
 * @return true if identity is valid, false otherwise.
 */
bool validCompositionIdentity(const QVariantMap& value);

/** Validates composition verticalScrolling (grid mode screen).
 * @brief Validates composition verticalScrolling (grid mode screen).
 * @details Checks that if verticalScrolling is true, enabled is true, grid is present, and grid mode is "screen".
 * @param value The composition map.
 * @return true if verticalScrolling is valid or absent, false otherwise.
 */
bool validCompositionVerticalScrolling(const QVariantMap& value);

/** Validates composition boolean keys.
 * @brief Validates composition boolean keys (enabled, fit, showProfile, showClock, verticalScrolling).
 * @details Checks that if present, each of enabled, fit, showProfile, showClock, verticalScrolling has metaType Bool.
 * @param value The composition map.
 * @return true if all boolean keys are valid, false otherwise.
 */
bool validCompositionBoolKeys(const QVariantMap& value);

/** Validates composition margin/gap.
 * @brief Validates composition margin/gap (bounded 0–100).
 * @details Checks that if margin/gap are present, they are numeric (not QString or Bool), finite, and between 0 and 100.
 * @param value The composition map.
 * @return true if margin/gap are valid or absent, false otherwise.
 */
bool validCompositionMarginGap(const QVariantMap& value);

/** Validates composition navigation tabs (unique ids, bounded names).
 * @brief Validates composition navigation tabs (unique ids, bounded names).
 * @details Checks that navigation is a QVariantList with size at most MaxNavigationTabs, each tab has a unique safe id, a non-empty trimmed name within MaxNameLength, valid keys (id, name, actions, sections), valid actions, and valid sections.
 * @param value The composition map.
 * @return true if navigation tabs are valid, false otherwise.
 */
bool validCompositionNavigationTabs(const QVariantMap& value);

/** Validates composition navigation tab list type and count.
 * @brief Validates composition navigation tab list type and count.
 * @details Checks that navigation is a QVariantList with size at most MaxNavigationTabs.
 * @param value The composition map.
 * @return true if navigation tab list type and count are valid, false otherwise.
 */
bool validCompositionNavigationTabsType(const QVariantMap& value);

/** Validates a single navigation tab.
 * @brief Validates a single navigation tab (id, name, actions, sections).
 * @details Checks that id is safe and unique, name is non-empty trimmed within MaxNameLength, keys are from {id, name, actions, sections}, actions pass validation, and sections pass validation.
 * @param tab The tab map to validate.
 * @param ids Set of already-seen tab ids for uniqueness checking.
 * @return true if the tab is valid, false otherwise.
 */
bool validNavigationTab(const QVariantMap& tab, QSet<QString>& ids);

/** Validates navigation tab keys only.
 * @brief Validates navigation tab keys only.
 * @details Checks that every key in the tab map is from the known set: id, name, actions, sections.
 * @param tab The tab map to validate.
 * @return true if all keys are known, false otherwise.
 */
bool validNavigationTabKeys(const QVariantMap& tab);

/** Validates navigation tab actions.
 * @brief Validates navigation tab actions (non-empty validated list).
 * @details Checks that if actions is present, it is a non-empty QVariantList and passes actions validation.
 * @param tab The tab map to validate.
 * @return true if actions are valid or absent, false otherwise.
 */
bool validNavigationTabActions(const QVariantMap& tab);

/** Validates navigation tab sections.
 * @brief Validates navigation tab sections (safe ids).
 * @details Checks that if sections is present, it is a QVariantList and each element passes safeId validation.
 * @param tab The tab map to validate.
 * @return true if sections are valid or absent, false otherwise.
 */
bool validNavigationTabSections(const QVariantMap& tab);

/** Validates composition initialTab.
 * @brief Validates composition initialTab (references an existing tab id).
 * @details Checks that if initialTab is present, its value references an existing tab id from the navigation tabs.
 * @param value The composition map.
 * @return true if initialTab is valid or absent, false otherwise.
 */
bool validCompositionInitialTab(const QVariantMap& value);

/** Validates composition optional fields.
 * @brief Validates composition optional fields (settingsCategoryOrder, captureInputArtwork, appGallery, designLibrary).
 * @details Checks settingsCategoryOrder, captureInputArtwork, appGallery, and designLibrary if present.
 * @param value The composition map.
 * @return true if all optional fields are valid, false otherwise.
 */
bool validCompositionOptionalFields(const QVariantMap& value);

/** Validates composition layout fields.
 * @brief Validates composition layout fields (header, background, pageNavigation, sidebarWidth).
 * @details Checks header, background, pageNavigation, and sidebarWidth if present.
 * @param value The composition map.
 * @return true if layout fields are valid, false otherwise.
 */
bool validCompositionLayout(const QVariantMap& value);

/** Validates composition grid fields.
 * @brief Validates composition grid fields (moviePage, showsPage, version, columns, grid, sectionHeight, wallpaper).
 * @details Checks moviePage/showsPage safe ids, version/columns/grid via versionColumnsGrid, sectionHeight bounds, and wallpaper path pattern.
 * @param value The composition map.
 * @return true if grid fields are valid, false otherwise.
 */
bool validCompositionGrid(const QVariantMap& value);

/** Validates composition identity fields.
 * @brief Validates composition identity fields (headerMode, identity, headerControls).
 * @details Checks headerMode, identity, and headerControls if present.
 * @param value The composition map.
 * @return true if identity fields are valid, false otherwise.
 */
bool validCompositionIdentity(const QVariantMap& value);

/** Validates composition scrolling fields.
 * @brief Validates composition scrolling fields (verticalScrolling, bool keys, margin, gap).
 * @details Checks verticalScrolling, boolean keys, and margin/gap if present.
 * @param value The composition map.
 * @return true if scrolling fields are valid, false otherwise.
 */
bool validCompositionScrolling(const QVariantMap& value);

/** Validates a placement grid.
 * @brief Validates a placement grid: columns 4-16, rows 3-10, gap/margin 0-100.
 * @details Checks that the map contains only known keys (columns, rows, gap, margin, snap, overlap, mode), mode is "screen" or "sections" if present, columns/rows are whole numbers within bounds, gap/margin are within bounds, and snap/overlap are booleans if present.
 * @param value The grid map.
 * @return true if the grid is valid, false otherwise.
 */
bool validGrid(const QVariantMap& value);

/** Validates composition grid keys only.
 * @brief Validates composition grid keys only.
 * @details Checks that every key in the grid map is from the known set: columns, rows, gap, margin, snap, overlap, mode.
 * @param value The grid map.
 * @return true if all keys are known, false otherwise.
 */
bool validGridKeys(const QVariantMap& value);

/** Validates composition grid mode.
 * @brief Validates composition grid mode ("screen" or "sections").
 * @details Checks that the value is a QString and matches "screen" or "sections".
 * @param value The grid map.
 * @return true if mode is valid, false otherwise.
 */
bool validGridMode(const QVariantMap& value);

/** Validates composition grid columns or rows field.
 * @brief Validates composition grid columns or rows field.
 * @details Checks that the value is numeric (not QString or Bool), finite, a whole number, and within bounds (columns: 4-16, rows: 3-10).
 * @param value The grid map.
 * @param key The field name ("columns" or "rows").
 * @return true if columns/rows are valid, false otherwise.
 */
bool validGridColumnsRows(const QVariantMap& value, const QString& key);

/** Validates composition grid gap/margin field.
 * @brief Validates composition grid gap/margin field.
 * @details Checks that if gap/margin is present, it is numeric (not QString or Bool), finite, and between 0 and 100.
 * @param value The grid map.
 * @return true if gap/margin are valid or absent, false otherwise.
 */
bool validGridGapMargin(const QVariantMap& value);

/** Validates composition grid boolean keys.
 * @brief Validates composition grid boolean keys (snap, overlap).
 * @details Checks that if present, snap and overlap have metaType Bool.
 * @param value The grid map.
 * @return true if boolean keys are valid or absent, false otherwise.
 */
bool validGridBoolKeys(const QVariantMap& value);

/** Returns the input grid filled with screen-mode defaults.
 * @brief Returns the input grid filled with screen-mode defaults (8 columns x 5 rows, gap 12, margin 20) for any missing keys.
 * @details Fills in missing columns, rows, gap, margin with default values for screen-mode grids.
 * @param value The grid map to normalize.
 * @return The normalized grid map with defaults filled in.
 */
QVariantMap normalizedGrid(const QVariantMap& value);

/** Validates a single placement.
 * @brief Validates a single placement: keys x/y/width/height, integers, non-empty positive size, contained within maxColumns x maxRows.
 * @details Checks that the map has exactly keys x/y/width/height, each is a whole number within bounds (x/width: 0-maxColumns, y/height: 0-maxRows), width and height are positive, and x+width <= maxColumns, y+height <= maxRows.
 * @param value The placement map.
 * @param maxColumns Maximum columns in the grid.
 * @param maxRows Maximum rows in the grid.
 * @return true if the placement is valid, false otherwise.
 */
bool validPlacement(const QVariantMap& value, int maxColumns, int maxRows);

/** Validates placement keys only.
 * @brief Validates placement keys only (x, y, width, height).
 * @details Checks that the map has exactly keys x/y/width/height.
 * @param value The placement map.
 * @return true if keys are exactly x/y/width/height, false otherwise.
 */
bool validPlacementKeys(const QVariantMap& value);

/** Validates a single placement dimension field.
 * @brief Validates a single placement dimension field.
 * @details Checks that the value for the given key is numeric (not QString or Bool), finite, a whole number, within bounds (x/width: 0-maxColumns, y/height: 0-maxRows).
 * @param value The placement map.
 * @param key The dimension field name ("x", "y", "width", or "height").
 * @param maxColumns Maximum columns in the grid.
 * @param maxRows Maximum rows in the grid.
 * @return true if the dimension is valid, false otherwise.
 */
bool validPlacementDimension(const QVariantMap& value, const QString& key, int maxColumns, int maxRows);

/** True when two placements overlap as axis-aligned rectangles.
 * @brief True when two placements overlap as axis-aligned rectangles.
 * @details Checks if two placement maps overlap by comparing their x/y/width/height rectangles.
 * @param a The first placement map.
 * @param b The second placement map.
 * @return true if the placements overlap, false otherwise.
 */
bool placementsOverlap(const QVariantMap& a, const QVariantMap& b);

/** True when every placed item fits the grid and none overlap.
 * @brief True when every placed item fits the grid and none overlap (unless overlap is enabled on the grid).
 * @details Checks that all items in the list fit within the grid bounds and none overlap (unless the grid has overlap enabled).
 * @param items The list of placement items.
 * @param grid The grid map defining bounds.
 * @return true if all items fit and don't overlap, false otherwise.
 */
bool placementsFit(const QJsonArray& items, const QVariantMap& grid);

/** Computes the effective grid for a category.
 * @brief Computes the effective grid for a category: category presentation override applied on top of the composition grid.
 * @details Merges category-specific presentation overrides with the base composition grid to produce an effective grid.
 * @param composition The composition map.
 * @param categories The categories array.
 * @param categoryId The category id to compute the grid for.
 * @return The effective grid map for the category.
 */
QVariantMap sectionGrid(const QVariantMap& composition, const QJsonArray& categories,
                        const QString& categoryId);

/** Validates every placement in a document against its composition grid and category sections.
 * @brief Validates every placement in a document against its composition grid and category sections, including nested containers.
 * @details Checks that all items in the list fit within their respective grids and none overlap.
 * @param items The list of placement items.
 * @param composition The composition map.
 * @param categories The categories array.
 * @return true if all placements fit, false otherwise.
 */
bool layoutPlacementsFit(const QJsonArray& items, const QVariantMap& composition, const QJsonArray& categories);

/** Validates the full component hierarchy.
 * @brief Validates the full component hierarchy: unique ids, valid parents, bounded nesting depth and placements that fit each container grid. Sets error on failure and returns false.
 * @details Validates component identities (unique safe ids, builds parent/child maps), parent references (existing container parents, no cycles, bounded nesting depth), container placements, layout, child fit, root-level placement and section fit.
 * @param items The list of component items.
 * @param composition The composition map.
 * @param categories The categories array.
 * @param error Output error message on failure.
 * @return true if the hierarchy fits, false otherwise.
 */
bool hierarchyFits(const QJsonArray& items,
                   const QVariantMap& composition,
                   const QJsonArray& categories,
                   QString* error = nullptr);

/** Validates component identities.
 * @brief Validates component identities: unique safe ids, builds parent/child maps.
 * @details Iterates all items to build id maps, checks for unique safe ids, builds parent/child maps, marks containers, checks for self-referencing parents and bounded nesting depth.
 * @param byId Output map of id to component object.
 * @param children Output map of parent id to child array.
 * @param roots Output array of root items.
 * @param containers Output flag indicating if containers exist.
 * @param items The list of component items.
 * @param error Output error message on failure.
 * @return true if identities are valid, false otherwise.
 */
bool hierarchyFitsValidateIds(QHash<QString, QJsonObject>& byId, QHash<QString, QJsonArray>& children,
                              QJsonArray& roots, bool& containers, const QJsonArray& items, QString* error);

/** Validates component parent references: existing container parents, no cycles, bounded nesting depth.
 * @brief Validates component parent references: existing container parents, no cycles, bounded nesting depth.
 * @param byId The map of id to component object.
 * @param children The map of parent id to child array.
 * @param error Output error message on failure.
 * @return true if parent references are valid, false otherwise.
 */
bool hierarchyFitsValidateParents(const QHash<QString, QJsonObject>& byId,
                                  const QHash<QString, QJsonArray>& children,
                                  QString* error);

/** Validates component containers: placement, layout, child fit.
 * @brief Validates component containers: placement, layout, child fit.
 * @param byId The map of id to component object.
 * @param children The map of parent id to child array.
 * @param composition The composition map.
 * @param categories The categories array.
 * @param error Output error message on failure.
 * @return true if containers are valid, false otherwise.
 */
bool hierarchyFitsValidateContainers(const QHash<QString, QJsonObject>& byId,
                                     const QHash<QString, QJsonArray>& children,
                                     const QVariantMap& composition, const QJsonArray& categories,
                                     QString* error);

/** Validates a single container: family-specific child rules, placement, layout, grid fit.
 * @brief Validates a single container: family-specific child rules, placement, layout, grid fit.
 * @param item The container item to validate.
 * @param children The map of parent id to child array.
 * @param composition The composition map.
 * @param categories The categories array.
 * @param id The container id.
 * @param fail Callback for failure reporting.
 * @return true if the container is valid, false otherwise.
 */
bool hierarchyFitsValidateContainer(const QJsonObject& item,
                                    const QHash<QString, QJsonArray>& children,
                                    const QVariantMap& composition,
                                    const QJsonArray& categories,
                                    const QString& id,
                                    const std::function<bool(const QString&)>& fail);

/** Validates root-level components: placement, section fit, library exclusivity.
 * @brief Validates root-level components: placement, section fit, library exclusivity.
 * @param byId The map of id to component object.
 * @param items The list of component items.
 * @param composition The composition map.
 * @param categories The categories array.
 * @param error Output error message on failure.
 * @return true if root-level components are valid, false otherwise.
 */
bool hierarchyFitsValidateRoots(const QHash<QString, QJsonObject>& byId, const QJsonArray& items,
                                const QVariantMap& composition, const QJsonArray& categories,
                                QString* error);

/** Computes the grid for a nested container's children, or the section grid when parentId is empty.
 * @brief Computes the grid for a nested container's children, or the section grid when parentId is empty.
 * @param items The list of component items.
 * @param composition The composition map.
 * @param categories The categories array.
 * @param parentId The parent container id.
 * @param categoryId The category id.
 * @return The computed grid map.
 */
QVariantMap componentGrid(const QJsonArray& items,
                          const QVariantMap& composition,
                          const QJsonArray& categories,
                          const QString& parentId,
                          const QString& categoryId);

/** Validates a typed component and normalizes its actions.
 * Editor commands can derive the source after an action changes. Stored designs
 * must supply the matching source explicitly.
 */
QVariantMap component(const QVariantMap& input, bool deriveSource, QString* error = nullptr);

/** Validates component type and shape fields.
 * @brief Validates component type and shape fields.
 * @details Checks that the component type and shape fields are valid.
 * @param input The component map.
 * @param type The component type.
 * @param error Output error message on failure.
 * @return true if type and shape are valid, false otherwise.
 */
bool componentValidateTypeAndShape(const QVariantMap& input, const QString& type, QString* error);

/** Validates component name field.
 * @brief Validates component name field.
 * @details Checks that the name field is valid (non-empty, within length limits).
 * @param input The component map.
 * @param error Output error message on failure.
 * @return true if name is valid, false otherwise.
 */
bool componentValidateName(const QVariantMap& input, QString* error);

/** Validates component family field.
 * @brief Validates component family field.
 * @details Checks that the family field is valid.
 * @param input The component map.
 * @param type The component type.
 * @param error Output error message on failure.
 * @return true if family is valid, false otherwise.
 */
bool componentValidateFamily(const QVariantMap& input, const QString& type, QString* error);

/** Validates component appCollection field.
 * @brief Validates component appCollection field.
 * @details Checks that the appCollection field is valid.
 * @param input The component map.
 * @param type The component type.
 * @param error Output error message on failure.
 * @return true if appCollection is valid, false otherwise.
 */
bool componentValidateAppCollection(const QVariantMap& input, const QString& type, QString* error);

/** Validates app collection shape (key count, mode, ids).
 * @brief Validates app collection shape (key count, mode, ids).
 * @details Checks that the collection map has the correct key count, mode, and ids.
 * @param collection The collection map to validate.
 * @param error Output error message on failure.
 * @return true if collection shape is valid, false otherwise.
 */
bool componentValidateAppCollectionShape(const QVariantMap& collection, QString* error);

/** Validates app collection showStore field (boolean).
 * @brief Validates app collection showStore field (boolean).
 * @details Checks that showStore is a boolean value.
 * @param collection The collection map to validate.
 * @param error Output error message on failure.
 * @return true if showStore is valid, false otherwise.
 */
bool componentValidateAppCollectionShowStore(const QVariantMap& collection, QString* error);

/** Validates app collection section field.
 * @brief Validates app collection section field.
 * @details Checks that the section field is valid.
 * @param collection The collection map to validate.
 * @param error Output error message on failure.
 * @return true if section is valid, false otherwise.
 */
bool componentValidateAppCollectionSection(const QVariantMap& collection, QString* error);

/** Validates app collection ids (unique safe ids).
 * @brief Validates app collection ids (unique safe ids).
 * @details Checks that ids are unique and safe.
 * @param collection The collection map to validate.
 * @param error Output error message on failure.
 * @return true if ids are valid, false otherwise.
 */
bool componentValidateAppCollectionIds(const QVariantMap& collection, QString* error);

/** Validates app collection mode ("all" requires empty ids).
 * @brief Validates app collection mode ("all" requires empty ids).
 * @details Checks that mode is valid ("all" requires empty ids).
 * @param collection The collection map to validate.
 * @param error Output error message on failure.
 * @return true if mode is valid, false otherwise.
 */
bool componentValidateAppCollectionMode(const QVariantMap& collection, QString* error);

/** Validates component accessibility map (name/hint bounded strings).
 * @brief Validates component accessibility map (name/hint bounded strings).
 * @details Checks that accessibility name/hint strings are bounded.
 * @param input The component map.
 * @param error Output error message on failure.
 * @return true if accessibility map is valid, false otherwise.
 */
bool componentValidateAccessibility(const QVariantMap& input, QString* error);

/** Validates component parentId field (safe id or empty).
 * @brief Validates component parentId field (safe id or empty).
 * @details Checks that parentId is either empty or a safe id.
 * @param input The component map.
 * @param error Output error message on failure.
 * @return true if parentId is valid, false otherwise.
 */
bool componentValidateParentId(const QVariantMap& input, QString* error);

/** Validates component children field (must be absent).
 * @brief Validates component children field (must be absent).
 * @details Checks that children field is absent.
 * @param input The component map.
 * @param error Output error message on failure.
 * @return true if children field is absent, false otherwise.
 */
bool componentValidateChildren(const QVariantMap& input, QString* error);

/** Validates component requiredCapability field (container + capture-inputs only).
 * @brief Validates component requiredCapability field (container + capture-inputs only).
 * @details Checks that requiredCapability is valid for container type with capture-inputs.
 * @param input The component map.
 * @param type The component type.
 * @param error Output error message on failure.
 * @return true if requiredCapability is valid, false otherwise.
 */
bool componentValidateRequiredCapability(const QVariantMap& input, const QString& type,
                                         QString* error);

/** Validates component size field ("compact", "standard", "wide").
 * @brief Validates component size field ("compact", "standard", "wide").
 * @details Checks that size is one of "compact", "standard", "wide".
 * @param input The component map.
 * @param error Output error message on failure.
 * @return true if size is valid, false otherwise.
 */
bool componentValidateSize(const QVariantMap& input, QString* error);

/** Validates component opacity field (numeric, bounded).
 * @brief Validates component opacity field (numeric, bounded).
 * @details Checks that opacity is numeric and within bounds.
 * @param input The component map.
 * @param error Output error message on failure.
 * @return true if opacity is valid, false otherwise.
 */
bool componentValidateOpacity(const QVariantMap& input, QString* error);

/** Validates component boolean fields (hidden, locked, showTitle, showType, showLogo, logoFill).
 * @brief Validates component boolean fields (hidden, locked, showTitle, showType, showLogo, logoFill).
 * @details Checks that boolean fields are of type Bool.
 * @param input The component map.
 * @param error Output error message on failure.
 * @return true if boolean fields are valid, false otherwise.
 */
bool componentValidateBooleans(const QVariantMap& input, QString* error);

/** Validates component accent field (opaque named color).
 * @brief Validates component accent field (opaque named color).
 * @details Checks that accent is a valid opaque color.
 * @param input The component map.
 * @param error Output error message on failure.
 * @return true if accent is valid, false otherwise.
 */
bool componentValidateAccent(const QVariantMap& input, QString* error);

/** Validates component symbol field (bounded string).
 * @brief Validates component symbol field (bounded string).
 * @details Checks that symbol is a string within length limits.
 * @param input The component map.
 * @param error Output error message on failure.
 * @return true if symbol is valid, false otherwise.
 */
bool componentValidateSymbol(const QVariantMap& input, QString* error);

/** Validates component linkedDesign/designNode fields (safe ids).
 * @brief Validates component linkedDesign/designNode fields (safe ids).
 * @details Checks that linkedDesign/designNode are safe ids.
 * @param input The component map.
 * @param error Output error message on failure.
 * @return true if linkedDesign/designNode are valid, false otherwise.
 */
bool componentValidateLinkedDesign(const QVariantMap& input, QString* error);

/** Validates component placement field.
 * @brief Validates component placement field.
 * @details Checks that placement is valid.
 * @param input The component map.
 * @param error Output error message on failure.
 * @return true if placement is valid, false otherwise.
 */
bool componentValidatePlacement(const QVariantMap& input, QString* error);

/** Validates component presentation field.
 * @brief Validates component presentation field.
 * @details Checks that presentation is valid.
 * @param input The component map.
 * @param type The component type.
 * @param error Output error message on failure.
 * @return true if presentation is valid, false otherwise.
 */
bool componentValidatePresentation(const QVariantMap& input, const QString& type,
                                   QString* error);

/** Validates component animateArtwork field (image only).
 * @brief Validates component animateArtwork field (image only).
 * @details Checks that animateArtwork is valid for image components only.
 * @param input The component map.
 * @param type The component type.
 * @param error Output error message on failure.
 * @return true if animateArtwork is valid, false otherwise.
 */
bool componentValidateAnimateArtwork(const QVariantMap& input, const QString& type,
                                     QString* error);

/** Validates component customLogo field (artwork path pattern).
 * @brief Validates component customLogo field (artwork path pattern).
 * @details Checks that customLogo matches the artwork path pattern.
 * @param input The component map.
 * @param error Output error message on failure.
 * @return true if customLogo is valid, false otherwise.
 */
bool componentValidateCustomLogo(const QVariantMap& input, QString* error);

/** Groups basic component validations: type/shape + name.
 * @brief Groups basic component validations: type/shape + name.
 * @details Runs type/shape validation and name validation.
 * @param input The component map.
 * @param type The component type.
 * @param error Output error message on failure.
 * @return true if basic validations pass, false otherwise.
 */
bool componentValidateBasic(const QVariantMap& input, const QString& type, QString* error);

/** Groups structural component validations: family, appCollection, accessibility, parentId, children.
 * @brief Groups structural component validations: family, appCollection, accessibility, parentId, children.
 * @details Runs family, appCollection, accessibility, parentId, and children validations.
 * @param input The component map.
 * @param type The component type.
 * @param error Output error message on failure.
 * @return true if structural validations pass, false otherwise.
 */
bool componentValidateStructure(const QVariantMap& input, const QString& type, QString* error);

/** Groups capability component validations: requiredCapability, size, opacity.
 * @brief Groups capability component validations: requiredCapability, size, opacity.
 * @details Runs requiredCapability, size, and opacity validations.
 * @param input The component map.
 * @param type The component type.
 * @param error Output error message on failure.
 * @return true if capability validations pass, false otherwise.
 */
bool componentValidateCapabilities(const QVariantMap& input, const QString& type, QString* error);
bool componentValidatePresentationSettings(const QVariantMap& input, const QString& type, QString* error);

/** Groups all component validations into a single gate.
 * @brief Groups all component validations into a single gate.
 * @details Runs all component validations: basic, structural, capabilities, presentation settings.
 * @param input The component map.
 * @param type The component type.
 * @param error Output error message on failure.
 * @return true if all validations pass, false otherwise.
 */
bool componentValidateAll(const QVariantMap& input, const QString& type, QString* error);

/** Builds a library source map for button/image/library component types.
 * @brief Builds a library source map for button/image/library component types.
 * @param input The component map.
 * @param style The presentation style map.
 * @param error Output error message on failure.
 * @return A valid library source map, or empty map on failure.
 */
QVariantMap componentBuildLibrarySource(const QVariantMap& input, const QVariantMap& style, QString* error);

/** Builds a library source map specifically for library component type.
 * @brief Builds a library source map specifically for library component type.
 * @param input The component map.
 * @param style The presentation style map.
 * @param error Output error message on failure.
 * @return A valid library source map, or empty map on failure.
 */
QVariantMap componentBuildLibrarySourceLibrary(const QVariantMap& input, const QVariantMap& style,
                                                QString* error);

/** Validates library guide field (channels/all/empty).
 * @brief Validates library guide field (channels/all/empty).
 * @details Checks that the library guide field is valid.
 * @param guide Whether this is a guide component.
 * @param style The presentation style map.
 * @param error Output error message on failure.
 * @return true if library guide field is valid, false otherwise.
 */
bool componentValidateLibraryGuide(bool guide, const QVariantMap& style, QString* error);

/** Validates library music field (albums/all/empty).
 * @brief Validates library music field (albums/all/empty).
 * @details Checks that the library music field is valid.
 * @param music Whether this is a music component.
 * @param style The presentation style map.
 * @param error Output error message on failure.
 * @return true if library music field is valid, false otherwise.
 */
bool componentValidateLibraryMusic(bool music, const QVariantMap& style, QString* error);

/** Validates library history field (history/watched/empty).
 * @brief Validates library history field (history/watched/empty).
 * @details Checks that the library history field is valid.
 * @param history Whether this is a history component.
 * @param style The presentation style map.
 * @param error Output error message on failure.
 * @return true if library history field is valid, false otherwise.
 */
bool componentValidateLibraryHistory(bool history, const QVariantMap& style, QString* error);

/** Validates library views fields (libraryKind/libraryView/libraryValue).
 * @brief Validates library views fields (libraryKind/libraryView/libraryValue).
 * @details Checks that library view fields are valid.
 * @param style The presentation style map.
 * @param error Output error message on failure.
 * @return true if library views are valid, false otherwise.
 */
bool componentValidateLibraryViews(const QVariantMap& style, QString* error);

/** Validates library games fields (supported views, platform name bound).
 * @brief Validates library games fields (supported views, platform name bound).
 * @details Checks that library games fields are valid.
 * @param style The presentation style map.
 * @param error Output error message on failure.
 * @return true if library games fields are valid, false otherwise.
 */
bool componentValidateLibraryGames(const QVariantMap& style, QString* error);

/** Builds a shell source map for clock/text/shape/container component types.
 * @brief Builds a shell source map for clock/text/shape/container component types.
 * @param input The component map.
 * @param type The component type.
 * @param error Output error message on failure.
 * @return A valid shell source map, or empty map on failure.
 */
QVariantMap componentBuildShellSource(const QVariantMap& input, const QString& type, QString* error);

/** Builds a launcher source map from normalized actions.
 * @brief Builds a launcher source map from normalized actions.
 * @param input The component map.
 * @param normalizedList The normalized actions list.
 * @param actionType The action type.
 * @param error Output error message on failure.
 * @return A valid launcher source map, or empty map on failure.
 */
QVariantMap componentBuildLauncherSource(const QVariantMap& input, const QVariantList& normalizedList,
                                         const QString& actionType, QString* error);
QVariantMap componentBuildLauncherSource(const QVariantMap& input, const QVariantList& normalizedList,
                                         const QString& actionType);

/** Human-readable unavailability reason for an action, or empty when available. Missing apps, collections and incompatible open-with handlers are reported; shell routes never require an Open With app.
 * @brief Human-readable unavailability reason for an action, or empty when available.
 * @details Missing apps, collections and incompatible open-with handlers are reported; shell routes never require an Open With app.
 * @param action The action map.
 * @param apps The app registry.
 * @param collections The collections list.
 * @return A human-readable unavailability reason, or empty string if available.
 */
QString unavailable(const QVariantMap& action, const AppRegistry* apps, const QVariantList& collections);

/** Returns empty for shell routes (open-page, open-apps, open-search, app-drawer, root-page).
 * @brief Returns empty for shell routes (open-page, open-apps, open-search, app-drawer, root-page).
 * @details Shell routes never require an Open With app.
 * @param type The route type.
 * @return Empty string for shell routes.
 */
QString unavailableShellRoutes(const QString& type);

/** Validates launch-app target against app registry.
 * @brief Validates launch-app target against app registry.
 * @details Checks that the launch-app target exists in the app registry.
 * @param value The action map.
 * @param apps The app registry.
 * @return A human-readable unavailability reason, or empty string if available.
 */
QString unavailableLaunchApp(const QVariantMap& value, const AppRegistry* apps);

/** Validates open-collection/play-movie/open-show target against collections.
 * @brief Validates open-collection/play-movie/open-show target against collections.
 * @details Checks that the collection target exists in the collections list.
 * @param value The action map.
 * @param collections The collections list.
 * @return A human-readable unavailability reason, or empty string if available.
 */
QString unavailableCollection(const QVariantMap& value, const QVariantList& collections);

/** Validates open-* target against app registry openWith handler.
 * @brief Validates open-* target against app registry openWith handler.
 * @details Checks that the openWith handler exists in the app registry.
 * @param value The action map.
 * @param apps The app registry.
 * @return A human-readable unavailability reason, or empty string if available.
 */
QString unavailableOpenWith(const QVariantMap& value, const AppRegistry* apps);

} // namespace ComponentSchema
