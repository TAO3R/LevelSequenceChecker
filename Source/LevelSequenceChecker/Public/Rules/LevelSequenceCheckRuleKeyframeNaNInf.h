#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceCheckRuleBase.h"
#include "LevelSequenceCheckRuleKeyframeNaNInf.generated.h"

class UMovieSceneSection;

/**
 * K-01: Keyframe NaN / Inf Rule
 * Checks whether any numeric channel (Float / Double / Vector / Color / Transform, etc.)
 * contains keyframe values that are NaN or Inf.
 */
UCLASS(meta=(DisplayName="K-01 关键帧 NaN/Inf"))
class LEVELSEQUENCECHECKER_API ULevelSequenceCheckRuleKeyframeNaNInf : public ULevelSequenceCheckRuleBase
{
	GENERATED_BODY()

public:
	ULevelSequenceCheckRuleKeyframeNaNInf();

	//~ Begin ULevelSequenceCheckRuleBase interface
	virtual FName GetRuleId() const override;
	virtual FText GetRuleName() const override;
	virtual FName GetCategory() const override;
	virtual void ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults) override;
	virtual void CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder) override;
	//~ End ULevelSequenceCheckRuleBase interface

private:
	/** Check Float channels for NaN/Inf */
	void CheckFloatChannels(
		UMovieSceneSection* Section,
		const FText& LocationContext,
		TArray<FLevelSequenceCheckResult>& OutResults);

	/** Check Double channels for NaN/Inf */
	void CheckDoubleChannels(
		UMovieSceneSection* Section,
		const FText& LocationContext,
		TArray<FLevelSequenceCheckResult>& OutResults);
};
