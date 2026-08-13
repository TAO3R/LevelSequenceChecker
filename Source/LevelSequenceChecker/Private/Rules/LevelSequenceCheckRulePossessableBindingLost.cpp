#include "Rules/LevelSequenceCheckRulePossessableBindingLost.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieScenePossessable.h"
#include "MovieSceneBindingReferences.h"
#include "Bindings/MovieSceneSpawnableBinding.h"
#include "UniversalObjectLocator.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LevelSequenceCheckRulePossessableBindingLost"

ULevelSequenceCheckRulePossessableBindingLost::ULevelSequenceCheckRulePossessableBindingLost()
{
	bEnabled = true;
}

FName ULevelSequenceCheckRulePossessableBindingLost::GetRuleId() const
{
	return FName("B-03");
}

FText ULevelSequenceCheckRulePossessableBindingLost::GetRuleName() const
{
	return LOCTEXT("RuleName", "Possessable 绑定丢失");
}

FName ULevelSequenceCheckRulePossessableBindingLost::GetCategory() const
{
	return FName("Binding");
}

void ULevelSequenceCheckRulePossessableBindingLost::ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults)
{
	//UE_LOG(LogTemp, Verbose, TEXT("[B-03] === ExecuteCheck START ==="));

	if (!IsValid(InSequence))
	{
		//UE_LOG(LogTemp, Warning, TEXT("[B-03] InSequence is INVALID, aborting"));
		return;
	}

	UMovieScene* MovieScene = InSequence->GetMovieScene();
	if (!MovieScene)
	{
		//UE_LOG(LogTemp, Warning, TEXT("[B-03] MovieScene is NULL, aborting"));
		return;
	}

	// Get BindingReferences for Locator access and Spawnable detection
	const FMovieSceneBindingReferences* BindingRefs = InSequence->GetBindingReferences();
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

	//UE_LOG(LogTemp, Verbose, TEXT("[B-03] BindingRefs=%s, EditorWorld=%s, PossessableCount=%d"),
	//	BindingRefs ? TEXT("Valid") : TEXT("NULL"),
	//	EditorWorld ? TEXT("Valid") : TEXT("NULL"),
	//	MovieScene->GetPossessableCount());

	// Collect Guids of Possessables whose parent binding is already lost (to avoid duplicate reports)
	TSet<FGuid> LostBindingGuids;

	for (int32 i = 0; i < MovieScene->GetPossessableCount(); ++i)
	{
		FMovieScenePossessable& Possessable = MovieScene->GetPossessable(i);
		FGuid BindingGuid = Possessable.GetGuid();
		FString PossessableName = Possessable.GetName();
		const UClass* PossessedClass = Possessable.GetPossessedObjectClass();
		FGuid ParentGuid = Possessable.GetParent();

		//UE_LOG(LogTemp, Verbose, TEXT("[B-03] Possessable[%d]: Name=%s, Guid=%s, Class=%s, ParentGuid=%s"),
		//	i,
		//	*PossessableName,
		//	*BindingGuid.ToString(),
		//	PossessedClass ? *PossessedClass->GetName() : TEXT("NULL"),
		//	*ParentGuid.ToString());

		// Skip Spawnables (in UE 5.7, they are stored as Possessable with CustomBinding)
		bool bIsSpawnable = false;
		if (BindingRefs)
		{
			const UMovieSceneCustomBinding* CustomBinding = BindingRefs->GetCustomBinding(BindingGuid, 0);
			if (Cast<UMovieSceneSpawnableBindingBase>(CustomBinding))
			{
				bIsSpawnable = true;
			}
		}
		if (!bIsSpawnable && MovieScene->FindSpawnable(BindingGuid))
		{
			bIsSpawnable = true;
		}
		if (bIsSpawnable)
		{
			//UE_LOG(LogTemp, Verbose, TEXT("[B-03]   Skipped: is Spawnable"));
			continue;
		}

		// Skip child components of Spawnables (they don't exist in EditorWorld at design time)
		if (BindingRefs && ParentGuid.IsValid())
		{
			const UMovieSceneCustomBinding* ParentCustomBinding = BindingRefs->GetCustomBinding(ParentGuid, 0);
			if (Cast<UMovieSceneSpawnableBindingBase>(ParentCustomBinding))
			{
				//UE_LOG(LogTemp, Verbose, TEXT("[B-03]   Skipped: parent is Spawnable"));
				continue;
			}
			if (MovieScene->FindSpawnable(ParentGuid))
			{
				//UE_LOG(LogTemp, Verbose, TEXT("[B-03]   Skipped: parent is Spawnable (legacy)"));
				continue;
			}
		}

		bool bBindingLost = false;
		FString DetailInfo;

		// Check 1: PossessedObjectClass is null (rare but possible for corrupted data)
		if (!PossessedClass)
		{
			bBindingLost = true;
			DetailInfo = TEXT("绑定对象类为空");
			//UE_LOG(LogTemp, Warning, TEXT("[B-03]   Check1 HIT: Possessable='%s' class is NULL"), *PossessableName);
		}

		// Check 2: ParentGuid references a binding that does not exist
		if (!bBindingLost && ParentGuid.IsValid())
		{
			bool bParentBindingExists = false;

			// UE 5.7: Check via BindingRefs
			if (BindingRefs && BindingRefs->HasBinding(ParentGuid))
			{
				bParentBindingExists = true;
			}

			// Legacy: Check via FindPossessable/FindSpawnable
			if (!bParentBindingExists)
			{
				FMovieScenePossessable* ParentPossessable = MovieScene->FindPossessable(ParentGuid);
				FMovieSceneSpawnable* ParentSpawnable = MovieScene->FindSpawnable(ParentGuid);
				bParentBindingExists = (ParentPossessable != nullptr || ParentSpawnable != nullptr);
			}

			if (!bParentBindingExists)
			{
				bBindingLost = true;
				DetailInfo = FString::Printf(TEXT("父级绑定丢失 (Guid: %s)"), *ParentGuid.ToString());
				//UE_LOG(LogTemp, Warning, TEXT("[B-03]   Check2 HIT: Possessable='%s' parent not found"), *PossessableName);
			}
			else if (LostBindingGuids.Contains(ParentGuid))
			{
				bBindingLost = true;
				DetailInfo = FString::Printf(TEXT("父级绑定已丢失 (Guid: %s)"), *ParentGuid.ToString());
				//UE_LOG(LogTemp, Warning, TEXT("[B-03]   Check2 HIT: Possessable='%s' parent already lost"), *PossessableName);
			}
		}

		// Check 3: Locator cannot resolve to a valid world object
		if (!bBindingLost && BindingRefs && EditorWorld)
		{
			if (BindingRefs->HasBinding(BindingGuid))
			{
				const FMovieSceneBindingReference* Ref = BindingRefs->GetReference(BindingGuid, 0);
				if (Ref)
				{
					if (Ref->Locator.IsEmpty())
					{
						bBindingLost = true;
						DetailInfo = TEXT("绑定引用路径为空");
						//UE_LOG(LogTemp, Warning, TEXT("[B-03]   Check3 HIT: Possessable='%s' locator is empty"), *PossessableName);
					}
					else
					{
						UObject* ResolvedObj = Ref->Locator.SyncFind(EditorWorld);

						// If SyncFind failed and this is a child component with a valid parent,
						// try resolving through the parent binding. Child component Locators
						// may use relative paths that can't resolve from World root context.
						if (!ResolvedObj && ParentGuid.IsValid())
						{
							if (BindingRefs->HasBinding(ParentGuid))
							{
								const FMovieSceneBindingReference* ParentRef = BindingRefs->GetReference(ParentGuid, 0);
								if (ParentRef && !ParentRef->Locator.IsEmpty())
								{
									UObject* ParentObj = ParentRef->Locator.SyncFind(EditorWorld);
									if (AActor* ParentActor = Cast<AActor>(ParentObj))
									{
										// Search for child component on parent actor by class and name
										if (PossessedClass && PossessedClass->IsChildOf<UActorComponent>())
										{
											TArray<UActorComponent*> Components;
											ParentActor->GetComponents(Components);
											for (UActorComponent* Comp : Components)
											{
												if (Comp->IsA(PossessedClass) && Comp->GetName() == PossessableName)
												{
													ResolvedObj = Comp;
													break;
												}
											}
										}
									}
								}
							}
						}

						if (!ResolvedObj)
						{
							bBindingLost = true;
							DetailInfo = TEXT("绑定的目标对象在关卡中不存在（可能已被删除）");
							//UE_LOG(LogTemp, Warning, TEXT("[B-03]   Check3 HIT: Possessable='%s' SyncFind returned NULL"), *PossessableName);
						}
						else
						{
							// Validate that the resolved object is truly alive in the level.
							// SyncFind may return an object still in memory due to UE's Undo/Transaction system,
							// even though it has been removed from the level.
							bool bTrulyAlive = IsValid(ResolvedObj);
							if (bTrulyAlive)
							{
								if (AActor* ResolvedActor = Cast<AActor>(ResolvedObj))
								{
									ULevel* ActorLevel = ResolvedActor->GetLevel();
									bTrulyAlive = ActorLevel && ActorLevel->Actors.Contains(ResolvedActor);
								}
								else if (UActorComponent* ResolvedComp = Cast<UActorComponent>(ResolvedObj))
								{
									AActor* Owner = ResolvedComp->GetOwner();
									if (Owner)
									{
										ULevel* OwnerLevel = Owner->GetLevel();
										bTrulyAlive = OwnerLevel && OwnerLevel->Actors.Contains(Owner);
									}
									else
									{
										bTrulyAlive = false;
									}
								}
							}

							if (!bTrulyAlive)
							{
								bBindingLost = true;
								DetailInfo = TEXT("绑定的目标对象已从关卡中移除（Undo 系统仍保留引用）");
								//UE_LOG(LogTemp, Warning, TEXT("[B-03]   Check3 HIT: Possessable='%s' resolved but not in Level::Actors"), *PossessableName);
							}
						}
					}
				}
			}
		}

		//UE_LOG(LogTemp, Verbose, TEXT("[B-03]   Result: %s %s"),
		//	bBindingLost ? TEXT("LOST") : TEXT("OK"),
		//	bBindingLost ? *DetailInfo : TEXT(""));

		if (bBindingLost)
		{
			LostBindingGuids.Add(BindingGuid);

			FString GuidStr = BindingGuid.ToString();

			FLevelSequenceCheckResult Result;
			Result.RuleId = GetRuleId();
			Result.RuleName = GetRuleName();
			Result.Description = FText::Format(
				LOCTEXT("BindingLost", "Possessable 绑定丢失: {0}"),
				FText::FromString(DetailInfo)
			);
			Result.LocationInfo = FText::Format(
				LOCTEXT("BindingLostLoc", "Possessable: {0} (Guid: {1})"),
				FText::FromString(PossessableName),
				FText::FromString(GuidStr)
			);
			Result.Severity = ELevelSequenceCheckSeverity::Error;
			OutResults.Add(MoveTemp(Result));
		}
	}

	//UE_LOG(LogTemp, Verbose, TEXT("[B-03] === ExecuteCheck END, results=%d ==="), OutResults.Num());

	// Check Camera Cuts Track for dangling binding references.
	// When a Possessable (e.g. Cine Camera Actor) is removed from the sequence,
	// the Camera Cuts Track sections may still reference its binding GUID.
	CheckCameraCutsTrackBindingLost(InSequence, MovieScene, OutResults);
}

