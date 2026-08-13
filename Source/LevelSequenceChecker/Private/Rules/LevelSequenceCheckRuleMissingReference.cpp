#include "Rules/LevelSequenceCheckRuleMissingReference.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieScenePossessable.h"
#include "MovieSceneBindingReferences.h"
#include "Bindings/MovieSceneSpawnableBinding.h"
#include "Sections/MovieSceneSubSection.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Sections/MovieSceneParticleSection.h"
#include "Channels/MovieSceneObjectPathChannel.h"
#include "Sections/MovieSceneObjectPropertySection.h"
#include "Sections/MovieScenePrimitiveMaterialSection.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"
#include "LevelSequenceCheckResult.h"
#include "LevelSequenceCheckerModule.h"
#include "MissingRefCache.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "UObject/UnrealType.h"
#include "UObject/TextProperty.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "LevelSequenceCheckRuleMissingReference"

ULevelSequenceCheckRuleMissingReference::ULevelSequenceCheckRuleMissingReference()
{
	bEnabled = true;
}

FName ULevelSequenceCheckRuleMissingReference::GetRuleId() const
{
	return FName("R-01");
}

FText ULevelSequenceCheckRuleMissingReference::GetRuleName() const
{
	return LOCTEXT("RuleName", "丢失资源引用");
}

FName ULevelSequenceCheckRuleMissingReference::GetCategory() const
{
	return FName("AssetNorm");
}

// ============================================================================
// ExecuteCheck — 三阶段入口 + 缓存合并
// ============================================================================
void ULevelSequenceCheckRuleMissingReference::ExecuteCheck(ULevelSequence* InSequence, TArray<FLevelSequenceCheckResult>& OutResults)
{
	if (!IsValid(InSequence)) { return; }
	
	UMovieScene* MovieScene = InSequence->GetMovieScene();
	if (!MovieScene) { return; }

	const UMovieScene* ConstMovieScene = MovieScene;

	// ---- Phase 1: 通过 AssetRegistry 收集丢失包集合 ----
	TSet<FString> MissingPackages = CollectMissingDependencies(InSequence);
	TSet<FString> ConsumedPackages;

	// ---- Phase 2: 层级遍历 + 交叉验证 ----
	// Traverse global tracks
	for (UMovieSceneTrack* Track : MovieScene->GetTracks())
	{
		if (!Track) { continue; }
		for (UMovieSceneSection* Section : Track->GetAllSections())
		{
			if (!Section) { continue; }
			FText ContextStr = FText::Format(
				FText::FromString(TEXT("全局变量 -> 轨道: {0} -> 段落: {1}")),
				Track->GetDisplayName(),
				FText::FromString(Section->GetName()));

			CheckSectionByType(Section, ContextStr, MissingPackages, ConsumedPackages, OutResults);
			CheckSoftReferences(Section, ContextStr, OutResults);
		}
	}

	// Traverse bindings
	const FMovieSceneBindingReferences* BindingRefs = InSequence->GetBindingReferences();

	for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
	{
		FGuid BindingGuid = Binding.GetObjectGuid();

		// 获取绑定名称：Possessable 优先（UE 5.7 中 Spawnable 也有 Possessable 条目）
		FText BindingName;
		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(BindingGuid);
		if (Possessable)
		{
			BindingName = FText::FromString(Possessable->GetName());
		}
		else
		{
			FMovieSceneSpawnable* LegacySpawnable = MovieScene->FindSpawnable(BindingGuid);
			BindingName = LegacySpawnable
				? FText::FromString(LegacySpawnable->GetName())
				: FText::FromString(BindingGuid.ToString());
		}

		// Spawnable template check: UE 5.7 CustomBinding 优先，旧版 Fallback
		bool bIsSpawnable = false;
		UObject* SpawnableTemplate = nullptr;
		FString SpawnableTemplateName;

		if (BindingRefs)
		{
			const UMovieSceneCustomBinding* CustomBinding = BindingRefs->GetCustomBinding(BindingGuid, 0);
			const UMovieSceneSpawnableBindingBase* SpawnableBinding = Cast<UMovieSceneSpawnableBindingBase>(CustomBinding);
			if (SpawnableBinding)
			{
				bIsSpawnable = true;
				SpawnableTemplate = const_cast<UMovieSceneSpawnableBindingBase*>(SpawnableBinding)->GetObjectTemplate();
				SpawnableTemplateName = Possessable ? Possessable->GetName() : BindingGuid.ToString();
			}
		}

		if (!bIsSpawnable)
		{
			FMovieSceneSpawnable* LegacySpawnable = MovieScene->FindSpawnable(BindingGuid);
			if (LegacySpawnable)
			{
				bIsSpawnable = true;
				SpawnableTemplate = LegacySpawnable->GetObjectTemplate();
				SpawnableTemplateName = LegacySpawnable->GetName();
			}
		}

		if (bIsSpawnable)
		{
			FText SpawnableContext = FText::Format(
				FText::FromString(TEXT("生成物模板：{0}")),
				FText::FromString(SpawnableTemplateName));
			CheckSpawnableTemplate(SpawnableTemplate, SpawnableTemplateName, SpawnableContext, MissingPackages, ConsumedPackages, OutResults);
		}

		// Binding tracks
		for (UMovieSceneTrack* Track : Binding.GetTracks())
		{
			if (!Track) { continue; }
			for (UMovieSceneSection* Section : Track->GetAllSections())
			{
				if (!Section) { continue; }
				FText ContextStr = FText::Format(
					FText::FromString(TEXT("对象: {0} -> 轨道: {1} -> 段落: {2}")),
					BindingName,
					Track->GetDisplayName(),
					FText::FromString(Section->GetName()));

				CheckSectionByType(Section, ContextStr, MissingPackages, ConsumedPackages, OutResults);
				CheckSoftReferences(Section, ContextStr, OutResults);
			}
		}
	}

	// ---- Phase 3: 补报未被 Phase 2 消费的丢失包（带 FReferenceFinder 内存验证） ----
	ReportUnconsumedMissingPackages(InSequence, MissingPackages, ConsumedPackages, OutResults);

	// ---- Phase 4: 覆写持久化缓存 ----
	FString SequencePath = InSequence->GetPathName();
	FLevelSequenceCheckerModule::Get().GetMissingRefCache().OverwriteCache(SequencePath, OutResults);
}

