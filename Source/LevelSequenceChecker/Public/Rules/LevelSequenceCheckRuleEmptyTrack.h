#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceCheckRuleBase.h"
#include "LevelSequenceCheckRuleEmptyTrack.generated.h"

/**
 * S-01: Empty Track Rule
 * Checks whether a LevelSequence contains tracks that have no sections.
 */
UCLASS(meta=(DisplayName="S-01 空 Track"))
class LEVELSEQUENCECHECKER_API ULevelSequenceCheckRuleEmptyTrack : public ULevelSequenceCheckRuleBase
{
	GENERATED_BODY()

public:
	ULevelSequenceCheckRuleEmptyTrack();

	//~ Begin ULevelSequenceCheckRuleBase interface
	virtual FName GetRuleId() const override;
	virtual FText GetRuleName() const override;
	virtual FName GetCategory() const override;
	virtual void ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults) override;
	virtual void CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder) override;
	//~ End ULevelSequenceCheckRuleBase interface
};
