#include "LevelSequenceCheckerSettingsCustomization.h"

#include "LevelSequenceCheckerSettings.h"
#include "LevelSequenceCheckRuleBase.h"
#include "LevelSequenceCheckerModule.h"
#include "MissingRefCache.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "LevelSequenceCheckerSettingsCustomization"

void FLevelSequenceCheckerSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Hide the raw CheckRules array — we render it ourselves grouped by category
	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(ULevelSequenceCheckerSettings, CheckRules));

	// Get the Settings object being customized
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() == 0)
	{
		return;
	}

	ULevelSequenceCheckerSettings* Settings = Cast<ULevelSequenceCheckerSettings>(Objects[0].Get());
	if (!Settings)
	{
		return;
	}
	
	// Force "General" category to appear first with highest priority
	IDetailCategoryBuilder& GeneralCategory = DetailBuilder.EditCategory(
		FName("General"),
		LOCTEXT("CatGeneral", "通用"),
		ECategoryPriority::Important
	);

	// Explicitly add bEnableCheckOnSave as a standard PropertyRow so that
	// the Settings panel search index can match it (RegisterCustomClassLayout
	// may skip implicit properties from the search index).
	GeneralCategory.AddProperty(
		DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULevelSequenceCheckerSettings, bEnableCheckOnSave)));

	// Add "Clear Missing Reference Cache" button
	GeneralCategory.AddCustomRow(LOCTEXT("ClearCacheRow", "Clear Cache"))
	.WholeRowContent()
	.VAlign(VAlign_Center)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 8, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("ClearCacheButton", "清除丢失记录缓存"))
			.ToolTipText(LOCTEXT("ClearCacheTooltip", "清除 R-01 丢失资源引用的持久化缓存记录"))
			.OnClicked_Lambda([]() -> FReply
			{
				FLevelSequenceCheckerModule::Get().GetMissingRefCache().ClearAll();
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("OpenCacheButton", "打开缓存文件"))
			.ToolTipText(LOCTEXT("OpenCacheTooltip", "在系统默认程序中打开 MissingRefCache.json"))
			.OnClicked_Lambda([]() -> FReply
			{
				const FString FilePath = FPaths::ProjectSavedDir() / TEXT("LevelSequenceChecker") / TEXT("MissingRefCache.json");
				const FString AbsPath = FPaths::ConvertRelativePathToFull(FilePath);
				if (FPaths::FileExists(AbsPath))
				{
					FPlatformProcess::LaunchFileInDefaultExternalApplication(*AbsPath);
				}
				else
				{
					const FString DirPath = FPaths::GetPath(AbsPath);
					FPlatformProcess::ExploreFolder(*DirPath);
				}
				return FReply::Handled();
			})
		]
	];

	// Category name mapping: internal FName -> display FText
	struct FCategoryInfo
	{
		FName CategoryName;
		FText DisplayName;
	};

	static const FCategoryInfo CategoryOrder[] = {
		{ FName("AssetNorm"),  LOCTEXT("CatAssetNorm",  "资产规范") },
		{ FName("Structure"),  LOCTEXT("CatStructure",  "MovieScene 结构") },
		{ FName("Binding"),    LOCTEXT("CatBinding",    "Spawnable / Possessable") },
		{ FName("Keyframe"),   LOCTEXT("CatKeyframe",   "关键帧 / 通道") },
	};

	// Pre-create all categories in desired order
	TMap<FName, IDetailCategoryBuilder*> CategoryBuilders;
	for (const FCategoryInfo& CatInfo : CategoryOrder)
	{
		IDetailCategoryBuilder& CatBuilder = DetailBuilder.EditCategory(
			CatInfo.CategoryName,
			CatInfo.DisplayName,
			ECategoryPriority::Default
		);
		CategoryBuilders.Add(CatInfo.CategoryName, &CatBuilder);
	}

	// Iterate rules and let each one draw itself into its category
	for (const TObjectPtr<ULevelSequenceCheckRuleBase>& Rule : Settings->CheckRules)
	{
		if (!IsValid(Rule))
		{
			continue;
		}

		FName CategoryKey = Rule->GetCategory();
		IDetailCategoryBuilder** FoundCat = CategoryBuilders.Find(CategoryKey);

		// If the rule's category isn't in our known list, create it on the fly
		IDetailCategoryBuilder* CatBuilder = FoundCat ? *FoundCat : &DetailBuilder.EditCategory(CategoryKey);

		// Let the rule customize its own UI within this category
		Rule->CustomizeRuleDetails(*CatBuilder);
	}
}

#undef LOCTEXT_NAMESPACE
