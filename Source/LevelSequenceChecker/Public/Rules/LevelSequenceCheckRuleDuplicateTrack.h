#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceCheckRuleBase.h"
#include "LevelSequenceCheckRuleDuplicateTrack.generated.h"

/**
 * S-03: Duplicate Track Rule
 * Checks whether the same binding object has multiple tracks of the same type.
 */
UCLASS(meta=(DisplayName="S-03 重复轨道"))
class LEVELSEQUENCECHECKER_API ULevelSequenceCheckRuleDuplicateTrack : public ULevelSequenceCheckRuleBase
{
	GENERATED_BODY()

public:
	ULevelSequenceCheckRuleDuplicateTrack();

	//~ Begin ULevelSequenceCheckRuleBase interface
	virtual FName GetRuleId() const override;
	virtual FText GetRuleName() const override;
	virtual FName GetCategory() const override;
	virtual void ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults) override;
	virtual void CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder) override;
	//~ End ULevelSequenceCheckRuleBase interface
};
