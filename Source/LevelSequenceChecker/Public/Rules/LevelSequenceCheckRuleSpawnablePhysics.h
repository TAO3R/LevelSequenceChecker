#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceCheckRuleBase.h"
#include "LevelSequenceCheckRuleSpawnablePhysics.generated.h"

/**
 * B-02: Spawnable Simulate Physics Enabled Rule
 * Checks whether any component in a Spawnable template has Simulate Physics enabled.
 */
UCLASS(meta=(DisplayName="B-02 Spawnable 物理模拟开启"))
class LEVELSEQUENCECHECKER_API ULevelSequenceCheckRuleSpawnablePhysics : public ULevelSequenceCheckRuleBase
{
	GENERATED_BODY()

public:
	ULevelSequenceCheckRuleSpawnablePhysics();

	//~ Begin ULevelSequenceCheckRuleBase interface
	virtual FName GetRuleId() const override;
	virtual FText GetRuleName() const override;
	virtual FName GetCategory() const override;
	virtual void ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults) override;
	virtual void CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder) override;
	//~ End ULevelSequenceCheckRuleBase interface
};