// ============================================================================
// Phase 1: 通过 AssetRegistry 收集丢失包集合
// ============================================================================
TSet<FString> ULevelSequenceCheckRuleMissingReference::CollectMissingDependencies(ULevelSequence* InSequence)
{
	TSet<FString> MissingPackages;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FName PackageName = InSequence->GetOutermost()->GetFName();
	TArray<FName> Dependencies;
	AssetRegistry.GetDependencies(PackageName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);

	for (const FName& Dep : Dependencies)
	{
		FString DepPackageStr = Dep.ToString();
		// 只检查 /Game/ 下的包（项目资产），跳过引擎内置
		if (DepPackageStr.StartsWith(TEXT("/Game/")))
		{
			if (!FPackageName::DoesPackageExist(DepPackageStr))
			{
				MissingPackages.Add(DepPackageStr);
			}
		}
	}

	return MissingPackages;
}

// ============================================================================
// Phase 2A: 对特定 Section 类型做空值特检 + 交叉验证
// ============================================================================
void ULevelSequenceCheckRuleMissingReference::CheckSectionByType(
	UMovieSceneSection* Section,
	const FText& LocationContext,
	const TSet<FString>& MissingPackages,
	TSet<FString>& ConsumedPackages,
	TArray<FLevelSequenceCheckResult>& OutResults)
{
	if (!Section) { return; }

	// --- UMovieSceneSubSection ---
	if (UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section))
	{
		if (!SubSection->GetSequence())
		{
			// 检查 MissingPackages 中是否有匹配的序列类包
			bool bHasEvidence = false;
			for (const FString& Pkg : MissingPackages)
			{
				if (Pkg.Contains(TEXT("Sequence")) || Pkg.Contains(TEXT("Cinematic")) || Pkg.Contains(TEXT("LevelSequence")))
				{
					ConsumedPackages.Add(Pkg);
					bHasEvidence = true;
				}
			}

			FLevelSequenceCheckResult Result;
			Result.RuleId = GetRuleId();
			Result.RuleName = GetRuleName();
			Result.Severity = ELevelSequenceCheckSeverity::Error;
			Result.Description = LOCTEXT("SubSectionNull", "子序列引用为空（关联资产可能已丢失）");
			Result.LocationInfo = LocationContext;
			OutResults.Add(MoveTemp(Result));
		}
		return;
	}

	// --- UMovieSceneAudioSection ---
	if (UMovieSceneAudioSection* AudioSection = Cast<UMovieSceneAudioSection>(Section))
	{
		if (!AudioSection->GetSound())
		{
			// 检查 MissingPackages 中是否有匹配的音频类包
			bool bHasEvidence = false;
			for (const FString& Pkg : MissingPackages)
			{
				if (Pkg.Contains(TEXT("Sound")) || Pkg.Contains(TEXT("Audio")) || Pkg.Contains(TEXT("Music")))
				{
					ConsumedPackages.Add(Pkg);
					bHasEvidence = true;
				}
			}

			FLevelSequenceCheckResult Result;
			Result.RuleId = GetRuleId();
			Result.RuleName = GetRuleName();
			Result.Severity = bHasEvidence ? ELevelSequenceCheckSeverity::Error : ELevelSequenceCheckSeverity::Warning;
			Result.Description = LOCTEXT("AudioSectionNull", "音频引用为空（关联资产可能已丢失）");
			Result.LocationInfo = LocationContext;
			OutResults.Add(MoveTemp(Result));
		}
		return;
	}

	// --- UMovieSceneSkeletalAnimationSection ---
	if (UMovieSceneSkeletalAnimationSection* AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(Section))
	{
		if (!AnimSection->Params.Animation)
		{
			// 检查 MissingPackages 中是否有匹配的动画类包
			bool bHasEvidence = false;
			for (const FString& Pkg : MissingPackages)
			{
				if (Pkg.Contains(TEXT("Anim")) || Pkg.Contains(TEXT("Animation")))
				{
					ConsumedPackages.Add(Pkg);
					bHasEvidence = true;
				}
			}

			FLevelSequenceCheckResult Result;
			Result.RuleId = GetRuleId();
			Result.RuleName = GetRuleName();
			Result.Severity = bHasEvidence ? ELevelSequenceCheckSeverity::Error : ELevelSequenceCheckSeverity::Warning;
			Result.Description = LOCTEXT("SkeletalAnimSectionNull", "动画引用为空（关联资产可能已丢失）");
			Result.LocationInfo = LocationContext;
			OutResults.Add(MoveTemp(Result));
		}
		return;
	}

	// --- UMovieSceneObjectPropertySection (新增: ObjectPath Channel 遍历) ---
	if (UMovieSceneObjectPropertySection* ObjPropSection = Cast<UMovieSceneObjectPropertySection>(Section))
	{
		CheckObjectPathChannel(ObjPropSection->ObjectChannel, LocationContext, MissingPackages, ConsumedPackages, OutResults);
		return;
	}

	// --- UMovieScenePrimitiveMaterialSection (新增: ObjectPath Channel 遍历) ---
	if (UMovieScenePrimitiveMaterialSection* MatSection = Cast<UMovieScenePrimitiveMaterialSection>(Section))
	{
		CheckObjectPathChannel(MatSection->MaterialChannel, LocationContext, MissingPackages, ConsumedPackages, OutResults);
		return;
	}

	// --- UMovieSceneParticleSection ---
	if (Section->IsA<UMovieSceneParticleSection>())
	{
		CheckSectionByReflection(Section, LocationContext, MissingPackages, ConsumedPackages, OutResults);
		return;
	}

	// --- 未被 Phase 2A 覆盖的 Section 类型 → 进入 Phase 2B ---
	CheckSectionByReflection(Section, LocationContext, MissingPackages, ConsumedPackages, OutResults);
}

