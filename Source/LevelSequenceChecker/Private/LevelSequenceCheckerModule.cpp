#include "LevelSequenceCheckerModule.h"

#include "LevelSequenceSaveInterceptor.h"
#include "LevelSequenceCheckReporter.h"
#include "LevelSequenceCheckerSettings.h"
#include "LevelSequenceCheckerSettingsCustomization.h"
#include "MissingRefCache.h"

#include "Engine/DeveloperSettings.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/IConsoleManager.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ISequencerModule.h"
#include "ISequencer.h"

#define LOCTEXT_NAMESPACE "FLevelSequenceCheckerModule"

void FLevelSequenceCheckerModule::StartupModule()
{
	// Register Message Log listing
	FMessageLogModule& MessageLogModule = FModuleManager::Get().LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.RegisterLogListing("LevelSequenceChecker", LOCTEXT("LogListingName", "LevelSequence 检查器"));

	// Register Detail Customization for Settings
	FPropertyEditorModule& PropertyModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(
		ULevelSequenceCheckerSettings::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FLevelSequenceCheckerSettingsCustomization::MakeInstance)
	);

	// Create Missing Reference Cache (load from disk)
	MissingRefCache = MakeUnique<FMissingRefCache>();
	MissingRefCache->LoadFromDisk();

	// Create Reporter first (Interceptor depends on it)
	Reporter = MakeUnique<FLevelSequenceCheckReporter>();

	// Create Save Interceptor
	SaveInterceptor = MakeUnique<FLevelSequenceSaveInterceptor>(*Reporter);
	SaveInterceptorPtr = SaveInterceptor.Get();

	// Register editor-close interceptor
	SaveInterceptor->RegisterCanCloseDelegate();

	// Register for Sequencer creation events to track active ISequencer instances.
	// This allows check rules to find preview instances of Spawnables, which have
	// up-to-date property values (unlike the Template which may be stale at PreSave time).
	if (FModuleManager::Get().IsModuleLoaded("Sequencer"))
	{
		ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer");
		SequencerCreatedHandle = SequencerModule.RegisterOnSequencerCreated(
			FOnSequencerCreated::FDelegate::CreateRaw(this, &FLevelSequenceCheckerModule::OnSequencerCreated)
		);
	}

	// Register console command for testing: inject NaN/Inf into keyframes
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("LSC.InjectNaNInf"),
		TEXT("Inject NaN and Inf into the first 2 keyframes of every numeric channel in all loaded LevelSequence assets. Usage: LSC.InjectNaNInf"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			// Find all loaded LevelSequence instances via asset registry
			TArray<UObject*> LoadedSequences;
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			TArray<FAssetData> AssetDataList;
			AssetRegistryModule.Get().GetAssetsByClass(ULevelSequence::StaticClass()->GetClassPathName(), AssetDataList);
			for (const FAssetData& AssetData : AssetDataList)
			{
				if (UObject* Asset = AssetData.GetAsset())
				{
					LoadedSequences.Add(Asset);
				}
			}

			int32 TotalModified = 0;

			for (UObject* Obj : LoadedSequences)
			{
				ULevelSequence* Seq = Cast<ULevelSequence>(Obj);
				if (!Seq) continue;

				UMovieScene* MovieScene = Seq->GetMovieScene();
				if (!MovieScene) continue;

				int32 ModifiedChannels = 0;

				auto ProcessSection = [&](UMovieSceneSection* Section)
				{
					if (!Section) return;
					const FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();

					// Float channels
					TArrayView<FMovieSceneFloatChannel*> FloatChannels = ChannelProxy.GetChannels<FMovieSceneFloatChannel>();
					for (FMovieSceneFloatChannel* Channel : FloatChannels)
					{
						if (Channel && Channel->GetNumKeys() >= 2)
						{
							auto Data = Channel->GetData();
							Data.GetValues()[0].Value = std::numeric_limits<float>::quiet_NaN();
							Data.GetValues()[1].Value = std::numeric_limits<float>::infinity();
							++ModifiedChannels;
						}
					}

					// Double channels
					TArrayView<FMovieSceneDoubleChannel*> DoubleChannels = ChannelProxy.GetChannels<FMovieSceneDoubleChannel>();
					for (FMovieSceneDoubleChannel* Channel : DoubleChannels)
					{
						if (Channel && Channel->GetNumKeys() >= 2)
						{
							auto Data = Channel->GetData();
							Data.GetValues()[0].Value = std::numeric_limits<double>::quiet_NaN();
							Data.GetValues()[1].Value = std::numeric_limits<double>::infinity();
							++ModifiedChannels;
						}
					}
				};

				// Process master tracks
				for (UMovieSceneTrack* Track : MovieScene->GetTracks())
				{
					for (UMovieSceneSection* Section : Track->GetAllSections())
					{
						ProcessSection(Section);
					}
				}

				// Process binding tracks
				const UMovieScene* ConstMovieScene = MovieScene;
				for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
				{
					for (UMovieSceneTrack* Track : Binding.GetTracks())
					{
						for (UMovieSceneSection* Section : Track->GetAllSections())
						{
							ProcessSection(Section);
						}
					}
				}

				if (ModifiedChannels > 0)
				{
					TotalModified += ModifiedChannels;
					UE_LOG(LogTemp, Log, TEXT("[LSC.InjectNaNInf] Modified %d channels in %s"), ModifiedChannels, *Seq->GetPathName());
				}
			}

			if (TotalModified > 0)
			{
				UE_LOG(LogTemp, Log, TEXT("[LSC.InjectNaNInf] Done! Total %d channels modified. Save Sequence(s) to trigger K-01."), TotalModified);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[LSC.InjectNaNInf] No numeric channels with 2+ keys found in any loaded LevelSequence."));
			}
		}),
		ECVF_Default
	);

	// Register console command for cleaning NaN/Inf keyframes
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("LSC.CleanNaNInf"),
		TEXT("Reset all NaN/Inf keyframe values to 0 in all loaded LevelSequence assets. Usage: LSC.CleanNaNInf"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			TArray<FAssetData> AssetDataList;
			AssetRegistryModule.Get().GetAssetsByClass(ULevelSequence::StaticClass()->GetClassPathName(), AssetDataList);

			int32 TotalFixed = 0;

			for (const FAssetData& AssetData : AssetDataList)
			{
				UObject* Asset = AssetData.GetAsset();
				ULevelSequence* Seq = Cast<ULevelSequence>(Asset);
				if (!Seq) continue;

				UMovieScene* MovieScene = Seq->GetMovieScene();
				if (!MovieScene) continue;

				int32 FixedInSeq = 0;

				auto CleanSection = [&](UMovieSceneSection* Section)
				{
					if (!Section) return;
					const FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();

					// Float channels
					TArrayView<FMovieSceneFloatChannel*> FloatChannels = ChannelProxy.GetChannels<FMovieSceneFloatChannel>();
					for (FMovieSceneFloatChannel* Channel : FloatChannels)
					{
						if (!Channel || Channel->GetNumKeys() == 0) continue;
						auto Data = Channel->GetData();
						TArrayView<FMovieSceneFloatValue> Values = Data.GetValues();
						for (int32 i = 0; i < Values.Num(); ++i)
						{
							if (FMath::IsNaN(Values[i].Value) || !FMath::IsFinite(Values[i].Value))
							{
								Values[i].Value = 0.0f;
								++FixedInSeq;
							}
						}
					}

					// Double channels
					TArrayView<FMovieSceneDoubleChannel*> DoubleChannels = ChannelProxy.GetChannels<FMovieSceneDoubleChannel>();
					for (FMovieSceneDoubleChannel* Channel : DoubleChannels)
					{
						if (!Channel || Channel->GetNumKeys() == 0) continue;
						auto Data = Channel->GetData();
						TArrayView<FMovieSceneDoubleValue> Values = Data.GetValues();
						for (int32 i = 0; i < Values.Num(); ++i)
						{
							if (FMath::IsNaN(Values[i].Value) || !FMath::IsFinite(Values[i].Value))
							{
								Values[i].Value = 0.0;
								++FixedInSeq;
							}
						}
					}
				};

				// Master tracks
				for (UMovieSceneTrack* Track : MovieScene->GetTracks())
				{
					for (UMovieSceneSection* Section : Track->GetAllSections())
					{
						CleanSection(Section);
					}
				}

				// Binding tracks
				const UMovieScene* ConstMovieScene = MovieScene;
				for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
				{
					for (UMovieSceneTrack* Track : Binding.GetTracks())
					{
						for (UMovieSceneSection* Section : Track->GetAllSections())
						{
							CleanSection(Section);
						}
					}
				}

				if (FixedInSeq > 0)
				{
					Seq->MarkPackageDirty();
					TotalFixed += FixedInSeq;
					UE_LOG(LogTemp, Log, TEXT("[LSC.CleanNaNInf] Fixed %d keyframes in %s"), FixedInSeq, *Seq->GetPathName());
				}
			}

			if (TotalFixed > 0)
			{
				UE_LOG(LogTemp, Log, TEXT("[LSC.CleanNaNInf] Done! Total %d keyframes reset to 0. Save the assets to persist."), TotalFixed);
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("[LSC.CleanNaNInf] No NaN/Inf keyframes found."));
			}
		}),
		ECVF_Default
	);
}

