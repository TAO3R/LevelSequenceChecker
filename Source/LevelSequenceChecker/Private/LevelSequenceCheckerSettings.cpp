#include "LevelSequenceCheckerSettings.h"

#include "Rules/LevelSequenceCheckRuleMissingReference.h"
#include "Rules/LevelSequenceCheckRuleTrackCount.h"
#include "Rules/LevelSequenceCheckRuleEmptyTrack.h"
#include "Rules/LevelSequenceCheckRuleEmptySection.h"
#include "Rules/LevelSequenceCheckRuleDuplicateTrack.h"
#include "Rules/LevelSequenceCheckRuleSectionCount.h"
#include "Rules/LevelSequenceCheckRuleSpawnableCollision.h"
#include "Rules/LevelSequenceCheckRuleSpawnablePhysics.h"
#include "Rules/LevelSequenceCheckRulePossessableBindingLost.h"
#include "Rules/LevelSequenceCheckRuleKeyframeNaNInf.h"

#define LOCTEXT_NAMESPACE "LevelSequenceCheckerSettings"

ULevelSequenceCheckerSettings::ULevelSequenceCheckerSettings()
{
}

FName ULevelSequenceCheckerSettings::GetContainerName() const
{
	return TEXT("Project");
}

FName ULevelSequenceCheckerSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FText ULevelSequenceCheckerSettings::GetSectionText() const
{
	return LOCTEXT("SectionText", "Level Sequence 资产检查工具");
}

FText ULevelSequenceCheckerSettings::GetSectionDescription() const
{
	return LOCTEXT("SectionDescription", "配置 Level Sequence 资产在保存时自动执行的检查规则。");
}

ULevelSequenceCheckRuleBase* ULevelSequenceCheckerSettings::FindRule(FName InRuleId) const
{
	for (const TObjectPtr<ULevelSequenceCheckRuleBase>& Rule : CheckRules)
	{
		if (IsValid(Rule) && Rule->GetRuleId() == InRuleId)
		{
			return Rule;
		}
	}
	return nullptr;
}

void ULevelSequenceCheckerSettings::PostInitProperties()
{
	Super::PostInitProperties();

	if (!bDefaultRulesInitialized)
	{
		InitDefaultRules();
		bDefaultRulesInitialized = true;
	}
}

void ULevelSequenceCheckerSettings::InitDefaultRules()
{
	// Register all default rule instances.
	// IMPORTANT: This is the ONLY place that needs updating when adding a new rule.
	// The Settings class, DetailCustomization, and Interceptor remain untouched.
	
	// R-01: Missing Asset Reference
	if (!FindRule(FName("R-01")))
	{
		CheckRules.Add(NewObject<ULevelSequenceCheckRuleMissingReference>(this));
	}

	// S-01: Empty Track
	if (!FindRule(FName("S-01")))
	{
		CheckRules.Add(NewObject<ULevelSequenceCheckRuleEmptyTrack>(this));
	}

	// S-02: Empty Section
	if (!FindRule(FName("S-02")))
	{
		CheckRules.Add(NewObject<ULevelSequenceCheckRuleEmptySection>(this));
	}

	// S-03: Duplicate Track
	if (!FindRule(FName("S-03")))
	{
		CheckRules.Add(NewObject<ULevelSequenceCheckRuleDuplicateTrack>(this));
	}

	// S-04: Track Count Limit
	if (!FindRule(FName("S-04")))
	{
		CheckRules.Add(NewObject<ULevelSequenceCheckRuleTrackCount>(this));
	}

	// S-05: Section Count Limit
	if (!FindRule(FName("S-05")))
	{
		CheckRules.Add(NewObject<ULevelSequenceCheckRuleSectionCount>(this));
	}

	// B-01: Spawnable Collision Not Disabled
	if (!FindRule(FName("B-01")))
	{
		CheckRules.Add(NewObject<ULevelSequenceCheckRuleSpawnableCollision>(this));
	}

	// B-02: Spawnable Simulate Physics Enabled
	if (!FindRule(FName("B-02")))
	{
		CheckRules.Add(NewObject<ULevelSequenceCheckRuleSpawnablePhysics>(this));
	}

	// B-03: Possessable Binding Lost
	if (!FindRule(FName("B-03")))
	{
		CheckRules.Add(NewObject<ULevelSequenceCheckRulePossessableBindingLost>(this));
	}

	// K-01: Keyframe NaN/Inf
	if (!FindRule(FName("K-01")))
	{
		CheckRules.Add(NewObject<ULevelSequenceCheckRuleKeyframeNaNInf>(this));
	}
}

#if WITH_EDITOR
void ULevelSequenceCheckerSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		SaveConfig();
	}
}
#endif

#undef LOCTEXT_NAMESPACE
