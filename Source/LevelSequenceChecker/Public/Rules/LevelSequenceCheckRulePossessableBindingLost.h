#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceCheckRuleBase.h"
#include "LevelSequenceCheckRulePossessableBindingLost.generated.h"

class UMovieScene;

/**
 * B-03: Possessable Binding Lost Rule
 * Checks whether Possessable bindings cannot resolve to a valid target object at runtime.
 * Also checks Camera Cuts Track sections for dangling binding references.
 */
UCLASS(meta=(DisplayName="B-03 Possessable 绑定丢失"))
class LEVELSEQUENCECHECKER_API ULevelSequenceCheckRulePossessableBindingLost : public ULevelSequenceCheckRuleBase
{
	GENERATED_BODY()

public:
	ULevelSequenceCheckRulePossessableBindingLost();

	//~ Begin ULevelSequenceCheckRuleBase interface
	virtual FName GetRuleId() const override;
	virtual FText GetRuleName() const override;
	virtual FName GetCategory() const override;
	virtual void ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults) override;
	virtual void CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder) override;
	//~ End ULevelSequenceCheckRuleBase interface

private:
	/** Check Camera Cuts Track sections for binding references that no longer exist in the MovieScene */
	void CheckCameraCutsTrackBindingLost(ULevelSequence* InSequence, UMovieScene* MovieScene, TArray<FLevelSequenceCheckResult>& OutResults);
};