// ============================================================================
// Phase 2A extension: FMovieSceneObjectPathChannel 特检
// ============================================================================
void ULevelSequenceCheckRuleMissingReference::CheckObjectPathChannel(
	const FMovieSceneObjectPathChannel& Channel,
	const FText& LocationContext,
	const TSet<FString>& MissingPackages,
	TSet<FString>& ConsumedPackages,
	TArray<FLevelSequenceCheckResult>& OutResults)
{
	// Deduplicate: track which missing packages have already been reported for this section
	TSet<FString> ReportedPackages;

	auto CheckSinglePath = [&](const FSoftObjectPath& Path)
	{
		if (!Path.IsValid()) { return; }

		FString PkgName = Path.GetLongPackageName();
		if (!PkgName.IsEmpty() && MissingPackages.Contains(PkgName) && !ReportedPackages.Contains(PkgName))
		{
			ReportedPackages.Add(PkgName);

			FLevelSequenceCheckResult Result;
			Result.RuleId = GetRuleId();
			Result.RuleName = GetRuleName();
			Result.Severity = ELevelSequenceCheckSeverity::Error;
			Result.Description = FText::Format(
				LOCTEXT("ObjectPathChannelMissing", "ObjectPathChannel 引用丢失包: {0}"),
				FText::FromString(PkgName));
			Result.LocationInfo = LocationContext;
			Result.MissingPackagePath = PkgName;
			OutResults.Add(MoveTemp(Result));
			ConsumedPackages.Add(PkgName);
		}
	};

	// Check default value
	CheckSinglePath(Channel.GetDefault().GetSoftPtr().ToSoftObjectPath());

	// Check all key values
	TMovieSceneChannelData<const FMovieSceneObjectPathChannelKeyValue> ChannelData = Channel.GetData();
	TArrayView<const FMovieSceneObjectPathChannelKeyValue> Values = ChannelData.GetValues();

	for (int32 i = 0; i < Values.Num(); ++i)
	{
		CheckSinglePath(Values[i].GetSoftPtr().ToSoftObjectPath());
	}
}

