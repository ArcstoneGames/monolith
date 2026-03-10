#include "MonolithPoseSearchActions.h"
#include "MonolithAssetUtils.h"
#include "MonolithParamSchema.h"

#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Editor.h"

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void FMonolithPoseSearchActions::RegisterActions(FMonolithToolRegistry& Registry)
{
	Registry.RegisterAction(TEXT("animation"), TEXT("get_pose_search_schema"),
		TEXT("Get PoseSearch schema configuration including skeleton, sample rate, and channels"),
		FMonolithActionHandler::CreateStatic(&HandleGetPoseSearchSchema),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("PoseSearchSchema asset path"))
			.Build());

	Registry.RegisterAction(TEXT("animation"), TEXT("get_pose_search_database"),
		TEXT("Get PoseSearch database contents including schema reference and animation asset list"),
		FMonolithActionHandler::CreateStatic(&HandleGetPoseSearchDatabase),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("PoseSearchDatabase asset path"))
			.Build());

	Registry.RegisterAction(TEXT("animation"), TEXT("add_database_sequence"),
		TEXT("Add an animation asset to a PoseSearch database"),
		FMonolithActionHandler::CreateStatic(&HandleAddDatabaseSequence),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("PoseSearchDatabase asset path"))
			.Required(TEXT("anim_path"), TEXT("string"), TEXT("Animation asset to add"))
			.Optional(TEXT("enabled"), TEXT("boolean"), TEXT("Enable for search (default true)"))
			.Build());

	Registry.RegisterAction(TEXT("animation"), TEXT("remove_database_sequence"),
		TEXT("Remove an animation asset from a PoseSearch database by index"),
		FMonolithActionHandler::CreateStatic(&HandleRemoveDatabaseSequence),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("PoseSearchDatabase asset path"))
			.Required(TEXT("sequence_index"), TEXT("integer"), TEXT("Index of the animation asset to remove"))
			.Build());

	Registry.RegisterAction(TEXT("animation"), TEXT("get_database_stats"),
		TEXT("Get PoseSearch database statistics including sequence count, schema, and search index info"),
		FMonolithActionHandler::CreateStatic(&HandleGetDatabaseStats),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("PoseSearchDatabase asset path"))
			.Build());
}

