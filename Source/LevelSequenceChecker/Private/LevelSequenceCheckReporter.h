#pragma once

#include "CoreMinimal.h"

class ULevelSequence;
struct FLevelSequenceCheckResult;

/**
 * Formats check results into the Message Log panel.
 * Uses FMessageLog with Tokenized Messages for:
 *   - Per-save NewPage for grouping
 *   - AssetNameToken for clickable asset path
 *   - TextTokens for rule ID, name, description, and location
 *   - Notify() for bottom-of-screen summary toast
 */
class FLevelSequenceCheckReporter
{
public:
	FLevelSequenceCheckReporter() = default;

	/** Format and output all results for a single LevelSequence check run */
	void Report(ULevelSequence* InSequence, const TArray<FLevelSequenceCheckResult>& InResults, bool bHasErrors = false);
};
