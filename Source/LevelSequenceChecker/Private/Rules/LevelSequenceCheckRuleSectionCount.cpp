#include "Rules/LevelSequenceCheckRuleSectionCount.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LevelSequenceCheckRuleSectionCount"

ULevelSequenceCheckRuleSectionCount::ULevelSequenceCheckRuleSectionCount()
{
	bEnabled = true;
}

FName ULevelSequenceCheckRuleSectionCount::GetRuleId() const
{
	return FName("S-05");
}

FText ULevelSequenceCheckRuleSectionCount::GetRuleName() const
{
	return LOCTEXT("RuleName", "Section 数量超限");
}

FName ULevelSequenceCheckRuleSectionCount::GetCategory() const
{
	return FName("Structure");
}

void ULevelSequenceCheckRuleSectionCount::ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults)
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

	const int32 TotalSections = CountAllSections(MovieScene);

	if (TotalSections > MaxSectionCount)
	{
		FLevelSequenceCheckResult Result;
		Result.RuleId = GetRuleId();
		Result.RuleName = GetRuleName();
		Result.Description = FText::Format(
			LOCTEXT("SectionCountExceeded", "当前包含 {0} 个 Section，已超过配置的上限值 {1} 个。"),
			FText::AsNumber(TotalSections),
			FText::AsNumber(MaxSectionCount)
		);
		Result.Severity = ELevelSequenceCheckSeverity::Error;

		OutResults.Add(MoveTemp(Result));
	}
}

void ULevelSequenceCheckRuleSectionCount::CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder)
{
	CategoryBuilder.AddCustomRow(LOCTEXT("RuleHeader", "S-05"))
	.WholeRowContent()
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("RuleTitle", "S-05 Section 数量超限"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
	];

	TArray<UObject*> ExternalObjects;
	ExternalObjects.Add(this);

	CategoryBuilder.AddExternalObjectProperty(ExternalObjects, GET_MEMBER_NAME_CHECKED(ULevelSequenceCheckRuleBase, bEnabled));
	CategoryBuilder.AddExternalObjectProperty(ExternalObjects, GET_MEMBER_NAME_CHECKED(ULevelSequenceCheckRuleSectionCount, MaxSectionCount));
}

int32 ULevelSequenceCheckRuleSectionCount::CountAllSections(UMovieScene* InMovieScene) const
{
	if (!InMovieScene)
	{
		return 0;
	}

	int32 Count = 0;

	// Count sections from master tracks
	for (UMovieSceneTrack* Track : InMovieScene->GetTracks())
	{
		Count += Track->GetAllSections().Num();
	}

	// Count sections from all object bindings
	const UMovieScene* ConstMovieScene = InMovieScene;
	for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
	{
		for (UMovieSceneTrack* Track : Binding.GetTracks())
		{
			Count += Track->GetAllSections().Num();
		}
	}

	return Count;
}

#undef LOCTEXT_NAMESPACE