void ULevelSequenceCheckRulePossessableBindingLost::CheckCameraCutsTrackBindingLost(
	ULevelSequence* InSequence, UMovieScene* MovieScene, TArray<FLevelSequenceCheckResult>& OutResults)
{
	// Collect all valid binding GUIDs in the MovieScene
	const UMovieScene* ConstMovieScene = MovieScene;
	TSet<FGuid> ValidBindingGuids;
	for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
	{
		ValidBindingGuids.Add(Binding.GetObjectGuid());
	}

	// In UE 5.7, CameraCutTrack is stored as a separate member (not in the Tracks array),
	// so GetTracks() will never return it. Use GetCameraCutTrack() instead.
	// Note: GetCameraCutTrack() returns UMovieSceneTrack*, need to Cast.
	UMovieSceneCameraCutTrack* CameraCutTrack = Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack());
	if (CameraCutTrack)
	{
		for (UMovieSceneSection* Section : CameraCutTrack->GetAllSections())
		{
			UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(Section);
			if (!CameraCutSection)
			{
				continue;
			}

			const FMovieSceneObjectBindingID& BindingID = CameraCutSection->GetCameraBindingID();
			FGuid CameraGuid = BindingID.GetGuid();

			// Skip invalid or empty bindings (no camera assigned at all)
			if (!CameraGuid.IsValid())
			{
				continue;
			}

			// Check if the binding GUID still exists in the MovieScene
			if (!ValidBindingGuids.Contains(CameraGuid))
			{
				// Try to find the old binding name for a more helpful error message
				FString BindingName;
				FMovieScenePossessable* Possessable = MovieScene->FindPossessable(CameraGuid);
				if (Possessable)
				{
					BindingName = Possessable->GetName();
				}
				else
				{
					FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(CameraGuid);
					if (Spawnable)
					{
						BindingName = Spawnable->GetName();
					}
					else
					{
						BindingName = CameraGuid.ToString();
					}
				}

				FLevelSequenceCheckResult Result;
				Result.RuleId = GetRuleId();
				Result.RuleName = GetRuleName();
				Result.Description = FText::Format(
					LOCTEXT("CameraCutBindingLost", "Camera Cuts Track 中引用的绑定已丢失: {0}"),
					FText::FromString(BindingName)
				);

				FFrameNumber StartFrame = Section->HasStartFrame() ? Section->GetInclusiveStartFrame() : 0;
				FFrameNumber EndFrame = Section->HasEndFrame() ? Section->GetExclusiveEndFrame() : 0;
				Result.LocationInfo = FText::Format(
					LOCTEXT("CameraCutBindingLostLoc", "Camera Cuts Track -> 段落帧范围: [{0} - {1}] (Guid: {2})"),
					FText::FromString(FString::FromInt(StartFrame.Value)),
					FText::FromString(FString::FromInt(EndFrame.Value)),
					FText::FromString(CameraGuid.ToString())
				);
				Result.Severity = ELevelSequenceCheckSeverity::Error;
				OutResults.Add(MoveTemp(Result));
			}
		}
	}
}

void ULevelSequenceCheckRulePossessableBindingLost::CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder)
{
	CategoryBuilder.AddCustomRow(LOCTEXT("RuleHeader", "B-03"))
	.WholeRowContent()
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("RuleTitle", "B-03 Possessable 绑定丢失"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
	];

	TArray<UObject*> ExternalObjects;
	ExternalObjects.Add(this);

	CategoryBuilder.AddExternalObjectProperty(ExternalObjects, GET_MEMBER_NAME_CHECKED(ULevelSequenceCheckRuleBase, bEnabled));
}

#undef LOCTEXT_NAMESPACE
