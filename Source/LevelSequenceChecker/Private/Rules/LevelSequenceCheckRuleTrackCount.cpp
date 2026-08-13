#include "Rules/LevelSequenceCheckRuleTrackCount.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LevelSequenceCheckRuleTrackCount"

ULevelSequenceCheckRuleTrackCount::ULevelSequenceCheckRuleTrackCount()
{
	bEnabled = true;
}

FName ULevelSequenceCheckRuleTrackCount::GetRuleId() const
{
	return FName("S-04");
}

FText ULevelSequenceCheckRuleTrackCount::GetRuleName() const
{
	return LOCTEXT("RuleName", "Track 数量超限");
}

FName ULevelSequenceCheckRuleTrackCount::GetCategory() const
{
	return FName("Structure");
}

void ULevelSequenceCheckRuleTrackCount::ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults)
{
	if (!IsValid(InSequence))
	{
		return;
	}

	UMovieScene* MovieScene = InSequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}

	const int32 TotalTracks = CountAllTracks(MovieScene);

	if (TotalTracks > MaxTrackCount)
	{
		FLevelSequenceCheckResult Result;
		Result.RuleId = GetRuleId();
		Result.RuleName = GetRuleName();
		Result.Description = FText::Format(
			LOCTEXT("TrackCountExceeded", "当前包含 {0} 条轨道，已超过配置的上限值 {1} 条。"),
			FText::AsNumber(TotalTracks),
			FText::AsNumber(MaxTrackCount)
		);
		Result.Severity = ELevelSequenceCheckSeverity::Error;

		OutResults.Add(MoveTemp(Result));
	}
}

void ULevelSequenceCheckRuleTrackCount::CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder)
{
	// Add a header row with the rule name
	CategoryBuilder.AddCustomRow(LOCTEXT("RuleHeader", "S-04"))
	.WholeRowContent()
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("RuleTitle", "S-04 Track 数量超限"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
	];

	// Add properties individually using AddExternalObjectProperty.
	// This avoids the "Object" sub-header that AddExternalObjects creates
	// when CollapseCategories is active on the class.
	TArray<UObject*> ExternalObjects;
	ExternalObjects.Add(this);

	// bEnabled from base class
	CategoryBuilder.AddExternalObjectProperty(ExternalObjects, GET_MEMBER_NAME_CHECKED(ULevelSequenceCheckRuleBase, bEnabled));

	// MaxTrackCount
	CategoryBuilder.AddExternalObjectProperty(ExternalObjects, GET_MEMBER_NAME_CHECKED(ULevelSequenceCheckRuleTrackCount, MaxTrackCount));
}

int32 ULevelSequenceCheckRuleTrackCount::CountAllTracks(UMovieScene* InMovieScene) const
{
	if (!InMovieScene)
	{
		return 0;
	}

	int32 Count = 0;

	// Count master tracks (tracks not bound to any object)
	Count += InMovieScene->GetTracks().Num();

	// Count tracks from all object bindings (use const pointer to avoid deprecation warning)
	const UMovieScene* ConstMovieScene = InMovieScene;
	for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
	{
		Count += Binding.GetTracks().Num();
	}

	return Count;
}

#undef LOCTEXT_NAMESPACE
