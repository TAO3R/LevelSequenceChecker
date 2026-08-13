#include "Rules/LevelSequenceCheckRuleSpawnableCollision.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneBindingReferences.h"
#include "Bindings/MovieSceneSpawnableBinding.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Editor.h"
#include "UniversalObjectLocatorResolveParams.h"
#include "ISequencer.h"
#include "LevelSequenceCheckerModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LevelSequenceCheckRuleSpawnableCollision"

ULevelSequenceCheckRuleSpawnableCollision::ULevelSequenceCheckRuleSpawnableCollision()
{
	bEnabled = true;
}

FName ULevelSequenceCheckRuleSpawnableCollision::GetRuleId() const
{
	return FName("B-01");
}

FText ULevelSequenceCheckRuleSpawnableCollision::GetRuleName() const
{
	return LOCTEXT("RuleName", "Spawnable 碰撞未关闭");
}

FName ULevelSequenceCheckRuleSpawnableCollision::GetCategory() const
{
	return FName("Binding");
}

void ULevelSequenceCheckRuleSpawnableCollision::ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults)
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
	const FMovieSceneBindingReferences* BindingRefs = InSequence->GetBindingReferences();
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

	int32 SpawnableFound = 0;
	int32 PossessableCount = 0;

	// Iterate all bindings, detect spawnables via CustomBinding system (UE 5.7+)
	// In UE 5.7, Spawnables are stored as CustomBindings (UMovieSceneSpawnableBindingBase)
	// rather than in the legacy FMovieSceneSpawnable array.
	for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
	{
		bool bIsSpawnable = false;
		if (BindingRefs)
		{
			const UMovieSceneCustomBinding* CustomBinding = BindingRefs->GetCustomBinding(Binding.GetObjectGuid(), 0);
			if (Cast<UMovieSceneSpawnableBindingBase>(CustomBinding))
			{
				bIsSpawnable = true;
			}
		}
		// Fallback: legacy FMovieSceneSpawnable array (pre-5.7 sequences)
		if (!bIsSpawnable && MovieScene->FindSpawnable(Binding.GetObjectGuid()))
		{
			bIsSpawnable = true;
		}

		if (bIsSpawnable)
		{
			++SpawnableFound;
		}
		else
		{
			++PossessableCount;
		}
	}

	//UE_LOG(LogTemp, Log, TEXT("[B-01] ExecuteCheck: Sequence=%s, SpawnableCount=%d, PossessableCount=%d"),
	//	*InSequence->GetPathName(), SpawnableFound, PossessableCount);

	// Iterate all bindings and process spawnables
	for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
	{
		FGuid BindingGuid = Binding.GetObjectGuid();
		UObject* Template = nullptr;
		FString SpawnableName;

		// Try new CustomBinding system first (UE 5.7+)
		if (BindingRefs)
		{
			const UMovieSceneCustomBinding* CustomBinding = BindingRefs->GetCustomBinding(BindingGuid, 0);
			const UMovieSceneSpawnableBindingBase* SpawnableBinding = Cast<UMovieSceneSpawnableBindingBase>(CustomBinding);
			if (SpawnableBinding)
			{
				Template = const_cast<UMovieSceneSpawnableBindingBase*>(SpawnableBinding)->GetObjectTemplate();
				// Get name from Possessable (in UE 5.7, spawnable names are stored in Possessable)
				FMovieScenePossessable* Possessable = MovieScene->FindPossessable(BindingGuid);
				SpawnableName = Possessable ? Possessable->GetName() : BindingGuid.ToString();
			}
		}

		// Fallback: legacy FMovieSceneSpawnable (pre-5.7 sequences)
		if (!Template)
		{
			FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(BindingGuid);
			if (!Spawnable)
			{
				continue;
			}
			Template = Spawnable->GetObjectTemplate();
			SpawnableName = Spawnable->GetName();
		}

		//UE_LOG(LogTemp, Log, TEXT("[B-01] Spawnable: Name=%s, Template=%s (Class=%s)"),
		//	*SpawnableName,
		//	Template ? *Template->GetName() : TEXT("NULL"),
		//	Template ? *Template->GetClass()->GetName() : TEXT("N/A"));

		AActor* TemplateActor = Cast<AActor>(Template);
		if (!TemplateActor)
		{
			continue;
		}

		// Prefer the preview instance over the Template to avoid reading stale values.
		// When user modifies Spawnable properties through Sequencer's detail panel,
		// changes are applied to the preview instance first; the Template may not
		// reflect those changes until after the save process completes.
		//
		// Resolution order:
		// 1. ISequencer::FindSpawnedObjectOrTemplate — uses Sequencer's internal
		//    spawn register to find the live preview instance. This always has the
		//    most up-to-date property values.
		// 2. ULevelSequence::LocateBoundObjects — uses UOL resolution, which may
		//    find the Template for Spawnables.
		// 3. Template Actor (fallback) — with LoadProfileData(false) to refresh
		//    the collision cache from CollisionProfileName.
		AActor* ActorToCheck = TemplateActor;

		// Try ISequencer first (most reliable for Spawnable preview instances)
		TSharedPtr<ISequencer> ActiveSequencer = FLevelSequenceCheckerModule::Get().FindSequencerForSequence(InSequence);
		if (ActiveSequencer.IsValid())
		{
			if (UObject* SpawnedObj = ActiveSequencer->FindSpawnedObjectOrTemplate(BindingGuid))
			{
				if (AActor* PreviewActor = Cast<AActor>(SpawnedObj))
				{
					ActorToCheck = PreviewActor;
				}
			}
		}

		// Fallback: try LocateBoundObjects with UOL resolution
		if (ActorToCheck == TemplateActor && EditorWorld)
		{
			TArray<UObject*, TInlineAllocator<1>> BoundObjects;
			UE::UniversalObjectLocator::FResolveParams ResolveParams(EditorWorld);
			InSequence->LocateBoundObjects(BindingGuid, ResolveParams, BoundObjects);
			for (UObject* Obj : BoundObjects)
			{
				if (AActor* PreviewActor = Cast<AActor>(Obj))
				{
					ActorToCheck = PreviewActor;
					break;
				}
			}
		}

		// If still using Template, refresh collision cache from profile data.
		// IMPORTANT: Use LoadProfileData(false) not LoadProfileData(true).
		// With bVerifyProfile=true, LoadProfileData compares the cached values
		// (CollisionEnabled, ObjectType, CollisionResponses) against the profile
		// template. If they don't match (stale cache), it INVALIDATES the profile
		// name — making the problem worse. With bVerifyProfile=false, it calls
		// UCollisionProfile::ReadConfig() which loads the profile settings INTO
		// the BodyInstance, effectively refreshing the cache from the profile name.
		if (ActorToCheck == TemplateActor)
		{
			TArray<UPrimitiveComponent*> PrimComps;
			TemplateActor->GetComponents<UPrimitiveComponent>(PrimComps);
			for (UPrimitiveComponent* PrimComp : PrimComps)
			{
				PrimComp->BodyInstance.LoadProfileData(false);
			}
		}

		// Check StaticMeshComponents
		TArray<UStaticMeshComponent*> StaticMeshComps;
		ActorToCheck->GetComponents<UStaticMeshComponent>(StaticMeshComps);
		//UE_LOG(LogTemp, Log, TEXT("[B-01] Spawnable '%s' StaticMeshComponent count: %d"), *SpawnableName, StaticMeshComps.Num());

		for (UStaticMeshComponent* MeshComp : StaticMeshComps)
		{
			ECollisionEnabled::Type CollisionEnabled = MeshComp->GetCollisionEnabled();
			FName CollisionProfile = MeshComp->GetCollisionProfileName();
			//UE_LOG(LogTemp, Log, TEXT("[B-01]   StaticMeshComp: %s, CollisionEnabled=%d, CollisionProfile=%s"),
			//	*MeshComp->GetName(), (int32)CollisionEnabled, *CollisionProfile.ToString());

			if (CollisionEnabled != ECollisionEnabled::NoCollision)
			{
				FLevelSequenceCheckResult Result;
				Result.RuleId = GetRuleId();
				Result.RuleName = GetRuleName();
				Result.Description = FText::Format(
					LOCTEXT("CollisionNotDisabled", "组件 '{0}' 的碰撞配置为 '{1}' (CollisionEnabled={2})，应为 NoCollision。"),
					FText::FromString(MeshComp->GetName()),
					FText::FromString(CollisionProfile.ToString()),
					FText::FromString(UEnum::GetDisplayValueAsText(CollisionEnabled).ToString())
				);
				Result.LocationInfo = FText::Format(
					LOCTEXT("CollisionLoc", "Spawnable: {0}"),
					FText::FromString(SpawnableName)
				);
				Result.Severity = ELevelSequenceCheckSeverity::Error;
				OutResults.Add(MoveTemp(Result));
			}
		}

		// Check SkeletalMeshComponents
		TArray<USkeletalMeshComponent*> SkeletalMeshComps;
		ActorToCheck->GetComponents<USkeletalMeshComponent>(SkeletalMeshComps);
		//UE_LOG(LogTemp, Log, TEXT("[B-01] Spawnable '%s' SkeletalMeshComponent count: %d"), *SpawnableName, SkeletalMeshComps.Num());

		for (USkeletalMeshComponent* MeshComp : SkeletalMeshComps)
		{
			ECollisionEnabled::Type CollisionEnabled = MeshComp->GetCollisionEnabled();
			FName CollisionProfile = MeshComp->GetCollisionProfileName();
			//UE_LOG(LogTemp, Log, TEXT("[B-01]   SkeletalMeshComp: %s, CollisionEnabled=%d, CollisionProfile=%s"),
			//	*MeshComp->GetName(), (int32)CollisionEnabled, *CollisionProfile.ToString());

			if (CollisionEnabled != ECollisionEnabled::NoCollision)
			{
				FLevelSequenceCheckResult Result;
				Result.RuleId = GetRuleId();
				Result.RuleName = GetRuleName();
				Result.Description = FText::Format(
					LOCTEXT("SkelCollisionNotDisabled", "组件 '{0}' 的碰撞配置为 '{1}' (CollisionEnabled={2})，应为 NoCollision。"),
					FText::FromString(MeshComp->GetName()),
					FText::FromString(CollisionProfile.ToString()),
					FText::FromString(UEnum::GetDisplayValueAsText(CollisionEnabled).ToString())
				);
				Result.LocationInfo = FText::Format(
					LOCTEXT("SkelCollisionLoc", "Spawnable: {0}"),
					FText::FromString(SpawnableName)
				);
				Result.Severity = ELevelSequenceCheckSeverity::Error;
				OutResults.Add(MoveTemp(Result));
			}
		}
	}
}

void ULevelSequenceCheckRuleSpawnableCollision::CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder)
{
	CategoryBuilder.AddCustomRow(LOCTEXT("RuleHeader", "B-01"))
	.WholeRowContent()
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("RuleTitle", "B-01 Spawnable 碰撞未关闭"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
	];

	TArray<UObject*> ExternalObjects;
	ExternalObjects.Add(this);

	CategoryBuilder.AddExternalObjectProperty(ExternalObjects, GET_MEMBER_NAME_CHECKED(ULevelSequenceCheckRuleBase, bEnabled));
}

#undef LOCTEXT_NAMESPACE
