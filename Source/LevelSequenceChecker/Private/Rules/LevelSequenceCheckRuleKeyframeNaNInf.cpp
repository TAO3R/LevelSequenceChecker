#include "Rules/LevelSequenceCheckRuleKeyframeNaNInf.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LevelSequenceCheckRuleKeyframeNaNInf"

ULevelSequenceCheckRuleKeyframeNaNInf::ULevelSequenceCheckRuleKeyframeNaNInf()
{
	bEnabled = true;
}

FName ULevelSequenceCheckRuleKeyframeNaNInf::GetRuleId() const
{
	return FName("K-01");
}

FText ULevelSequenceCheckRuleKeyframeNaNInf::GetRuleName() const
{
	return LOCTEXT("RuleName", "关键帧值 NaN/Inf");
}

FName ULevelSequenceCheckRuleKeyframeNaNInf::GetCategory() const
{
	return FName("Keyframe");
}

void ULevelSequenceCheckRuleKeyframeNaNInf::CheckFloatChannels(
	UMovieSceneSection* Section,
	const FText& LocationContext,
	TArray<FLevelSequenceCheckResult>& OutResults)
{
	const FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();

	TArrayView<FMovieSceneFloatChannel*> Channels = ChannelProxy.GetChannels<FMovieSceneFloatChannel>();

	const FString ChannelTypeName = FMovieSceneFloatChannel::StaticStruct()->GetName();

	for (int32 ChannelIdx = 0; ChannelIdx < Channels.Num(); ++ChannelIdx)
	{
		const FMovieSceneFloatChannel* Channel = Channels[ChannelIdx];
		if (!Channel || Channel->GetNumKeys() == 0)
		{
			continue;
		}

		FString ChannelName = FString::Printf(TEXT("%s[%d]"), *ChannelTypeName, ChannelIdx);

		auto ChannelData = Channel->GetData();
		TArrayView<const FFrameNumber> Times = ChannelData.GetTimes();
		TArrayView<const FMovieSceneFloatValue> Values = ChannelData.GetValues();

		for (int32 KeyIdx = 0; KeyIdx < Values.Num(); ++KeyIdx)
		{
			float Value = Values[KeyIdx].Value;
			bool bIsNaN = FMath::IsNaN(Value);
			bool bIsInf = !FMath::IsFinite(Value) && !bIsNaN;

			if (bIsNaN || bIsInf)
			{
				FString AnomalyType = bIsNaN ? TEXT("NaN") : TEXT("Inf");
				int32 FrameNumber = Times.IsValidIndex(KeyIdx) ? Times[KeyIdx].Value : 0;

				FLevelSequenceCheckResult Result;
				Result.RuleId = GetRuleId();
				Result.RuleName = GetRuleName();
				Result.Description = FText::Format(
					LOCTEXT("KeyframeAnomaly", "通道 '{0}' 的关键帧值为 {1}。"),
					FText::FromString(ChannelName),
					FText::FromString(AnomalyType)
				);
				Result.LocationInfo = FText::Format(
					LOCTEXT("KeyframeAnomalyLoc", "{0} -> 通道: {1} -> 帧: {2}"),
					LocationContext,
					FText::FromString(ChannelName),
					FText::AsNumber(FrameNumber)
				);
				Result.Severity = ELevelSequenceCheckSeverity::Error;
				OutResults.Add(MoveTemp(Result));
			}
		}
	}
}

