#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceCheckRuleBase.h"
#include "LevelSequenceCheckRuleEmptySection.generated.h"

class UMovieSceneSection;
class UMovieSceneTrack;

/**
 * S-02: Empty Section Rule
 * Checks whether a LevelSequence contains rows that have no keyframes across all sections.
 * Detection granularity is at the Row level — if any section on a row has keyframes, the entire row is skipped.
 */
UCLASS(meta=(DisplayName="S-02 空 Section"))
class LEVELSEQUENCECHECKER_API ULevelSequenceCheckRuleEmptySection : public ULevelSequenceCheckRuleBase
{
	GENERATED_BODY()

public:
	ULevelSequenceCheckRuleEmptySection();

	//~ Begin ULevelSequenceCheckRuleBase interface
	virtual FName GetRuleId() const override;
	virtual FText GetRuleName() const override;
	virtual FName GetCategory() const override;
	virtual void ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults) override;
	virtual void CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder) override;
	//~ End ULevelSequenceCheckRuleBase interface

private:
	/** Check whether a section is empty (all channels have zero keys) */
	bool IsSectionEmpty(UMovieSceneSection* Section) const;

	/** Check whether a section has zero or negative duration */
	bool IsDegenerateSection(UMovieSceneSection* Section) const;

	/** Check all sections on a track grouped by RowIndex and report empty rows */
	void CheckTrackSections(UMovieSceneTrack* Track, const FText& TrackLocationPrefix, TArray<FLevelSequenceCheckResult>& OutResults);
};