// ---------------------------------------------------------------------------
// get_pose_search_schema
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithPoseSearchActions::HandleGetPoseSearchSchema(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UPoseSearchSchema* Schema = FMonolithAssetUtils::LoadAssetByPath<UPoseSearchSchema>(AssetPath);
	if (!Schema)
		return FMonolithActionResult::Error(FString::Printf(TEXT("PoseSearchSchema not found: %s"), *AssetPath));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetNumberField(TEXT("sample_rate"), Schema->SampleRate);

	// Skeletons
	const TArray<FPoseSearchRoledSkeleton>& RoledSkeletons = Schema->GetRoledSkeletons();
	if (RoledSkeletons.Num() > 0)
	{
		const FPoseSearchRoledSkeleton& DefaultSkeleton = RoledSkeletons[0];
		if (DefaultSkeleton.Skeleton)
		{
			Root->SetStringField(TEXT("skeleton"), DefaultSkeleton.Skeleton->GetPathName());
		}

		// All roled skeletons
		TArray<TSharedPtr<FJsonValue>> SkeletonArray;
		for (int32 i = 0; i < RoledSkeletons.Num(); ++i)
		{
			TSharedPtr<FJsonObject> SkelObj = MakeShared<FJsonObject>();
			SkelObj->SetNumberField(TEXT("index"), i);
			SkelObj->SetStringField(TEXT("role"), RoledSkeletons[i].Role.ToString());
			if (RoledSkeletons[i].Skeleton)
			{
				SkelObj->SetStringField(TEXT("skeleton"), RoledSkeletons[i].Skeleton->GetPathName());
			}
			// MirrorDataTable omitted — forward-declared type, not worth including full header
			SkeletonArray.Add(MakeShared<FJsonValueObject>(SkelObj));
		}
		Root->SetArrayField(TEXT("skeletons"), SkeletonArray);
	}

	// Channels
	TConstArrayView<TObjectPtr<UPoseSearchFeatureChannel>> Channels = Schema->GetChannels();
	TArray<TSharedPtr<FJsonValue>> ChannelArray;
	for (int32 i = 0; i < Channels.Num(); ++i)
	{
		const UPoseSearchFeatureChannel* Channel = Channels[i];
		if (!Channel) continue;

		TSharedPtr<FJsonObject> ChObj = MakeShared<FJsonObject>();
		ChObj->SetNumberField(TEXT("index"), i);
		ChObj->SetStringField(TEXT("type"), Channel->GetClass()->GetName());
		ChObj->SetNumberField(TEXT("cardinality"), Channel->GetChannelCardinality());
		ChObj->SetNumberField(TEXT("data_offset"), Channel->GetChannelDataOffset());
		ChannelArray.Add(MakeShared<FJsonValueObject>(ChObj));
	}
	Root->SetArrayField(TEXT("channels"), ChannelArray);
	Root->SetNumberField(TEXT("schema_cardinality"), Schema->SchemaCardinality);

	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// get_pose_search_database
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithPoseSearchActions::HandleGetPoseSearchDatabase(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UPoseSearchDatabase* Database = FMonolithAssetUtils::LoadAssetByPath<UPoseSearchDatabase>(AssetPath);
	if (!Database)
		return FMonolithActionResult::Error(FString::Printf(TEXT("PoseSearchDatabase not found: %s"), *AssetPath));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);

	// Schema reference
	if (Database->Schema)
	{
		Root->SetStringField(TEXT("schema"), Database->Schema->GetPathName());
	}

	// Animation assets
	const int32 NumAssets = Database->GetNumAnimationAssets();
	Root->SetNumberField(TEXT("sequence_count"), NumAssets);

	TArray<TSharedPtr<FJsonValue>> SeqArray;
	for (int32 i = 0; i < NumAssets; ++i)
	{
		const FPoseSearchDatabaseAnimationAsset* AnimAsset = Database->GetDatabaseAnimationAsset(i);
		if (!AnimAsset) continue;

		TSharedPtr<FJsonObject> SeqObj = MakeShared<FJsonObject>();
		SeqObj->SetNumberField(TEXT("index"), i);

		if (UObject* Asset = AnimAsset->GetAnimationAsset())
		{
			SeqObj->SetStringField(TEXT("animation"), Asset->GetPathName());
			SeqObj->SetStringField(TEXT("asset_class"), Asset->GetClass()->GetName());
		}

#if WITH_EDITORONLY_DATA
		SeqObj->SetBoolField(TEXT("enabled"), AnimAsset->IsEnabled());
		FFloatInterval Range = AnimAsset->GetSamplingRange();
		SeqObj->SetNumberField(TEXT("sampling_range_start"), Range.Min);
		SeqObj->SetNumberField(TEXT("sampling_range_end"), Range.Max);
		SeqObj->SetStringField(TEXT("mirror_option"),
			AnimAsset->GetMirrorOption() == EPoseSearchMirrorOption::UnmirroredOnly ? TEXT("UnmirroredOnly") :
			AnimAsset->GetMirrorOption() == EPoseSearchMirrorOption::MirroredOnly ? TEXT("MirroredOnly") :
			TEXT("UnmirroredAndMirrored"));
#endif

		SeqArray.Add(MakeShared<FJsonValueObject>(SeqObj));
	}
	Root->SetArrayField(TEXT("sequences"), SeqArray);

	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// add_database_sequence
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithPoseSearchActions::HandleAddDatabaseSequence(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString AnimPath = Params->GetStringField(TEXT("anim_path"));
	bool bEnabled = true;
	if (Params->HasField(TEXT("enabled")))
	{
		bEnabled = Params->GetBoolField(TEXT("enabled"));
	}

	UPoseSearchDatabase* Database = FMonolithAssetUtils::LoadAssetByPath<UPoseSearchDatabase>(AssetPath);
	if (!Database)
		return FMonolithActionResult::Error(FString::Printf(TEXT("PoseSearchDatabase not found: %s"), *AssetPath));

	UObject* AnimAsset = FMonolithAssetUtils::LoadAssetByPath<UObject>(AnimPath);
	if (!AnimAsset)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Animation asset not found: %s"), *AnimPath));

	// Check if already in database
	if (Database->Contains(AnimAsset))
		return FMonolithActionResult::Error(FString::Printf(TEXT("Animation already in database: %s"), *AnimPath));

	GEditor->BeginTransaction(FText::FromString(TEXT("Add PoseSearch Database Animation")));
	Database->Modify();

	FPoseSearchDatabaseAnimationAsset NewEntry;
	NewEntry.AnimAsset = AnimAsset;
#if WITH_EDITORONLY_DATA
	NewEntry.bEnabled = bEnabled;
#endif

	const int32 IndexBefore = Database->GetNumAnimationAssets();
	Database->AddAnimationAsset(NewEntry);

	GEditor->EndTransaction();
	Database->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("index"), IndexBefore);
	Root->SetStringField(TEXT("animation"), AnimAsset->GetPathName());
	Root->SetBoolField(TEXT("enabled"), bEnabled);
	Root->SetNumberField(TEXT("new_count"), Database->GetNumAnimationAssets());
	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// remove_database_sequence
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithPoseSearchActions::HandleRemoveDatabaseSequence(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	int32 SequenceIndex = static_cast<int32>(Params->GetNumberField(TEXT("sequence_index")));

	UPoseSearchDatabase* Database = FMonolithAssetUtils::LoadAssetByPath<UPoseSearchDatabase>(AssetPath);
	if (!Database)
		return FMonolithActionResult::Error(FString::Printf(TEXT("PoseSearchDatabase not found: %s"), *AssetPath));

	const int32 NumAssets = Database->GetNumAnimationAssets();
	if (SequenceIndex < 0 || SequenceIndex >= NumAssets)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Invalid sequence index: %d (database has %d entries)"), SequenceIndex, NumAssets));

	// Capture info before removal
	FString RemovedAnimPath;
	if (const FPoseSearchDatabaseAnimationAsset* AnimAsset = Database->GetDatabaseAnimationAsset(SequenceIndex))
	{
		if (UObject* Asset = AnimAsset->GetAnimationAsset())
		{
			RemovedAnimPath = Asset->GetPathName();
		}
	}

	GEditor->BeginTransaction(FText::FromString(TEXT("Remove PoseSearch Database Animation")));
	Database->Modify();

	Database->RemoveAnimationAssetAt(SequenceIndex);

	GEditor->EndTransaction();
	Database->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("removed_index"), SequenceIndex);
	Root->SetStringField(TEXT("removed_animation"), RemovedAnimPath);
	Root->SetNumberField(TEXT("remaining_count"), Database->GetNumAnimationAssets());
	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// get_database_stats
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithPoseSearchActions::HandleGetDatabaseStats(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UPoseSearchDatabase* Database = FMonolithAssetUtils::LoadAssetByPath<UPoseSearchDatabase>(AssetPath);
	if (!Database)
		return FMonolithActionResult::Error(FString::Printf(TEXT("PoseSearchDatabase not found: %s"), *AssetPath));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetNumberField(TEXT("sequence_count"), Database->GetNumAnimationAssets());

	// Schema
	if (Database->Schema)
	{
		Root->SetStringField(TEXT("schema"), Database->Schema->GetPathName());
		Root->SetNumberField(TEXT("sample_rate"), Database->Schema->SampleRate);
		Root->SetNumberField(TEXT("schema_cardinality"), Database->Schema->SchemaCardinality);
	}

	// Search index stats
	const UE::PoseSearch::FSearchIndex& SearchIndex = Database->GetSearchIndex();
	const int32 NumPoses = SearchIndex.GetNumPoses();
	Root->SetNumberField(TEXT("total_pose_count"), NumPoses);
	Root->SetBoolField(TEXT("is_valid"), NumPoses > 0);

	// Search mode
	FString SearchModeStr;
	switch (Database->PoseSearchMode)
	{
	case EPoseSearchMode::BruteForce: SearchModeStr = TEXT("BruteForce"); break;
	case EPoseSearchMode::PCAKDTree: SearchModeStr = TEXT("PCAKDTree"); break;
	case EPoseSearchMode::VPTree: SearchModeStr = TEXT("VPTree"); break;
	case EPoseSearchMode::EventOnly: SearchModeStr = TEXT("EventOnly"); break;
	default: SearchModeStr = TEXT("Unknown"); break;
	}
	Root->SetStringField(TEXT("search_mode"), SearchModeStr);

	// Cost biases
	Root->SetNumberField(TEXT("continuing_pose_cost_bias"), Database->ContinuingPoseCostBias);
	Root->SetNumberField(TEXT("base_cost_bias"), Database->BaseCostBias);
	Root->SetNumberField(TEXT("looping_cost_bias"), Database->LoopingCostBias);

	// Tags
	if (Database->Tags.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> TagArray;
		for (const FName& Tag : Database->Tags)
		{
			TagArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
		}
		Root->SetArrayField(TEXT("tags"), TagArray);
	}

	return FMonolithActionResult::Success(Root);
}
