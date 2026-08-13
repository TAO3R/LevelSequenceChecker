#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LevelSequenceCheckResult.h"
#include "LevelSequenceCheckRuleBase.generated.h"

class ULevelSequence;
class IDetailCategoryBuilder;

/**
 * Abstract base class for all Level Sequence check rules.
 * Each rule is self-contained: it owns its config data, check logic, and UI customization.
 * 
 * To add a new rule in Phase 2:
 *   1. Create a subclass of ULevelSequenceCheckRuleBase
 *   2. Implement all pure virtual methods
 *   3. Override CustomizeRuleDetails() if the rule needs custom UI (e.g. threshold sliders)
 *   4. Register the default instance in ULevelSequenceCheckerSettings::PostInitProperties
 * 
 * No framework code needs to change.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class LEVELSEQUENCECHECKER_API ULevelSequenceCheckRuleBase : public UObject
{
	GENERATED_BODY()

public:
	ULevelSequenceCheckRuleBase();

	/** 是否启用此规则。通过 PostEditChangeProperty 中的 SaveConfig() 持久化。 */
	UPROPERTY(EditAnywhere, Category = "Rule", meta=(DisplayName="启用"))
	bool bEnabled = true;

	/** Unique identifier for this rule (e.g. "S-01", "K-01") */
	virtual FName GetRuleId() const PURE_VIRTUAL(ULevelSequenceCheckRuleBase::GetRuleId, return NAME_None;);

	/** Human-readable rule name */
	virtual FText GetRuleName() const PURE_VIRTUAL(ULevelSequenceCheckRuleBase::GetRuleName, return FText::GetEmpty(););

	/**
	 * Category for grouping in the Settings panel.
	 * Should return one of: "AssetNorm" / "Structure" / "Binding" / "Keyframe"
	 * (corresponding to the 4 groups in the requirements doc §4)
	 */
	virtual FName GetCategory() const PURE_VIRTUAL(ULevelSequenceCheckRuleBase::GetCategory, return NAME_None;);

	/**
	 * Execute the check on the given LevelSequence.
	 * Append any problems found to OutResults.
	 * Implemented by concrete rule subclasses in Phase 2.
	 */
	virtual void ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults) PURE_VIRTUAL(ULevelSequenceCheckRuleBase::ExecuteCheck,);

	/**
	 * Let the rule customize its own UI within the given category builder.
	 * Default implementation adds bEnabled as a toggle.
	 * Subclasses can override to add threshold sliders, etc.
	 */
	virtual void CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder);

	//~ Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject interface
};
