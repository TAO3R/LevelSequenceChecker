#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceCheckRuleBase.h"
#include "LevelSequenceCheckRuleMissingReference.generated.h"

class UMovieSceneTrack;
class UMovieSceneSection;
struct FMovieSceneBinding;
struct FMovieSceneObjectPathChannel;

/**
  * R-01: Missing Asset Reference Rule
  * Refactored: Uses dual-source cross-validation (AssetRegistry + Section traversal)
  * to distinguish "intentionally empty" from "missing due to deleted asset".
  */
UCLASS(meta=(DisplayName="R-01 丢失资源引用"))
class LEVELSEQUENCECHECKER_API ULevelSequenceCheckRuleMissingReference : public ULevelSequenceCheckRuleBase
{
    GENERATED_BODY()
	
public:
	ULevelSequenceCheckRuleMissingReference();
	
	// ULevelSequenceCheckRuleBase Interface
	virtual FName GetRuleId() const override;
	virtual FText GetRuleName() const override;
	virtual FName GetCategory() const override;
	virtual void ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults) override;
	virtual void CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder) override;
	// End
	
private:
    /** Phase 1: Collect all dependency packages that don't exist on disk via AssetRegistry */
    TSet<FString> CollectMissingDependencies(ULevelSequence* InSequence);

    /** Phase 2A: Type-specific Section null-check + cross-reference with MissingPackages */
    void CheckSectionByType(
        UMovieSceneSection* Section,
        const FText& LocationContext,
        const TSet<FString>& MissingPackages,
        TSet<FString>& ConsumedPackages,
        TArray<FLevelSequenceCheckResult>& OutResults);

    /** Phase 2A extension: Check FMovieSceneObjectPathChannel key values */
    void CheckObjectPathChannel(
        const FMovieSceneObjectPathChannel& Channel,
        const FText& LocationContext,
        const TSet<FString>& MissingPackages,
        TSet<FString>& ConsumedPackages,
        TArray<FLevelSequenceCheckResult>& OutResults);

    /** Phase 2B: Reflection fallback — FObjectProperty scan for Section types not covered by 2A */
    void CheckSectionByReflection(
        UMovieSceneSection* Section,
        const FText& LocationContext,
        const TSet<FString>& MissingPackages,
        TSet<FString>& ConsumedPackages,
        TArray<FLevelSequenceCheckResult>& OutResults);

    /** Phase 2B helper: recursively check FStructProperty / FArrayProperty for nested FObjectProperty */
    void CheckPropertyRecursive(
        FProperty* Property,
        void* ContainerPtr,
        const FText& LocationContext,
        const TSet<FString>& MissingPackages,
        TSet<FString>& ConsumedPackages,
        TArray<FLevelSequenceCheckResult>& OutResults,
        int32 Depth = 0);

    /** Spawnable ObjectTemplate null-check + cross-reference with MissingPackages */
    void CheckSpawnableTemplate(
        UObject* Template,
        const FString& SpawnableName,
        const FText& LocationContext,
        const TSet<FString>& MissingPackages,
        TSet<FString>& ConsumedPackages,
        TArray<FLevelSequenceCheckResult>& OutResults);

    /** Soft reference path check (preserved from original) */
    void CheckSoftReferences(
        UObject* Obj,
        const FText& LocationContext,
        TArray<FLevelSequenceCheckResult>& OutResults);

    /** Phase 3: Report missing packages not consumed by Phase 2 (with FReferenceFinder validation) */
    void ReportUnconsumedMissingPackages(
        ULevelSequence* InSequence,
        const TSet<FString>& MissingPackages,
        const TSet<FString>& ConsumedPackages,
        TArray<FLevelSequenceCheckResult>& OutResults);

    /** Helper: validate a single soft reference path */
    void VerifySoftObjectPath(const FSoftObjectPath& Path, const FText& Context, TArray<FLevelSequenceCheckResult>& OutResults);
};
