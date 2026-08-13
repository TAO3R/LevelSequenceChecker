#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceCheckRuleBase.h"
#include "LevelSequenceCheckRuleTrackCount.generated.h"

class UMovieScene;

/**
 * S-04: Track Count Limit Rule
 * Checks whether the total number of tracks in a LevelSequence exceeds a configured threshold.
 */
UCLASS(meta=(DisplayName="S-04 Track 数量超限"))
class LEVELSEQUENCECHECKER_API ULevelSequenceCheckRuleTrackCount : public ULevelSequenceCheckRuleBase
{
	GENERATED_BODY()

public:
	ULevelSequenceCheckRuleTrackCount();

	/** Maximum allowed track count. If the total exceeds this value, the rule fires. */
	UPROPERTY(EditAnywhere, Category = "Rule", meta = (ClampMin = 1, UIMin = 1, DisplayName = "轨道数量上限"))
	int32 MaxTrackCount = 50;

	//~ Begin ULevelSequenceCheckRuleBase interface
	virtual FName GetRuleId() const override;
	virtual FText GetRuleName() const override;
	virtual FName GetCategory() const override;
	virtual void ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults) override;
	virtual void CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder) override;
	//~ End ULevelSequenceCheckRuleBase interface

private:
	/** Count all tracks in the MovieScene (master tracks + binding tracks) */
	int32 CountAllTracks(UMovieScene* InMovieScene) const;
};