// ============================================================================
// Phase 2B: 泛化反射兜底（增强：支持结构体/数组嵌套）
// ============================================================================
void ULevelSequenceCheckRuleMissingReference::CheckSectionByReflection(
	UMovieSceneSection* Section,
	const FText& LocationContext,
	const TSet<FString>& MissingPackages,
	TSet<FString>& ConsumedPackages,
	TArray<FLevelSequenceCheckResult>& OutResults)
{
	if (!Section || !IsValid(Section) || MissingPackages.IsEmpty()) { return; }

	// 跳过 placeholder/reinst 类——内存布局不可靠，反射遍历不安全
	FString ClassName = Section->GetClass()->GetName();
	if (ClassName.StartsWith(TEXT("PLACEHOLDER_")) || ClassName.StartsWith(TEXT("REINST_"))) { return; }

	for (TFieldIterator<FProperty> PropIt(Section->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		CheckPropertyRecursive(Property, Section, LocationContext, MissingPackages, ConsumedPackages, OutResults, 0);
	}
}

void ULevelSequenceCheckRuleMissingReference::CheckPropertyRecursive(
	FProperty* Property,
	void* ContainerPtr,
	const FText& LocationContext,
	const TSet<FString>& MissingPackages,
	TSet<FString>& ConsumedPackages,
	TArray<FLevelSequenceCheckResult>& OutResults,
	int32 Depth)
{
	if (!Property || !ContainerPtr || Depth > 2) { return; }

	// FObjectProperty — 检查 nullptr 的硬引用
	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		UObject* ReferencedObj = ObjProp->GetPropertyValue_InContainer(ContainerPtr);
		if (!ReferencedObj)
		{
			FString PropertyClassName = ObjProp->PropertyClass ? ObjProp->PropertyClass->GetName() : TEXT("");
			FString PropertyClassPath = ObjProp->PropertyClass ? ObjProp->PropertyClass->GetPathName() : TEXT("");

			bool bMatched = false;
			for (const FString& Pkg : MissingPackages)
			{
				if (Pkg.Contains(PropertyClassName) || PropertyClassPath.Contains(Pkg))
				{
					bMatched = true;
					ConsumedPackages.Add(Pkg);
					break;
				}
			}

			if (bMatched)
			{
				FLevelSequenceCheckResult Result;
				Result.RuleId = GetRuleId();
				Result.RuleName = GetRuleName();
				Result.Severity = ELevelSequenceCheckSeverity::Error;
				Result.Description = FText::Format(
					LOCTEXT("ReflectionNullMatched", "属性 '{0}' 引用为空，且存在关联丢失包（期望类型: {1}）"),
					FText::FromString(Property->GetName()),
					FText::FromString(PropertyClassName));
				Result.LocationInfo = LocationContext;
				OutResults.Add(MoveTemp(Result));
			}
		}
		return;
	}

	// FStructProperty — 递归进入结构体内部
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		// Skip FSoftObjectPath (handled by CheckSoftReferences)
		if (StructProp->Struct == TBaseStructure<FSoftObjectPath>::Get())
		{
			return;
		}

		void* StructPtr = StructProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		for (TFieldIterator<FProperty> InnerIt(StructProp->Struct); InnerIt; ++InnerIt)
		{
			CheckPropertyRecursive(*InnerIt, StructPtr, LocationContext, MissingPackages, ConsumedPackages, OutResults, Depth + 1);
		}
		return;
	}

	// FArrayProperty — 若内部元素为 FStructProperty，取第一层内的 FObjectProperty
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner);
		if (InnerStructProp)
		{
			FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(ContainerPtr));
			for (int32 i = 0; i < ArrayHelper.Num(); ++i)
			{
				void* ElementPtr = ArrayHelper.GetRawPtr(i);
				for (TFieldIterator<FProperty> InnerIt(InnerStructProp->Struct); InnerIt; ++InnerIt)
				{
					CheckPropertyRecursive(*InnerIt, ElementPtr, LocationContext, MissingPackages, ConsumedPackages, OutResults, Depth + 1);
				}
			}
		}
		// If inner is FObjectProperty, iterate and check
		else if (FObjectProperty* InnerObjProp = CastField<FObjectProperty>(ArrayProp->Inner))
		{
			FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(ContainerPtr));
			for (int32 i = 0; i < ArrayHelper.Num(); ++i)
			{
				void* RawPtr = ArrayHelper.GetRawPtr(i);
				if (!RawPtr) { continue; }
				UObject** ObjPtr = reinterpret_cast<UObject**>(RawPtr);
				if (!*ObjPtr)
				{
					FString PropertyClassName = InnerObjProp->PropertyClass ? InnerObjProp->PropertyClass->GetName() : TEXT("");
					bool bMatched = false;
					for (const FString& Pkg : MissingPackages)
					{
						if (Pkg.Contains(PropertyClassName))
						{
							bMatched = true;
							ConsumedPackages.Add(Pkg);
							break;
						}
					}
					if (bMatched)
					{
						FLevelSequenceCheckResult Result;
						Result.RuleId = GetRuleId();
						Result.RuleName = GetRuleName();
						Result.Severity = ELevelSequenceCheckSeverity::Error;
					Result.Description = FText::Format(
						LOCTEXT("ArrayElementNullMatched", "数组属性 '{0}' 元素引用为空，且存在关联丢失包（期望类型: {1}）"),
						FText::FromString(Property->GetName()),
						FText::FromString(PropertyClassName));
						Result.LocationInfo = LocationContext;
						OutResults.Add(MoveTemp(Result));
					}
				}
			}
		}
	}
}

