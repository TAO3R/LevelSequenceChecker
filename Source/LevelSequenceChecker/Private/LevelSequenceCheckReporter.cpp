#include "LevelSequenceCheckReporter.h"

#include "LevelSequenceCheckResult.h"
#include "LevelSequence.h"

#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/PackageName.h"

#define LOCTEXT_NAMESPACE "LevelSequenceCheckReporter"

void FLevelSequenceCheckReporter::Report(ULevelSequence* InSequence, const TArray<FLevelSequenceCheckResult>& InResults, bool bHasErrors)
{
	if (!IsValid(InSequence) || InResults.Num() == 0)
	{
		return;
	}

	FMessageLog MessageLog("LevelSequenceChecker");

	// Create a new page for this check run, labeled with the asset name
	const FString AssetPath = InSequence->GetPathName();
	const FText PageLabel = FText::Format(
		LOCTEXT("PageLabel", "检查: {0}"),
		FText::FromString(AssetPath)
	);
	MessageLog.NewPage(PageLabel);

	// Convert each result into a tokenized message with inline clickable asset link
	const FString AssetName = FPaths::GetBaseFilename(AssetPath);

	for (const FLevelSequenceCheckResult& Result : InResults)
	{
		const EMessageSeverity::Type Severity = static_cast<EMessageSeverity::Type>(Result.Severity);

		// Build tokenized message with asset name embedded as a clickable link:
		// "[RuleId][RuleName] 资产 [AssetLink] Description"
		TSharedRef<FTokenizedMessage> TokenizedMsg = FTokenizedMessage::Create(Severity);

		// Prefix: "[S-04][Track 数量超限] 资产 "
		TokenizedMsg->AddToken(FTextToken::Create(
			FText::Format(LOCTEXT("ResultPrefix", "[{0}][{1}] 资产 "),
				FText::FromName(Result.RuleId),
				Result.RuleName)
		));

		// Clickable asset name token
		TokenizedMsg->AddToken(FAssetNameToken::Create(AssetPath, FText::FromString(AssetName)));

		// Suffix: " Description"
		TokenizedMsg->AddToken(FTextToken::Create(
			FText::Format(LOCTEXT("ResultSuffix", " {0}"), Result.Description)
		));

		// Add location info as supplementary token (if present)
		if (!Result.LocationInfo.IsEmpty())
		{
			TokenizedMsg->AddToken(FTextToken::Create(
				FText::Format(LOCTEXT("LocationSuffix", " @ {0}"), Result.LocationInfo)
			));
		}

		MessageLog.AddMessage(TokenizedMsg);
	}

	// Flush and notify — skip the toast when errors exist (modal dialog handles it)
	if (!bHasErrors)
	{
		const FText NotificationMessage = FText::Format(
			LOCTEXT("Notification", "LevelSequence 检查器: 在 {1} 中发现 {0} 个问题"),
			InResults.Num(),
			FText::FromString(FPaths::GetBaseFilename(AssetPath))
		);

		MessageLog.Notify(NotificationMessage, EMessageSeverity::Warning);
	}
}

#undef LOCTEXT_NAMESPACE
