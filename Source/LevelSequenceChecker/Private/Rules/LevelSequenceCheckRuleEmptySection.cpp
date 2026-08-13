#include "Rules/LevelSequenceCheckRuleEmptySection.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneChannel.h"
#include "Sections/MovieSceneSubSection.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LevelSequenceCheckRuleEmptySection"

ULevelSequenceCheckRuleEmptySection::ULevelSequenceCheckRuleEmptySection()
{
	bEnabled = true;
}

FName ULevelSequenceCheckRuleEmptySection::GetRuleId() const
{
	return FName("S-02");
}

FText ULevelSequenceCheckRuleEmptySection::GetRuleName() const
{
	return LOCTEXT("RuleName", "空 Section");
}

FName ULevelSequenceCheckRuleEmptySection::GetCategory() const
{
	return FName("Structure");
}

bool ULevelSequenceCheckRuleEmptySection::IsSectionEmpty(UMovieSceneSection* Section) const
{
	if (!Section)
	{
		return true;
	}

	// SubSection 引用子序列资产，本身不含传统 keyframe Channel，不应被视为空
	if (UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section))
	{
		return (SubSection->GetSequence() == nullptr);
	}

	// Audio Section 引用音频资产，即使没有额外 keyframe 也不应视为空
	if (UMovieSceneAudioSection* AudioSection = Cast<UMovieSceneAudioSection>(Section))
	{
		return (AudioSection->GetSound() == nullptr);
	}

	// SkeletalAnimation Section 引用动画资产，即使没有额外 keyframe 也不应视为空
	if (UMovieSceneSkeletalAnimationSection* AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(Section))
	{
		return (AnimSection->Params.Animation == nullptr);
	}

	const FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();
	int32 TotalKeys = 0;

	for (const FMovieSceneChannelEntry& Entry : ChannelProxy.GetAllEntries())
	{
		for (FMovieSceneChannel* Channel : Entry.GetChannels())
		{
			if (Channel)
			{
				TotalKeys += Channel->GetNumKeys();
			}
		}
	}

	return (TotalKeys == 0);
}

bool ULevelSequenceCheckRuleEmptySection::IsDegenerateSection(UMovieSceneSection* Section) const
{
	return Section && Section->HasStartFrame() && Section->HasEndFrame() &&
		Section->GetInclusiveStartFrame() >= Section->GetExclusiveEndFrame();
}

void ULevelSequenceCheckRuleEmptySection::CheckTrackSections(
	UMovieSceneTrack* Track, const FText& TrackLocationPrefix, TArray<FLevelSequenceCheckResult>& OutResults)
{
	if (!Track)
	{
		return;
	}

	const TArray<UMovieSceneSection*>& Sections = Track->GetAllSections();
	if (Sections.Num() == 0)
	{
		return;
	}

	// 按 RowIndex 分组收集非 degenerate 的 Section
	TMap<int32, TArray<UMovieSceneSection*>> RowToSections;
	for (UMovieSceneSection* Section : Sections)
	{
		if (!Section || IsDegenerateSection(Section))
		{
			continue;
		}
		const int32 RowIndex = Section->GetRowIndex();
		RowToSections.FindOrAdd(RowIndex).Add(Section);
	}

	// 对每个 Row 判定：只要有一个非空 Section，整行跳过；否则报一次 Error
	for (const auto& Pair : RowToSections)
	{
		const int32 RowIndex = Pair.Key;
		const TArray<UMovieSceneSection*>& RowSections = Pair.Value;

		bool bRowHasKeys = false;
		for (UMovieSceneSection* Section : RowSections)
		{
			if (!IsSectionEmpty(Section))
			{
				bRowHasKeys = true;
				break;
			}
		}

		if (bRowHasKeys)
		{
			continue;
		}

		// 整行所有 Section 都为空，报一次 Error
		FLevelSequenceCheckResult Result;
		Result.RuleId = GetRuleId();
		Result.RuleName = GetRuleName();
		Result.Description = LOCTEXT("EmptySectionError", "段落不包含任何关键帧，内容为空。");
		Result.Severity = ELevelSequenceCheckSeverity::Error;

		Result.LocationInfo = FText::Format(
			LOCTEXT("EmptySectionTrackLoc", "{0} -> 轨道: {1} -> Row {2}"),
			TrackLocationPrefix,
			Track->GetDisplayName(),
			FText::FromString(FString::FromInt(RowIndex))
		);
		OutResults.Add(MoveTemp(Result));
	}
}

void ULevelSequenceCheckRuleEmptySection::ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults)
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

	const UMovieScene* ConstMovieScene = MovieScene;

	// Check master tracks
	for (UMovieSceneTrack* Track : MovieScene->GetTracks())
	{
		CheckTrackSections(Track, LOCTEXT("GlobalTrack", "全局轨道"), OutResults);
	}

	// Check binding tracks
	for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
	{
		FText BindingName;
		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Binding.GetObjectGuid());
		if (Possessable)
		{
			BindingName = FText::FromString(Possessable->GetName());
		}
		else
		{
			FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(Binding.GetObjectGuid());
			BindingName = Spawnable
				? FText::FromString(Spawnable->GetName())
				: FText::FromString(Binding.GetObjectGuid().ToString());
		}

		for (UMovieSceneTrack* Track : Binding.GetTracks())
		{
			CheckTrackSections(Track, FText::Format(
				LOCTEXT("BindingTrack", "对象: {0}"), BindingName), OutResults);
		}
	}
}

void ULevelSequenceCheckRuleEmptySection::CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder)
{
	CategoryBuilder.AddCustomRow(LOCTEXT("RuleHeader", "S-02"))
	.WholeRowContent()
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("RuleTitle", "S-02 空 Section"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
	];

	TArray<UObject*> ExternalObjects;
	ExternalObjects.Add(this);

	CategoryBuilder.AddExternalObjectProperty(ExternalObjects, GET_MEMBER_NAME_CHECKED(ULevelSequenceCheckRuleBase, bEnabled));
}

#undef LOCTEXT_NAMESPACE
