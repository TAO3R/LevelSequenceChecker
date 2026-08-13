#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FLevelSequenceSaveInterceptor;
class FLevelSequenceCheckReporter;
class FMissingRefCache;
class ISequencer;
class ULevelSequence;

class FLevelSequenceCheckerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FLevelSequenceCheckerModule& Get()
	{
		return FModuleManager::Get().GetModuleChecked<FLevelSequenceCheckerModule>("LevelSequenceChecker");
	}

	/** Access the missing reference cache */
	FMissingRefCache& GetMissingRefCache() const;

	/**
	 * Find the active ISequencer that is editing the given LevelSequence.
	 * Returns nullptr if no matching sequencer is found.
	 * This is used by check rules to access preview instances of Spawnables,
	 * which have up-to-date property values (unlike the Template which may be stale).
	 */
	TSharedPtr<ISequencer> FindSequencerForSequence(ULevelSequence* InSequence) const;

private:
	/** Called when a new sequencer is created */
	void OnSequencerCreated(TSharedRef<ISequencer> InSequencer);

	/** Clean up expired weak references */
	void PruneDeadSequencers() const;

	TUniquePtr<FLevelSequenceSaveInterceptor> SaveInterceptor;
	FLevelSequenceSaveInterceptor* SaveInterceptorPtr = nullptr;
	TUniquePtr<FLevelSequenceCheckReporter> Reporter;
	TUniquePtr<FMissingRefCache> MissingRefCache;

	/** Handle for the OnSequencerCreated delegate */
	FDelegateHandle SequencerCreatedHandle;

	/** Weak references to active sequencer instances */
	mutable TArray<TWeakPtr<ISequencer>> ActiveSequencers;
};