void FLevelSequenceCheckerModule::ShutdownModule()
{
	// Unregister Sequencer creation delegate
	if (SequencerCreatedHandle.IsValid() && FModuleManager::Get().IsModuleLoaded("Sequencer"))
	{
		ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer");
		SequencerModule.UnregisterOnSequencerCreated(SequencerCreatedHandle);
		SequencerCreatedHandle.Reset();
	}
	ActiveSequencers.Empty();

	// Unregister console commands
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("LSC.InjectNaNInf"));
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("LSC.CleanNaNInf"));

	// Destroy in reverse order
	SaveInterceptor.Reset();
	SaveInterceptorPtr = nullptr;
	Reporter.Reset();

	// Save and destroy cache
	if (MissingRefCache)
	{
		MissingRefCache->SaveToDisk();
		MissingRefCache.Reset();
	}

	// Unregister Detail Customization
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout(ULevelSequenceCheckerSettings::StaticClass()->GetFName());
	}

	// Unregister Message Log listing
	if (FModuleManager::Get().IsModuleLoaded("MessageLog"))
	{
		FMessageLogModule& MessageLogModule = FModuleManager::Get().GetModuleChecked<FMessageLogModule>("MessageLog");
		MessageLogModule.UnregisterLogListing("LevelSequenceChecker");
	}
}