void ULevelSequenceCheckRuleKeyframeNaNInf::CheckDoubleChannels(
	UMovieSceneSection* Section,
	const FText& LocationContext,
	TArray<FLevelSequenceCheckResult>& OutResults)
{
	const FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();

	TArrayView<FMovieSceneDoubleChannel*> Channels = ChannelProxy.GetChannels<FMovieSceneDoubleChannel>();

	const FString ChannelTypeName = FMovieSceneDoubleChannel::StaticStruct()->GetName();

	for (int32 ChannelIdx = 0; ChannelIdx < Channels.Num(); ++ChannelIdx)
	{
		const FMovieSceneDoubleChannel* Channel = Channels[ChannelIdx];
		if (!Channel || Channel->GetNumKeys() == 0)
		{
			continue;
		}

		FString ChannelName = FString::Printf(TEXT("%s[%d]"), *ChannelTypeName, ChannelIdx);

		auto ChannelData = Channel->GetData();
		TArrayView<const FFrameNumber> Times = ChannelData.GetTimes();
		TArrayView<const FMovieSceneDoubleValue> Values = ChannelData.GetValues();

		for (int32 KeyIdx = 0; KeyIdx < Values.Num(); ++KeyIdx)
		{
			double Value = Values[KeyIdx].Value;
			bool bIsNaN = FMath::IsNaN(Value);
			bool bIsInf = !FMath::IsFinite(Value) && !bIsNaN;

			if (bIsNaN || bIsInf)
			{
				FString AnomalyType = bIsNaN ? TEXT("NaN") : TEXT("Inf");
				int32 FrameNumber = Times.IsValidIndex(KeyIdx) ? Times[KeyIdx].Value : 0;

				FLevelSequenceCheckResult Result;
				Result.RuleId = GetRuleId();
				Result.RuleName = GetRuleName();
				Result.Description = FText::Format(
					LOCTEXT("KeyframeAnomalyDouble", "通道 '{0}' 的关键帧值为 {1}。"),
					FText::FromString(ChannelName),
					FText::FromString(AnomalyType)
				);
				Result.LocationInfo = FText::Format(
					LOCTEXT("KeyframeAnomalyLocDouble", "{0} -> 通道: {1} -> 帧: {2}"),
					LocationContext,
					FText::FromString(ChannelName),
					FText::AsNumber(FrameNumber)
				);
				Result.Severity = ELevelSequenceCheckSeverity::Error;
				OutResults.Add(MoveTemp(Result));
			}
		}
	}
}

void ULevelSequenceCheckRuleKeyframeNaNInf::ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults)
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
		FText TrackContext = FText::Format(
			LOCTEXT("GlobalTrackContext", "全局轨道 -> 轨道: {0}"),
			Track->GetDisplayName()
		);

		for (UMovieSceneSection* Section : Track->GetAllSections())
		{
			FText SectionContext = FText::Format(
				LOCTEXT("SectionContext", "{0} -> 段落: {1}"),
				TrackContext,
				FText::FromString(Section->GetName())
			);

			CheckFloatChannels(Section, SectionContext, OutResults);
			CheckDoubleChannels(Section, SectionContext, OutResults);
		}
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
			FText TrackContext = FText::Format(
				LOCTEXT("BindingTrackContext", "对象: {0} -> 轨道: {1}"),
				BindingName,
				Track->GetDisplayName()
			);

			for (UMovieSceneSection* Section : Track->GetAllSections())
			{
				FText SectionContext = FText::Format(
					LOCTEXT("SectionContextBinding", "{0} -> 段落: {1}"),
					TrackContext,
					FText::FromString(Section->GetName())
				);

				CheckFloatChannels(Section, SectionContext, OutResults);
				CheckDoubleChannels(Section, SectionContext, OutResults);
			}
		}
	}
}

void ULevelSequenceCheckRuleKeyframeNaNInf::CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder)
{
	CategoryBuilder.AddCustomRow(LOCTEXT("RuleHeader", "K-01"))
	.WholeRowContent()
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("RuleTitle", "K-01 关键帧值 NaN/Inf"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
	];

	TArray<UObject*> ExternalObjects;
	ExternalObjects.Add(this);

	CategoryBuilder.AddExternalObjectProperty(ExternalObjects, GET_MEMBER_NAME_CHECKED(ULevelSequenceCheckRuleBase, bEnabled));
}

#undef LOCTEXT_NAMESPACE
