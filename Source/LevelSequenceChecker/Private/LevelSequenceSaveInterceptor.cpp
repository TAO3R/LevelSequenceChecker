#include "LevelSequenceSaveInterceptor.h"

#include "LevelSequenceCheckReporter.h"
#include "LevelSequenceCheckResult.h"
#include "LevelSequenceCheckerSettings.h"
#include "LevelSequenceCheckRuleBase.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "Misc/PackageName.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "Misc/MessageDialog.h"
#include "Logging/MessageLog.h"
#include "Interfaces/IMainFrameModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/PlatformFileManager.h"

#define LOCTEXT_NAMESPACE "LevelSequenceSaveInterceptor"

FLevelSequenceSaveInterceptor::FLevelSequenceSaveInterceptor(FLevelSequenceCheckReporter& InReporter)
	: Reporter(InReporter)
{
	UPackage::PreSavePackageWithContextEvent.AddRaw(this, &FLevelSequenceSaveInterceptor::OnPreSavePackage);
	UPackage::PackageSavedWithContextEvent.AddRaw(this, &FLevelSequenceSaveInterceptor::OnPackageSaved);
}

FLevelSequenceSaveInterceptor::~FLevelSequenceSaveInterceptor()
{
	UnregisterCanCloseDelegate();

	if (UPackage::PreSavePackageWithContextEvent.IsBoundToObject(this))
	{
		UPackage::PreSavePackageWithContextEvent.RemoveAll(this);
	}
	if (UPackage::PackageSavedWithContextEvent.IsBoundToObject(this))
	{
		UPackage::PackageSavedWithContextEvent.RemoveAll(this);
	}
}

void FLevelSequenceSaveInterceptor::RegisterCanCloseDelegate()
{
	if (CanCloseDelegateHandle.IsValid())
	{
		return; // Already registered
	}

	IMainFrameModule& MainFrameModule = IMainFrameModule::Get();
	CanCloseDelegateHandle = MainFrameModule.RegisterCanCloseEditor(
		IMainFrameModule::FMainFrameCanCloseEditor::CreateRaw(this, &FLevelSequenceSaveInterceptor::OnCanCloseEditor)
	);
}

void FLevelSequenceSaveInterceptor::UnregisterCanCloseDelegate()
{
	if (CanCloseDelegateHandle.IsValid())
	{
		if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
		{
			IMainFrameModule& MainFrameModule = IMainFrameModule::Get();
			MainFrameModule.UnregisterCanCloseEditor(CanCloseDelegateHandle);
		}
		CanCloseDelegateHandle.Reset();
	}
}

void FLevelSequenceSaveInterceptor::OnPreSavePackage(UPackage* Package, FObjectPreSaveContext ObjectSaveContext)
{
	// Cook / 命令行 commandlet 下不介入：本拦截器仅服务于编辑器交互式保存。
	// B-03 等规则依赖 GEditor 当前 EditorWorld 做 SyncFind，cook 按包遍历时 world 上下文错配，
	// 会把本无问题的 possessable 绑定误报为"丢失"，Error 级日志顶高 cook 退出码导致打包失败。
	if (ObjectSaveContext.IsCooking() || IsRunningCommandlet())
	{
		return;
	}

	// Quick filter: check if the package contains a ULevelSequence asset
	UObject* TopLevelAsset = Package ? Package->FindAssetInPackage() : nullptr;
	if (!TopLevelAsset || !TopLevelAsset->IsA<ULevelSequence>())
	{
		return;
	}
	
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(TopLevelAsset);

	// Read settings
	const ULevelSequenceCheckerSettings* Settings = GetDefault<ULevelSequenceCheckerSettings>();
	if (!Settings || !Settings->bEnableCheckOnSave)
	{
		return;
	}

	// Execute all enabled rules and collect results
	TArray<FLevelSequenceCheckResult> AllResults;

	for (const TObjectPtr<ULevelSequenceCheckRuleBase>& Rule : Settings->CheckRules)
	{
		if (!IsValid(Rule) || !Rule->bEnabled)
		{
			continue;
		}

		Rule->ExecuteCheck(LevelSequence, AllResults);
	}

	// Report if any issues found
	if (AllResults.Num() > 0)
	{
		// Count errors
		int32 ErrorCount = 0;
		for (const FLevelSequenceCheckResult& Result : AllResults)
		{
			if (Result.Severity == ELevelSequenceCheckSeverity::Error)
			{
				ErrorCount++;
			}
		}

		const bool bHasErrors = ErrorCount > 0;
		Reporter.Report(LevelSequence, AllResults, bHasErrors);

		if (bHasErrors)
		{
			// Back up the current on-disk file so we can restore it after save completes
			FPackageBackupEntry BackupEntry;
			if (ReadPackageDiskFile(Package, BackupEntry.AssetFilePath, BackupEntry.OriginalFileData))
			{
				PackageBackups.Add(Package, MoveTemp(BackupEntry));
			}
			else
			{
				// New asset that has never been saved to disk — nothing to back up.
				// We still add an entry with empty data so PostSave knows to re-dirty it.
				FPackageBackupEntry& EmptyEntry = PackageBackups.Add(Package);
				// AssetFilePath and OriginalFileData remain empty
			}

			// Show a modal dialog with two options: "查看" (Yes) to open Message Log, "关闭" (No) to dismiss
			const FText DialogTitle = LOCTEXT("SaveBlockedTitle", "保存被阻止");
			const FText DialogMessage = FText::Format(
				LOCTEXT("SaveBlockedMessage",
					"LevelSequence 检查器在资产 {0} 中发现 {1} 个错误，保存已被阻止。\n\n"
					"点击\"是\"查看详细错误信息，点击\"否\"关闭此窗口。"),
				FText::FromString(FPaths::GetBaseFilename(LevelSequence->GetPathName())),
				ErrorCount
			);
			const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, DialogMessage, DialogTitle);
			if (Result == EAppReturnType::Yes)
			{
				// Open the Message Log panel and navigate to the LevelSequenceChecker log
				FMessageLog("LevelSequenceChecker").Open(EMessageSeverity::Error, true);
			}
		}
	}
}