// ============================================================================
// Spawnable ObjectTemplate 空值检查（优化匹配策略）
// ============================================================================
void ULevelSequenceCheckRuleMissingReference::CheckSpawnableTemplate(
	UObject* Template,
	const FString& SpawnableName,
	const FText& LocationContext,
	const TSet<FString>& MissingPackages,
	TSet<FString>& ConsumedPackages,
	TArray<FLevelSequenceCheckResult>& OutResults)
{
	// ObjectTemplate 为空 — 原始蓝图/资产已被删除
	if (!Template)
	{
		// Strategy: Match Spawnable name against missing package short names
		FString MatchedPackagePath;
		for (const FString& Pkg : MissingPackages)
		{
			FString PkgShortName = FPaths::GetCleanFilename(Pkg);
			if (PkgShortName.Contains(SpawnableName) || SpawnableName.Contains(PkgShortName))
			{
				ConsumedPackages.Add(Pkg);
				if (MatchedPackagePath.IsEmpty())
				{
					MatchedPackagePath = Pkg;
				}
			}
		}

		FText DescText;
		if (!MatchedPackagePath.IsEmpty())
		{
			DescText = FText::Format(
				LOCTEXT("SpawnableTemplateNullMatched", "生成物模板为空（匹配丢失包: {1}）: {0}"),
				FText::FromString(SpawnableName),
				FText::FromString(MatchedPackagePath));
		}
		else
		{
			DescText = FText::Format(
				LOCTEXT("SpawnableTemplateNull", "生成物模板为空（原始蓝图/资产可能已丢失）: {0}"),
				FText::FromString(SpawnableName));
		}

		FLevelSequenceCheckResult Result;
		Result.RuleId = GetRuleId();
		Result.RuleName = GetRuleName();
		Result.Severity = ELevelSequenceCheckSeverity::Error;
		Result.Description = DescText;
		Result.LocationInfo = LocationContext;
		if (!MatchedPackagePath.IsEmpty())
		{
			Result.MissingPackagePath = MatchedPackagePath;
		}
		OutResults.Add(MoveTemp(Result));
	}
	// ObjectTemplate 存在但类是 placeholder/reinst —— 原始蓝图已被删除
	else
	{
		FString TemplateClassName = Template->GetClass()->GetName();
		if (TemplateClassName.StartsWith(TEXT("PLACEHOLDER_")) || TemplateClassName.StartsWith(TEXT("REINST_")))
		{
			FString MatchedPackagePath;
			for (const FString& Pkg : MissingPackages)
			{
				FString PkgShortName = FPaths::GetCleanFilename(Pkg);
				if (PkgShortName.Contains(SpawnableName) || SpawnableName.Contains(PkgShortName))
				{
					ConsumedPackages.Add(Pkg);
					if (MatchedPackagePath.IsEmpty())
					{
						MatchedPackagePath = Pkg;
					}
				}
			}

			FText DescText = FText::Format(
				LOCTEXT("SpawnableTemplatePlaceholder", "生成物模板类已失效（原始蓝图已被强制删除）: {0} -> {1}"),
				FText::FromString(SpawnableName),
				FText::FromString(TemplateClassName));

			FLevelSequenceCheckResult Result;
			Result.RuleId = GetRuleId();
			Result.RuleName = GetRuleName();
			Result.Severity = ELevelSequenceCheckSeverity::Error;
			Result.Description = DescText;
			Result.LocationInfo = LocationContext;
			if (!MatchedPackagePath.IsEmpty())
			{
				Result.MissingPackagePath = MatchedPackagePath;
			}
			OutResults.Add(MoveTemp(Result));
		}
	}
}

