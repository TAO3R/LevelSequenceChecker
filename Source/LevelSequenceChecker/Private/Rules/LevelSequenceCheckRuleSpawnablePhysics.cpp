#include "Rules/LevelSequenceCheckRuleSpawnablePhysics.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneBindingReferences.h"
#include "Bindings/MovieSceneSpawnableBinding.h"
#include "Components/PrimitiveComponent.h"
#include "Editor.h"
#include "UniversalObjectLocatorResolveParams.h"
#include "ISequencer.h"
#include "LevelSequenceCheckerModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LevelSequenceCheckRuleSpawnablePhysics"

ULevelSequenceCheckRuleSpawnablePhysics::ULevelSequenceCheckRuleSpawnablePhysics()
{
	bEnabled = true;
}

FName ULevelSequenceCheckRuleSpawnablePhysics::GetRuleId() const
{
	return FName("B-02");
}

FText ULevelSequenceCheckRuleSpawnablePhysics::GetRuleName() const
{
	return LOCTEXT("RuleName", "Spawnable 物理模拟开启");
}

FName ULevelSequenceCheckRuleSpawnablePhysics::GetCategory() const
{
	return FName("Binding");
}

void ULevelSequenceCheckRuleSpawnablePhysics::ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults)
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

	// Iterate all bindings, detect spawnables via CustomBinding system (UE 5.7+)
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
		//    spawn register to find the live preview instance.
		// 2. ULevelSequence::LocateBoundObjects — uses UOL resolution.
		// 3. Template Actor (fallback)
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

		// Iterate all components, looking for PrimitiveComponents with Simulate Physics
		TArray<UActorComponent*> Components;
		ActorToCheck->GetComponents(Components);

		for (UActorComponent* Comp : Components)
		{
			UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp);
			if (!PrimComp)
			{
				continue;
			}

			// Check BodyInstance.bSimulatePhysics
			if (PrimComp->BodyInstance.bSimulatePhysics)
			{
				FLevelSequenceCheckResult Result;
				Result.RuleId = GetRuleId();
				Result.RuleName = GetRuleName();
				Result.Description = FText::Format(
					LOCTEXT("PhysicsEnabled", "组件 '{0}' 开启了物理模拟（Simulate Physics）。"),
					FText::FromString(PrimComp->GetName())
				);
				Result.LocationInfo = FText::Format(
					LOCTEXT("PhysicsLoc", "Spawnable: {0}"),
					FText::FromString(SpawnableName)
				);
				Result.Severity = ELevelSequenceCheckSeverity::Error;
				OutResults.Add(MoveTemp(Result));
			}
		}
	}
}

void ULevelSequenceCheckRuleSpawnablePhysics::CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder)
{
	CategoryBuilder.AddCustomRow(LOCTEXT("RuleHeader", "B-02"))
	.WholeRowContent()
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("RuleTitle", "B-02 Spawnable 物理模拟开启"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
	];

	TArray<UObject*> ExternalObjects;
	ExternalObjects.Add(this);

	CategoryBuilder.AddExternalObjectProperty(ExternalObjects, GET_MEMBER_NAME_CHECKED(ULevelSequenceCheckRuleBase, bEnabled));
}

#undef LOCTEXT_NAMESPACE