void FLevelSequenceSaveInterceptor::OnPackageSaved(const FString& PackageFilename, UPackage* Package, FObjectPostSaveContext ObjectSaveContext)
{
	// 与 OnPreSavePackage 对称：cook / commandlet 下不介入，避免对源资产做磁盘回写与 SetDirtyFlag。
	if (ObjectSaveContext.IsCooking() || IsRunningCommandlet())
	{
		return;
	}

	FPackageBackupEntry* BackupEntry = PackageBackups.Find(Package);
	if (!BackupEntry)
	{
		return; // No backup — this package had no errors, normal save
	}

	// Restore the backed-up file to disk, undoing the serialization
	if (BackupEntry->OriginalFileData.Num() > 0 && !BackupEntry->AssetFilePath.IsEmpty())
	{
		IFileManager& FileManager = IFileManager::Get();
		if (FileManager.FileExists(*BackupEntry->AssetFilePath))
		{
			// Overwrite the just-saved file with the backup
			FFileHelper::SaveArrayToFile(BackupEntry->OriginalFileData, *BackupEntry->AssetFilePath);
		}
	}

	// Re-mark the package as dirty so the editor shows it as unsaved
	Package->SetDirtyFlag(true);

	// Clean up the backup entry
	PackageBackups.Remove(Package);
}

bool FLevelSequenceSaveInterceptor::OnCanCloseEditor() const
{
	TArray<UPackage*> DirtyErrorPackages;
	TArray<UPackage*> NonDirtyErrorPackages;

	if (!CheckLoadedSequencesForErrors(DirtyErrorPackages, NonDirtyErrorPackages))
	{
		// No errors found — allow closing
		return true;
	}

	// Build the dialog message based on whether there are dirty packages with errors
	const int32 TotalErrorPackages = DirtyErrorPackages.Num() + NonDirtyErrorPackages.Num();

	if (DirtyErrorPackages.Num() > 0 && NonDirtyErrorPackages.Num() > 0)
	{
		// Mix: some dirty, some non-dirty
		const FText DialogMessage = FText::Format(
			LOCTEXT("CloseBlockedMixed",
				"有 {0} 个 LevelSequence 资产存在错误，无法正常关闭引擎。\n\n"
				"其中 {1} 个资产包含未保存的错误改动，可选择\"丢弃改动并退出\"。\n"
				"另有 {2} 个资产在启动前就已存在错误，无法丢弃改动。\n\n"
				"请修复所有错误后重试，或点击\"查看\"打开 Message Log 查看详情。"),
			TotalErrorPackages,
			DirtyErrorPackages.Num(),
			NonDirtyErrorPackages.Num()
		);

		const FText DialogTitle = LOCTEXT("CloseBlockedTitle", "无法关闭引擎");
		const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::OkCancel, DialogMessage, DialogTitle);
		if (Result == EAppReturnType::Ok)
		{
			FMessageLog("LevelSequenceChecker").Open(EMessageSeverity::Error, true);
		}
		return false;
	}
	else if (DirtyErrorPackages.Num() > 0)
	{
		// All error packages are dirty — user can choose to discard changes and exit
		const FText DialogMessage = FText::Format(
			LOCTEXT("CloseBlockedDirty",
				"有 {0} 个 LevelSequence 资产存在错误，无法正常关闭引擎。\n\n"
				"这些错误由未保存的改动导致。您可以选择：\n"
				"• 点击\"是\"：丢弃导致资产报错的改动并退出引擎（磁盘将保留上次正确版本）\n"
				"• 点击\"否\"：取消退出，返回编辑器修复问题"),
			DirtyErrorPackages.Num()
		);

		const FText DialogTitle = LOCTEXT("CloseBlockedTitle", "无法关闭引擎");
		const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, DialogMessage, DialogTitle);
		if (Result == EAppReturnType::Yes)
		{
			// Discard dirty flag on all error packages to allow close.
			// Disk files are already correct (restored by PostSave backup mechanism),
			// so clearing dirty will cause these in-memory changes to be lost on next load.
			for (UPackage* Pkg : DirtyErrorPackages)
			{
				Pkg->SetDirtyFlag(false);
			}
			return true;
		}
		return false;
	}
	else
	{
		// All error packages are non-dirty — cannot discard, can only cancel close
		const FText DialogMessage = FText::Format(
			LOCTEXT("CloseBlockedNonDirty",
				"有 {0} 个 LevelSequence 资产存在错误，无法正常关闭引擎。\n\n"
				"这些错误在引擎启动前就已存在，无法通过丢弃改动来解决。\n"
				"请修复错误后重试，或点击\"查看\"打开 Message Log 查看详情。"),
			NonDirtyErrorPackages.Num()
		);

		const FText DialogTitle = LOCTEXT("CloseBlockedTitle", "无法关闭引擎");
		const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::OkCancel, DialogMessage, DialogTitle);
		if (Result == EAppReturnType::Ok)
		{
			FMessageLog("LevelSequenceChecker").Open(EMessageSeverity::Error, true);
		}
		return false;
	}
}

