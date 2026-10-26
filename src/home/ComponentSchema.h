#pragma once
#include <QVariantMap>
#include <QVariantList>
class AppRegistry;

// Data contracts only. No process launching, networking or Kodi mutations.
// Every validator returns true only for documents that satisfy the layout
// bounds declared in ComponentSchema.cpp; values are never clamped silently.
namespace ComponentSchema {
// Versioned native-channel definitions and bounded nested rule trees.
QVariantMap guideChannel(const QVariantMap &value, QString *error = nullptr);
QVariantMap guideRule(const QVariantMap &value, QString *error = nullptr);
QVariantMap gameRegistration(const QVariantMap &value, QString *error = nullptr);
// Logical page extent is independent of the bounded TV viewport grid.
inline constexpr int MaxScrollableRows = 10240;
// Validates a component/section presentation map. When section is true the
// map is a page section; when container is true it is a group container.
// Allowed state keys: normal, focused, selected, pressed, disabled, unavailable.
bool validPresentation(const QVariantMap &value, bool section = false, bool container = false);
// Validates a child layout map (grid/row/column mode with bounded columns,
// rows, gap and padding).
bool validChildLayout(const QVariantMap &value);
// Validates a page composition document: navigation tabs, sections, grid,
// background and header settings.
bool validComposition(const QVariantMap &value);
// Validates a placement grid: columns 4-16, rows 3-10, gap/margin 0-100.
bool validGrid(const QVariantMap &value);
// Returns the input grid filled with screen-mode defaults (8 columns x 5 rows,
// gap 12, margin 20) for any missing keys.
QVariantMap normalizedGrid(const QVariantMap &value);
// Validates a single placement: keys x/y/width/height, integers, non-empty
// positive size, contained within maxColumns x maxRows.
bool validPlacement(const QVariantMap &value, int maxColumns, int maxRows);
// True when two placements overlap as axis-aligned rectangles.
bool placementsOverlap(const QVariantMap &a, const QVariantMap &b);
// True when every placed item fits the grid and none overlap (unless overlap
// is enabled on the grid).
bool placementsFit(const QJsonArray &items, const QVariantMap &grid);
// Computes the effective grid for a category: category presentation override
// applied on top of the composition grid.
QVariantMap sectionGrid(const QVariantMap &composition, const QJsonArray &categories, const QString &categoryId);
// Validates every placement in a document against its composition grid and
// category sections, including nested containers.
bool layoutPlacementsFit(const QJsonArray &items, const QVariantMap &composition, const QJsonArray &categories);
// Validates the full component hierarchy: unique ids, valid parents, bounded
// nesting depth and placements that fit each container grid. Sets error on
// failure and returns false.
bool hierarchyFits(const QJsonArray &items, const QVariantMap &composition, const QJsonArray &categories, QString *error = nullptr);
// Computes the grid for a nested container's children, or the section grid
// when parentId is empty.
QVariantMap componentGrid(const QJsonArray &items, const QVariantMap &composition, const QJsonArray &categories, const QString &parentId, const QString &categoryId);
// Category inherited by id through the parentId chain; empty when the chain
// is broken, cyclic or deeper than MaxComponentNestingDepth.
QString effectiveCategory(const QJsonArray &items, const QString &id);
// True when id or any ancestor in the parentId chain is locked or missing.
bool positionLocked(const QJsonArray &items, const QString &id);
// True when id matches the safe identifier pattern (alphanumeric start,
// dots, dashes, underscores, max MaxComponentIdLength characters) and is
// neither "." nor "..".
bool safeId(const QString &id);
// Normalizes and validates an action map; returns an empty map and sets
// error on rejection. Owns the full list of supported action types.
QVariantMap action(const QVariantMap &input, QString *error = nullptr);
// Normalizes and validates a component map; legacy mode accepts v1 documents.
// Returns an empty map and sets error on rejection.
QVariantMap component(const QVariantMap &input, bool legacy, QString *error = nullptr);
// Human-readable unavailability reason for an action, or empty when
// available. Missing apps, collections and incompatible open-with handlers
// are reported here; page existence is checked by the shell dispatcher.
QString unavailable(const QVariantMap &action, const AppRegistry *apps, const QVariantList &collections);

// Runtime cache bounds shared by the schema module and PageData, per active
// profile (AGENTS.md: bound everything, never clamp silently).
constexpr int MaxPageDataClientKeySize = 330;      // full client key length
constexpr int MaxPageDataClientIdLength = 160;     // id prefix validated with safeId
constexpr int MaxPageDataClients = 128;            // library clients
constexpr int MaxPageDataArtwork = 512;            // artwork items

// Action input bounds (used by ComponentSchema::action and the shell).
constexpr int MinHdmiInput = 1;
constexpr int MaxHdmiInput = 8;

// Opacity / roundness ratio range.
constexpr double MinOpacity = 0.0;
constexpr double MaxOpacity = 1.0;

// Default tile corner roundness when a theme does not specify one.
constexpr double DefaultTileRoundness = 0.18;

// Page composition bounds.
constexpr int MinSidebarWidth = 120;
constexpr int MaxSidebarWidth = 400;
constexpr int MinSectionHeight = 60;
constexpr int MaxSectionHeight = 800;
constexpr int MinDesignHeight = 480;
constexpr int MaxDesignHeight = 1440;

// Container nesting: groups support at most four nested container levels.
constexpr int MaxComponentNestingDepth = 4;

}