// ============================================================================
// 软引用路径检查（保留原有逻辑）
// ============================================================================
void ULevelSequenceCheckRuleMissingReference::CheckSoftReferences(
	UObject* Obj,
	const FText& LocationContext,
	TArray<FLevelSequenceCheckResult>& OutResults)
{
	if (!Obj) { return; }

	// 跳过 placeholder/reinst 类——内存布局不可靠
	FString ClassName = Obj->GetClass()->GetName();
	if (ClassName.StartsWith(TEXT("PLACEHOLDER_")) || ClassName.StartsWith(TEXT("REINST_"))) { return; }

	for (TFieldIterator<FProperty> PropIt(Obj->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		// TSoftObjectPtr / FSoftObjectPath
		if (FSoftObjectProperty* SoftObjectProp = CastField<FSoftObjectProperty>(Property))
		{
			FSoftObjectPtr SoftPtr = SoftObjectProp->GetPropertyValue_InContainer(Obj);
			VerifySoftObjectPath(SoftPtr.ToSoftObjectPath(), LocationContext, OutResults);
		}
		else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			// Some properties embed FSoftObjectPath as struct fields
			if (StructProp->Struct == TBaseStructure<FSoftObjectPath>::Get())
			{
				if (const FSoftObjectPath* Path = StructProp->ContainerPtrToValuePtr<FSoftObjectPath>(Obj))
				{
					VerifySoftObjectPath(*Path, LocationContext, OutResults);
				}
			}
		}
	}
}

