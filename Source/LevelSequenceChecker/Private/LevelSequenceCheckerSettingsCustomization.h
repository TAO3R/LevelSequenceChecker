#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/**
 * Custom Detail panel layout for ULevelSequenceCheckerSettings.
 * 
 * Instead of showing the raw CheckRules array (which would be an unstyled list),
 * this customization:
 *   1. Hides the raw CheckRules property
 *   2. Iterates all rule instances
 *   3. Groups them by GetCategory()
 *   4. Calls CustomizeRuleDetails() on each rule so it can draw itself
 */
class FLevelSequenceCheckerSettingsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FLevelSequenceCheckerSettingsCustomization>();
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
