#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceCheckRuleBase.h"
#include "LevelSequenceCheckRuleSpawnableCollision.generated.h"

/**
 * B-01: Spawnable Collision Not Disabled Rule
 * Checks whether Spawnable templates have SkeletalMeshComponent / StaticMeshComponent
 * whose CollisionProfile is not NoCollision.
 */
UCLASS(meta=(DisplayName="B-01 Spawnable 碰撞未关闭"))
class LEVELSEQUENCECHECKER_API ULevelSequenceCheckRuleSpawnableCollision : public ULevelSequenceCheckRuleBase
{
	GENERATED_BODY()

public:
	ULevelSequenceCheckRuleSpawnableCollision();

	//~ Begin ULevelSequenceCheckRuleBase interface
	virtual FName GetRuleId() const override;
	virtual FText GetRuleName() const override;
	virtual FName GetCategory() const override;
	virtual void ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults) override;
	virtual void CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder) override;
	//~ End ULevelSequenceCheckRuleBase interface
};