// ============================================================================
// Helper: 验证软引用路径
// ============================================================================
void ULevelSequenceCheckRuleMissingReference::VerifySoftObjectPath(const FSoftObjectPath& Path, const FText& Context, TArray<FLevelSequenceCheckResult>& OutResults)
{
	if (!Path.IsValid()) { return; }
	
	FString PackageName = Path.GetLongPackageName();
	
	if (!PackageName.IsEmpty() && !FPackageName::DoesPackageExist(PackageName))
	{
		FLevelSequenceCheckResult Result;
		Result.RuleId = GetRuleId();
		Result.RuleName = GetRuleName();
		Result.Severity = ELevelSequenceCheckSeverity::Error;
		Result.Description = FText::Format(
			LOCTEXT("SoftRefMissing", "引用的资产无法解析: {0}"),
			FText::FromString(Path.ToString())
		);
		Result.LocationInfo = Context;
		Result.MissingPackagePath = PackageName;
		OutResults.Add(MoveTemp(Result));
	}
}

// ============================================================================
// Phase 3: 补报未被 Phase 2 消费的丢失包
// ============================================================================
void ULevelSequenceCheckRuleMissingReference::ReportUnconsumedMissingPackages(
	ULevelSequence* InSequence,
	const TSet<FString>& MissingPackages,
	const TSet<FString>& ConsumedPackages,
	TArray<FLevelSequenceCheckResult>& OutResults)
{
	// 收集所有 unconsumed missing packages
	TSet<FString> UnconsumedPackages;
	for (const FString& Pkg : MissingPackages)
	{
		if (!ConsumedPackages.Contains(Pkg))
		{
			UnconsumedPackages.Add(Pkg);
		}
	}

	if (UnconsumedPackages.IsEmpty()) { return; }

	// 使用 FReferenceFinder 从 ULevelSequence 对象开始扫描内存引用图
	TSet<FString> ReferencedPackagePaths;
	{
		TArray<UObject*> ReferencedObjects;
		FReferenceFinder Collector(ReferencedObjects, nullptr, /*bRequireDirectOuter=*/false, /*bShouldIgnoreArchetype=*/true, /*bSerializeRecursively=*/true, /*bShouldIgnoreTransient=*/true);
		Collector.FindReferences(InSequence);

		for (UObject* RefObj : ReferencedObjects)
		{
			if (RefObj)
			{
				FString RefPkg = RefObj->GetOutermost()->GetName();
				ReferencedPackagePaths.Add(RefPkg);
			}
		}
	}

	// 只报出内存中仍被引用的丢失包；未被引用的说明用户已删除相关 track
	for (const FString& Pkg : UnconsumedPackages)
	{
		if (ReferencedPackagePaths.Contains(Pkg))
		{
			FLevelSequenceCheckResult Result;
			Result.RuleId = GetRuleId();
			Result.RuleName = GetRuleName();
			Result.Severity = ELevelSequenceCheckSeverity::Error;
			Result.Description = FText::Format(
			LOCTEXT("UnconsumedMissingPackage", "资产依赖包丢失（未能定位到具体轨道/段落）: {0}"),
			FText::FromString(Pkg));
		Result.LocationInfo = LOCTEXT("AssetDependency", "资产依赖");
			Result.MissingPackagePath = Pkg;
			OutResults.Add(MoveTemp(Result));
		}
	}
}

// ============================================================================
// UI Customization
// ============================================================================
void ULevelSequenceCheckRuleMissingReference::CustomizeRuleDetails(IDetailCategoryBuilder& CategoryBuilder)
{
	// Add a header row with the rule name
	CategoryBuilder.AddCustomRow(LOCTEXT("RuleHeader", "R-01"))
	.WholeRowContent()
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("RuleTitle", "R-01 丢失资源引用"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
	];

	// Add properties individually using AddExternalObjectProperty.
	TArray<UObject*> ExternalObjects;
	ExternalObjects.Add(this);

	// bEnabled from base class
	CategoryBuilder.AddExternalObjectProperty(ExternalObjects, GET_MEMBER_NAME_CHECKED(ULevelSequenceCheckRuleBase, bEnabled));
}

#undef LOCTEXT_NAMESPACE
