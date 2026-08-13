#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LevelSequenceCheckRuleBase.h"
#include "LevelSequenceCheckerSettings.generated.h"

/**
 * Project-wide settings for the LevelSequence Checker tool.
 * Accessible via Project Settings > Plugins > Level Sequence Checker.
 *
 * Design: This class holds NO hardcoded rule properties.
 * All rules live in the Instanced TArray<ULevelSequenceCheckRuleBase*>.
 * The DetailCustomization renders each rule's UI by calling CustomizeRuleDetails().
 */
UCLASS(Config=Editor, DefaultConfig, meta=(DisplayName="Level Sequence 资产检查工具"))
class LEVELSEQUENCECHECKER_API ULevelSequenceCheckerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	ULevelSequenceCheckerSettings();

	//~ Begin UDeveloperSettings interface
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
	//~ End UDeveloperSettings interface

	/** 启用保存时检查 */
	UPROPERTY(Config, EditAnywhere, Category = "General", meta=(ConsoleVariable="LevelSequenceChecker.EnableCheckOnSave", DisplayName="启用保存时检查"))
	bool bEnableCheckOnSave = true;

	/**
	 * All check rule instances. Populated with default instances in PostInitProperties.
	 * Rendered dynamically by FLevelSequenceCheckerSettingsCustomization —
	 * each rule draws itself inside its category via CustomizeRuleDetails().
	 *
	 * Note: Instanced UObject arrays cannot use the 'config' specifier.
	 * Each rule instance handles its own config persistence via SaveConfig() in PostEditChangeProperty.
	 */
	UPROPERTY(EditAnywhere, Instanced, Category = "Rules")
	TArray<TObjectPtr<ULevelSequenceCheckRuleBase>> CheckRules;

	/** Get a rule instance by its RuleId, or nullptr if not found */
	ULevelSequenceCheckRuleBase* FindRule(FName InRuleId) const;

	//~ Begin UObject interface
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject interface

private:
	/** Populate CheckRules with default instances (called once when CDO is first created) */
	void InitDefaultRules();

	/** Tracks whether default rules have been initialized to avoid re-adding on CDO re-init */
	bool bDefaultRulesInitialized = false;
};
