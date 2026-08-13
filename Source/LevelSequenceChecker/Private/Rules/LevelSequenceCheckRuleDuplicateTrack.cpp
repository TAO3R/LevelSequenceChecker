#include "Rules/LevelSequenceCheckRuleDuplicateTrack.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LevelSequenceCheckRuleDuplicateTrack"

ULevelSequenceCheckRuleDuplicateTrack::ULevelSequenceCheckRuleDuplicateTrack()
{
	bEnabled = true;
}

FName ULevelSequenceCheckRuleDuplicateTrack::GetRuleId() const
{
	return FName("S-03");
}

FText ULevelSequenceCheckRuleDuplicateTrack::GetRuleName() const
{
	return LOCTEXT("RuleName", "重复轨道");
}

FName ULevelSequenceCheckRuleDuplicateTrack::GetCategory() const
{
	return FName("Structure");
}

void ULevelSequenceCheckRuleDuplicateTrack::ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults)
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

	// For each binding, group tracks by class and report duplicates
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

		// Group tracks by (class, property name). For PropertyTracks, tracks of the
		// same class but targeting different properties are NOT considered duplicates.
		using FTrackGroupKey = TPair<UClass*, FName>;
		TMap<FTrackGroupKey, TArray<UMovieSceneTrack*>> TracksByKey;
		for (UMovieSceneTrack* Track : Binding.GetTracks())
		{
			if (!Track)
			{
				continue;
			}

			UClass* TrackClass = Track->GetClass();
			FName GroupPropertyKey = NAME_None;

			UMovieScenePropertyTrack* PropTrack = Cast<UMovieScenePropertyTrack>(Track);
			if (PropTrack)
			{
				// Use GetPropertyName() (leaf name) via ToString->FName to eliminate
				// FName Number suffix differences caused by copy-paste uniquification.
				// GetPropertyName is more stable than GetPropertyPath for grouping.
				GroupPropertyKey = FName(*PropTrack->GetPropertyName().ToString());

				//UE_LOG(LogTemp, Log, TEXT("S-03 Debug: Binding='%s' TrackClass='%s' PropertyName='%s' PropertyPath='%s' DisplayName='%s' UniqueTrackName='%s'"),
			//	*BindingName.ToString(),
			//	*TrackClass->GetName(),
			//	*PropTrack->GetPropertyName().ToString(),
			//	*PropTrack->GetPropertyPath().ToString(),
			//	*Track->GetDisplayName().ToString(),
			//	*PropTrack->UniqueTrackName.ToString()
			//);
			}
			else
			{
				//UE_LOG(LogTemp, Log, TEXT("S-03 Debug: Binding='%s' TrackClass='%s' (Non-PropertyTrack) DisplayName='%s'"),
			//	*BindingName.ToString(),
			//	*TrackClass->GetName(),
			//	*Track->GetDisplayName().ToString()
			//);
			}

			TracksByKey.FindOrAdd(FTrackGroupKey(TrackClass, GroupPropertyKey)).Add(Track);
		}

		// Report groups with 2+ tracks of the same type and property name
		for (auto& Pair : TracksByKey)
		{
			if (Pair.Value.Num() >= 2)
			{
				UClass* TrackClass = Pair.Key.Key;
				FName GroupPropertyKey = Pair.Key.Value;
				FText TrackTypeName = TrackClass->GetDisplayNameText();

				FLevelSequenceCheckResult Result;
				Result.RuleId = GetRuleId();
				Result.RuleName = GetRuleName();
				Result.Description = FText::Format(
					LOCTEXT("DuplicateTrack", "同一绑定对象上存在 {0} 条相同类型的轨道。"),
					FText::AsNumber(Pair.Value.Num())
				);

				if (GroupPropertyKey != NAME_None)
				{
					Result.LocationInfo = FText::Format(
						LOCTEXT("DuplicateTrackLocWithProp", "对象: {0} -> 轨道类型: {1} -> 属性: {2}"),
						BindingName,
						TrackTypeName,
						FText::FromString(GroupPropertyKey.ToString())
					);
				}
				else
				{
					Result.LocationInfo = FText::Format(
						LOCTEXT("DuplicateTrackLoc", "对象: {0} -> 轨道类型: {1}"),
						BindingName,
						TrackTypeName
					);
				}

				Result.Severity = ELevelSequenceCheckSeverity::Error;
				OutResults.Add(MoveTemp(Result));
			}
		}
	}
}

void ULevelSequenceCheckRuleDuplicateTrack::CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder)
{
	CategoryBuilder.AddCustomRow(LOCTEXT("RuleHeader", "S-03"))
	.WholeRowContent()
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("RuleTitle", "S-03 重复轨道"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
	];

	TArray<UObject*> ExternalObjects;
	ExternalObjects.Add(this);

	CategoryBuilder.AddExternalObjectProperty(ExternalObjects, GET_MEMBER_NAME_CHECKED(ULevelSequenceCheckRuleBase, bEnabled));
}

#undef LOCTEXT_NAMESPACE