bool FLevelSequenceSaveInterceptor::CheckLoadedSequencesForErrors(
	TArray<UPackage*>& OutDirtyErrorPackages,
	TArray<UPackage*>& OutNonDirtyErrorPackages) const
{
	OutDirtyErrorPackages.Reset();
	OutNonDirtyErrorPackages.Reset();

	const ULevelSequenceCheckerSettings* Settings = GetDefault<ULevelSequenceCheckerSettings>();
	if (!Settings || !Settings->bEnableCheckOnSave)
	{
		return false;
	}

	// Find all loaded LevelSequence assets via asset registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssetsByClass(ULevelSequence::StaticClass()->GetClassPathName(), AssetDataList);

	bool bFoundErrors = false;

	for (const FAssetData& AssetData : AssetDataList)
	{
		ULevelSequence* LevelSequence = Cast<ULevelSequence>(AssetData.GetAsset());
		if (!LevelSequence)
		{
			continue;
		}

		// Execute check rules
		TArray<FLevelSequenceCheckResult> AllResults;
		for (const TObjectPtr<ULevelSequenceCheckRuleBase>& Rule : Settings->CheckRules)
		{
			if (!IsValid(Rule) || !Rule->bEnabled)
			{
				continue;
			}
			Rule->ExecuteCheck(LevelSequence, AllResults);
		}

		// Check if any errors exist
		bool bHasErrors = false;
		for (const FLevelSequenceCheckResult& Result : AllResults)
		{
			if (Result.Severity == ELevelSequenceCheckSeverity::Error)
			{
				bHasErrors = true;
				break;
			}
		}

		if (bHasErrors)
		{
			bFoundErrors = true;
			UPackage* Package = LevelSequence->GetOutermost();
			if (Package && Package->IsDirty())
			{
				OutDirtyErrorPackages.Add(Package);
			}
			else if (Package)
			{
				OutNonDirtyErrorPackages.Add(Package);
			}
		}
	}

	return bFoundErrors;
}

bool FLevelSequenceSaveInterceptor::ReadPackageDiskFile(UPackage* Package, FString& OutFilePath, TArray64<uint8>& OutData)
{
	OutFilePath.Empty();
	OutData.Reset();

	if (!Package)
	{
		return false;
	}

	// Convert package name to disk file path
	const FString PackageName = Package->GetName();
	FString LocalFilePath;
	if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, LocalFilePath, FPackageName::GetAssetPackageExtension()))
	{
		return false;
	}

	// Make absolute
	FPaths::MakeStandardFilename(LocalFilePath);
	if (!FPaths::FileExists(LocalFilePath))
	{
		// Asset has never been saved to disk (new asset)
		return false;
	}

	// Read the entire file into memory
	if (!FFileHelper::LoadFileToArray(OutData, *LocalFilePath))
	{
		return false;
	}

	OutFilePath = LocalFilePath;
	return true;
}

#undef LOCTEXT_NAMESPACE
