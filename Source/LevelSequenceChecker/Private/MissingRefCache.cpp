#include "MissingRefCache.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformFileManager.h"

#define LOCTEXT_NAMESPACE "MissingRefCache"

FString FMissingRefCache::GetCacheFilePath() const
{
	return FPaths::ProjectSavedDir() / TEXT("LevelSequenceChecker") / TEXT("MissingRefCache.json");
}

void FMissingRefCache::LoadFromDisk()
{
	Records.Empty();

	const FString FilePath = GetCacheFilePath();
	if (!FPaths::FileExists(FilePath))
	{
		return;
	}

	FString JsonStr;
	if (!FFileHelper::LoadFileToString(JsonStr, *FilePath))
	{
		return;
	}

	TSharedPtr<FJsonObject> RootObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		return;
	}

	for (const auto& Pair : RootObj->Values)
	{
		const FString& SequencePath = Pair.Key;
		const TSharedPtr<FJsonObject> SeqObj = Pair.Value->AsObject();
		if (!SeqObj.IsValid())
		{
			continue;
		}

		FCachedSequenceRecord Record;
		Record.LastDetectedTime = SeqObj->GetStringField(TEXT("LastDetectedTime"));

		const TArray<TSharedPtr<FJsonValue>>* EntriesArr;
		if (SeqObj->TryGetArrayField(TEXT("Entries"), EntriesArr))
		{
			for (const TSharedPtr<FJsonValue>& EntryVal : *EntriesArr)
			{
				const TSharedPtr<FJsonObject> EntryObj = EntryVal->AsObject();
				if (!EntryObj.IsValid())
				{
					continue;
				}

				FCachedMissingEntry Entry;
				Entry.MissingPackage = EntryObj->GetStringField(TEXT("MissingPackage"));
				Entry.LocationInfo = EntryObj->GetStringField(TEXT("LocationInfo"));
				Entry.Description = EntryObj->GetStringField(TEXT("Description"));
				Record.Entries.Add(MoveTemp(Entry));
			}
		}

		Records.Add(SequencePath, MoveTemp(Record));
	}
}

void FMissingRefCache::SaveToDisk()
{
	const FString FilePath = GetCacheFilePath();

	// Ensure directory exists
	FString DirPath = FPaths::GetPath(FilePath);
	IFileManager::Get().MakeDirectory(*DirPath, true);

	TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();

	for (const auto& Pair : Records)
	{
		const FString& SequencePath = Pair.Key;
		const FCachedSequenceRecord& Record = Pair.Value;

		TSharedRef<FJsonObject> SeqObj = MakeShared<FJsonObject>();
		SeqObj->SetStringField(TEXT("LastDetectedTime"), Record.LastDetectedTime);

		TArray<TSharedPtr<FJsonValue>> EntriesArr;
		for (const FCachedMissingEntry& Entry : Record.Entries)
		{
			TSharedRef<FJsonObject> EntryObj = MakeShared<FJsonObject>();
			EntryObj->SetStringField(TEXT("MissingPackage"), Entry.MissingPackage);
			EntryObj->SetStringField(TEXT("LocationInfo"), Entry.LocationInfo);
			EntryObj->SetStringField(TEXT("Description"), Entry.Description);
			EntriesArr.Add(MakeShared<FJsonValueObject>(EntryObj));
		}
		SeqObj->SetArrayField(TEXT("Entries"), EntriesArr);

		RootObj->SetObjectField(SequencePath, SeqObj);
	}

	FString JsonStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
	FJsonSerializer::Serialize(RootObj, Writer);

	FFileHelper::SaveStringToFile(JsonStr, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void FMissingRefCache::ClearAll()
{
	Records.Empty();

	const FString FilePath = GetCacheFilePath();
	if (FPaths::FileExists(FilePath))
	{
		IFileManager::Get().Delete(*FilePath);
	}
}

void FMissingRefCache::ClearForSequence(const FString& SequencePath)
{
	Records.Remove(SequencePath);
}

void FMissingRefCache::OverwriteCache(
	const FString& SequencePath,
	const TArray<FLevelSequenceCheckResult>& CurrentResults)
{
	// Build the new record from current results — completely replaces any old record
	FCachedSequenceRecord NewRecord;

	// Get current timestamp
	FDateTime Now = FDateTime::UtcNow();
	NewRecord.LastDetectedTime = Now.ToString(TEXT("%Y-%m-%dT%H:%M:%S"));

	for (const FLevelSequenceCheckResult& Result : CurrentResults)
	{
		FCachedMissingEntry Entry;
		// Use MissingPackagePath if available (the actual package path like /Game/Foo/Bar),
		// otherwise fall back to empty string
		Entry.MissingPackage = Result.MissingPackagePath;
		Entry.LocationInfo = Result.LocationInfo.ToString();
		Entry.Description = Result.Description.ToString();
		NewRecord.Entries.Add(MoveTemp(Entry));
	}

	// Update the in-memory records
	if (NewRecord.Entries.Num() > 0)
	{
		Records.Add(SequencePath, MoveTemp(NewRecord));
	}
	else
	{
		// No entries at all — remove the sequence from cache
		Records.Remove(SequencePath);
	}

	// Persist to disk
	SaveToDisk();
}

#undef LOCTEXT_NAMESPACE
