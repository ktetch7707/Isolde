#pragma once

namespace GamesResourceLimits {
inline constexpr int CatalogEntries = 50000;
inline constexpr int PageEntries = 50;
inline constexpr int Platforms = 128;
inline constexpr int Collections = 256;
inline constexpr int EditionsPerGame = 32;
inline constexpr int ProvenanceRecords = CatalogEntries * EditionsPerGame;
inline constexpr long long CatalogArchiveBytes = 32LL * 1024 * 1024;
inline constexpr long long ArtworkRestoreBytes = 128LL * 1024 * 1024;
inline constexpr long long ArtworkStoreBytes = 512LL * 1024 * 1024;
}
