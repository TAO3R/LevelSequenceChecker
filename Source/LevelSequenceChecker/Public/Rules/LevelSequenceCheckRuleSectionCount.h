#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceCheckRuleBase.h"
#include "LevelSequenceCheckRuleSectionCount.generated.h"

class UMovieScene;

/**
 * S-05: Section Count Limit Rule
 * Checks whether the total number of sections in a LevelSequence exceeds a configured threshold.
 */
UCLASS(meta=(DisplayName="S-05 Section 数量超限"))
class LEVELSEQUENCECHECKER_API ULevelSequenceCheckRuleSectionCount : public ULevelSequenceCheckRuleBase
{
	GENERATED_BODY()

public:
	ULevelSequenceCheckRuleSectionCount();

	/** Maximum allowed section count. If the total exceeds this value, the rule fires. */
	UPROPERTY(EditAnywhere, Category = "Rule", meta = (ClampMin = 1, UIMin = 1, DisplayName = "Section数量上限"))
	int32 MaxSectionCount = 100;

	//~ Begin ULevelSequenceCheckRuleBase interface
	virtual FName GetRuleId() const override;
	virtual FText GetRuleName() const override;
	virtual FName GetCategory() const override;
	virtual void ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults) override;
	virtual void CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder) override;
	//~ End ULevelSequenceCheckRuleBase interface

private:
	/** Count all sections in the MovieScene (master tracks + binding tracks) */
	int32 CountAllSections(UMovieScene* InMovieScene) const;
};
