#include "LevelSequenceCheckRuleBase.h"

#include "DetailCategoryBuilder.h"

ULevelSequenceCheckRuleBase::ULevelSequenceCheckRuleBase()
{
}

void ULevelSequenceCheckRuleBase::CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder)
{
	// Default: just show the bEnabled toggle.
	// Subclasses override this to add their own custom UI (thresholds, etc.)
}

#if WITH_EDITOR
void ULevelSequenceCheckRuleBase::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Propagate config changes to ini
	if (PropertyChangedEvent.Property)
	{
		SaveConfig();
	}
}
#endif
