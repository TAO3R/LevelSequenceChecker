#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceCheckResult.h"

struct FCachedMissingEntry
{
	FString MissingPackage;
	FString LocationInfo;
	FString Description;
};

struct FCachedSequenceRecord
{
	FString LastDetectedTime;
	TArray<FCachedMissingEntry> Entries;
};

/**
 * Manages persistent cache of missing reference records for R-01.
 * Stored as JSON under Saved/LevelSequenceChecker/MissingRefCache.json.
 *
 * Lifecycle:
 *   - Module Startup: LoadFromDisk()
 *   - R-01 ExecuteCheck: OverwriteCache() — replace cache with current check results
 *   - Module Shutdown: SaveToDisk()
 *   - Manual clear via Settings panel button
 *
 * Design: pure overwrite strategy — each check completely replaces the cached
 * record for a sequence. This ensures that when a user removes a track/section,
 * the stale cached entries are automatically eliminated on next save.
 */
class FMissingRefCache
{
public:
	/** Load cache from disk (call at module startup) */
	void LoadFromDisk();

	/** Save cache to disk (call at module shutdown, also after each OverwriteCache) */
	void SaveToDisk();

	/** Clear all cached records and delete the file */
	void ClearAll();

	/** Clear cached records for a specific Sequence */
	void ClearForSequence(const FString& SequencePath);

	/**
	 * Replace cached records for a sequence with the current check results.
	 * Pure overwrite strategy: no merging with old records. This guarantees
	 * that stale entries (e.g. deleted tracks) are automatically removed.
	 *
	 * @param SequencePath  The asset path of the LevelSequence being checked
	 * @param CurrentResults  Results found during this check run
	 */
	void OverwriteCache(
		const FString& SequencePath,
		const TArray<FLevelSequenceCheckResult>& CurrentResults);

private:
	TMap<FString, FCachedSequenceRecord> Records;
	FString GetCacheFilePath() const;
};
