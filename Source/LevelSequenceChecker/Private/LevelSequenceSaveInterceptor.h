#pragma once

#include "CoreMinimal.h"

class FLevelSequenceCheckReporter;

/**
 * Per-file backup data stored when a save is intercepted.
 * Keeps the original disk content so it can be restored in PostSave,
 * effectively preventing the errored asset from being serialized.
 */
struct FPackageBackupEntry
{
	/** Absolute path to the .uasset file on disk (before save overwrites it) */
	FString AssetFilePath;

	/** Original file content read from disk at PreSave time */
	TArray64<uint8> OriginalFileData;
};

/**
 * Listens for pre-save events, filters for ULevelSequence assets,
 * and dispatches enabled check rules.
 *
 * When errors are found in PreSave:
 *   1. The current .uasset file on disk is backed up into memory.
 *   2. Serialization completes (cannot be aborted via the event).
 *   3. In PostSave, the backup overwrites the just-saved file,
 *      restoring the disk to its pre-save state.
 *   4. The package is re-marked dirty so the editor still shows
 *      the asset as unsaved.
 *
 * This achieves the effect of "blocking serialization" even though
 * UE5's PreSavePackageWithContextEvent is notification-only.
 *
 * Also registers an editor-close interceptor via IMainFrameModule
 * to prevent closing the editor when LevelSequence assets have errors.
 *
 * Lifecycle: Created/destroyed by FLevelSequenceCheckerModule.
 * Intentionally NOT a USubsystem — the save event is global and
 * doesn't depend on World/Editor lifetime.
 */
class FLevelSequenceSaveInterceptor
{
public:
	explicit FLevelSequenceSaveInterceptor(FLevelSequenceCheckReporter& InReporter);
	~FLevelSequenceSaveInterceptor();

	/** Register the editor-close interceptor delegate */
	void RegisterCanCloseDelegate();

	/** Unregister the editor-close interceptor delegate */
	void UnregisterCanCloseDelegate();

private:
	/** Callback for UPackage::PreSavePackageWithContextEvent */
	void OnPreSavePackage(UPackage* Package, FObjectPreSaveContext ObjectSaveContext);

	/** Callback for UPackage::PackageSavedWithContextEvent — restores backup if errors were found */
	void OnPackageSaved(const FString& PackageFilename, UPackage* Package, FObjectPostSaveContext ObjectSaveContext);

	/** Callback for IMainFrameModule::RegisterCanCloseEditor — returns false to block editor close */
	bool OnCanCloseEditor() const;

	/** Check all loaded LevelSequence assets for errors, returns true if any have errors */
	bool CheckLoadedSequencesForErrors(TArray<UPackage*>& OutDirtyErrorPackages, TArray<UPackage*>& OutNonDirtyErrorPackages) const;

	/**
	 * Read the on-disk .uasset file for the given package into a byte array.
	 * Returns false if the file cannot be read (e.g. new asset not yet saved).
	 */
	static bool ReadPackageDiskFile(UPackage* Package, FString& OutFilePath, TArray64<uint8>& OutData);

	FLevelSequenceCheckReporter& Reporter;

	/**
	 * Packages that had errors during PreSave and whose disk files have been
	 * backed up. In PostSave, these backups are written back to disk to
	 * undo the serialization, and the package is re-marked dirty.
	 */
	TMap<UPackage*, FPackageBackupEntry> PackageBackups;

	/** Handle for the CanCloseEditor delegate */
	FDelegateHandle CanCloseDelegateHandle;
};
