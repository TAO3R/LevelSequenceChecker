#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceCheckResult.generated.h"

/** Severity level for a check result, with values corresponding to EMessageSeverity::Type */
UENUM()
enum class ELevelSequenceCheckSeverity : uint8
{
	Error = 1,
	PerformanceWarning = 2,
	Warning = 3,
	Info = 4,
};

/**
 * A single check result produced by a rule.
 * Contains all information needed to display a problem in the Message Log.
 */
USTRUCT()
struct FLevelSequenceCheckResult
{
	GENERATED_BODY()

	/** Unique rule identifier (e.g. "S-01") */
	UPROPERTY(VisibleAnywhere, Category = "Result")
	FName RuleId;

	/** Human-readable rule name */
	UPROPERTY(VisibleAnywhere, Category = "Result")
	FText RuleName;

	/** Short description of the problem found */
	UPROPERTY(VisibleAnywhere, Category = "Result")
	FText Description;

	/** Location within the asset (Track name / Section / Binding / Frame, etc.) */
	UPROPERTY(VisibleAnywhere, Category = "Result")
	FText LocationInfo;

	/** Severity of the result */
	UPROPERTY(VisibleAnywhere, Category = "Result")
	ELevelSequenceCheckSeverity Severity = ELevelSequenceCheckSeverity::Warning;

	/**
	 * Optional: the missing package path associated with this result (e.g. "/Game/Foo/Bar").
	 * Used by the MissingRefCache to correctly identify which package is missing,
	 * separate from LocationInfo which describes the track/section location.
	 */
	FString MissingPackagePath;

	FLevelSequenceCheckResult() = default;

	FLevelSequenceCheckResult(
		FName InRuleId,
		const FText& InRuleName,
		const FText& InDescription,
		const FText& InLocationInfo,
		ELevelSequenceCheckSeverity InSeverity = ELevelSequenceCheckSeverity::Warning
	)
		: RuleId(InRuleId)
		, RuleName(InRuleName)
		, Description(InDescription)
		, LocationInfo(InLocationInfo)
		, Severity(InSeverity)
	{}
};