FMissingRefCache& FLevelSequenceCheckerModule::GetMissingRefCache() const
{
	check(MissingRefCache.IsValid());
	return *MissingRefCache;
}

void FLevelSequenceCheckerModule::OnSequencerCreated(TSharedRef<ISequencer> InSequencer)
{
	ActiveSequencers.Add(InSequencer);
}

void FLevelSequenceCheckerModule::PruneDeadSequencers() const
{
	ActiveSequencers.RemoveAll([](const TWeakPtr<ISequencer>& WeakSeq)
	{
		return !WeakSeq.IsValid();
	});
}

TSharedPtr<ISequencer> FLevelSequenceCheckerModule::FindSequencerForSequence(ULevelSequence* InSequence) const
{
	if (!InSequence)
	{
		return nullptr;
	}

	PruneDeadSequencers();

	for (const TWeakPtr<ISequencer>& WeakSeq : ActiveSequencers)
	{
		TSharedPtr<ISequencer> Seq = WeakSeq.Pin();
		if (!Seq.IsValid())
		{
			continue;
		}

		// Check both root and focused sequences — the LevelSequence might be focused as a sub-sequence
		UMovieSceneSequence* RootSequence = Seq->GetRootMovieSceneSequence();
		UMovieSceneSequence* FocusedSequence = Seq->GetFocusedMovieSceneSequence();

		if (RootSequence == InSequence || FocusedSequence == InSequence)
		{
			return Seq;
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLevelSequenceCheckerModule, LevelSequenceChecker)
