#include "Rules/LevelSequenceCheckRuleEmptyTrack.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LevelSequenceCheckRuleEmptyTrack"

ULevelSequenceCheckRuleEmptyTrack::ULevelSequenceCheckRuleEmptyTrack()
{
	bEnabled = true;
}

FName ULevelSequenceCheckRuleEmptyTrack::GetRuleId() const
{
	return FName("S-01");
}

FText ULevelSequenceCheckRuleEmptyTrack::GetRuleName() const
{
	return LOCTEXT("RuleName", "空 Track");
}

FName ULevelSequenceCheckRuleEmptyTrack::GetCategory() const
{
	return FName("Structure");
}

void ULevelSequenceCheckRuleEmptyTrack::ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults)
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

	// Camera Cuts Track is stored separately from the regular master tracks in UE 5.7.
	// A Camera Cuts Track with no camera binding cannot drive the viewport camera and should be treated as an empty/invalid track.
	if (UMovieSceneCameraCutTrack* CameraCutTrack = Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack()))
	{
		const TArray<UMovieSceneSection*>& CameraCutSections = CameraCutTrack->GetAllSections();
		if (CameraCutSections.Num() == 0)
		{
			FLevelSequenceCheckResult Result;
			Result.RuleId = GetRuleId();
			Result.RuleName = GetRuleName();
			Result.Description = LOCTEXT("EmptyCameraCutsTrack", "Camera Cuts Track 不包含任何 Section，因此没有绑定 Cine Camera Actor。");
			Result.LocationInfo = LOCTEXT("CameraCutsTrack", "Camera Cuts Track");
			Result.Severity = ELevelSequenceCheckSeverity::Error;
			OutResults.Add(MoveTemp(Result));
		}

		for (UMovieSceneSection* Section : CameraCutSections)
		{
			UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(Section);
			if (!CameraCutSection)
			{
				continue;
			}

			const FMovieSceneObjectBindingID& CameraBindingID = CameraCutSection->GetCameraBindingID();
			if (!CameraBindingID.IsValid())
			{
				FFrameNumber StartFrame = Section->HasStartFrame() ? Section->GetInclusiveStartFrame() : 0;
				FFrameNumber EndFrame = Section->HasEndFrame() ? Section->GetExclusiveEndFrame() : 0;

				FLevelSequenceCheckResult Result;
				Result.RuleId = GetRuleId();
				Result.RuleName = GetRuleName();
				Result.Description = LOCTEXT("CameraCutNoCameraBinding", "Camera Cuts Track 的段落未绑定 Cine Camera Actor。");
				Result.LocationInfo = FText::Format(
					LOCTEXT("CameraCutNoCameraBindingLoc", "Camera Cuts Track -> 段落帧范围: [{0} - {1}]"),
					FText::FromString(FString::FromInt(StartFrame.Value)),
					FText::FromString(FString::FromInt(EndFrame.Value))
				);
				Result.Severity = ELevelSequenceCheckSeverity::Error;
				OutResults.Add(MoveTemp(Result));
			}
		}
	}

	// Check master tracks (global tracks)
	for (UMovieSceneTrack* Track : MovieScene->GetTracks())
	{
		if (Track && Track->GetAllSections().Num() == 0)
		{
			FLevelSequenceCheckResult Result;
			Result.RuleId = GetRuleId();
			Result.RuleName = GetRuleName();
			Result.Description = FText::Format(
				LOCTEXT("EmptyMasterTrack", "轨道 '{0}' 不包含任何 Section。"),
				Track->GetDisplayName()
			);
			Result.LocationInfo = LOCTEXT("GlobalTrack", "全局轨道");
			Result.Severity = ELevelSequenceCheckSeverity::Error;
			OutResults.Add(MoveTemp(Result));
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
			if (Track && Track->GetAllSections().Num() == 0)
			{
				FLevelSequenceCheckResult Result;
				Result.RuleId = GetRuleId();
				Result.RuleName = GetRuleName();
				Result.Description = FText::Format(
					LOCTEXT("EmptyBindingTrack", "轨道 '{0}' 不包含任何 Section。"),
					Track->GetDisplayName()
				);
				Result.LocationInfo = FText::Format(
					LOCTEXT("BindingTrackLoc", "对象: {0}"),
					BindingName
				);
				Result.Severity = ELevelSequenceCheckSeverity::Error;
				OutResults.Add(MoveTemp(Result));
			}
		}
	}
}

void ULevelSequenceCheckRuleEmptyTrack::CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder)
{
	CategoryBuilder.AddCustomRow(LOCTEXT("RuleHeader", "S-01"))
	.WholeRowContent()
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("RuleTitle", "S-01 空 Track"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
	];

	TArray<UObject*> ExternalObjects;
	ExternalObjects.Add(this);

	CategoryBuilder.AddExternalObjectProperty(ExternalObjects, GET_MEMBER_NAME_CHECKED(ULevelSequenceCheckRuleBase, bEnabled));
}

#undef LOCTEXT_NAMESPACE
