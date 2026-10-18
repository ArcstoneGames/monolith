#pragma once

#include "MonolithIndexer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UPackage;

enum class EMonolithLevelIndexStepResult : uint8
{
	Continue,
	/** Unreal deferred GC while releasing the previous map; call again after a short delay. */
	RetryLater,
	Complete,
	Degraded,
	Failed
};

/**
 * Indexes level actors from World/Map assets.
 * Runs after all other indexers (needs all assets in DB).
 * Uses special class name "__Levels__" for post-indexing dispatch.
 * Loads each level's persistent level to extract actor metadata.
 */
class FLevelIndexer : public IMonolithIndexer
{
public:
	virtual TArray<FString> GetSupportedClasses() const override
	{
		return { TEXT("__Levels__") };
	}

	virtual bool IndexAsset(const FAssetData& AssetData, UObject* LoadedAsset, FMonolithIndexDatabase& DB, int64 AssetId) override;
	virtual FString GetName() const override { return TEXT("LevelIndexer"); }
	virtual bool IsSentinel() const override { return true; }

	/** Each ProcessNextWorld call loads at most one root map. */
	bool BeginIndex(FMonolithIndexDatabase& DB);
	EMonolithLevelIndexStepResult ProcessNextWorld(FMonolithIndexDatabase& DB);
	bool FinishIndex(FMonolithIndexDatabase& DB);

	/** Set of valid path prefixes for indexing */
	TArray<FName> IndexedPaths;

	/** Set indexed paths. Called before IndexAsset. */
	void SetIndexedPaths(const TArray<FName>& InPaths) { IndexedPaths = InPaths; }

private:
	void BuildActorRows(class ULevel* Level, int64 AssetId, TArray<FIndexedActor>& OutActors);
	bool PersistState(FMonolithIndexDatabase& DB, const FString& Prefix, const FString& Detail = FString()) const;
	/** Record a degraded stop, naming the maps this pass did not reach. */
	EMonolithLevelIndexStepResult StopDegraded(FMonolithIndexDatabase& DB, const FString& Reason);
	/** Retry a release that Unreal deferred. Returns true once nothing is pending. */
	bool RetryPendingRelease();
	void ResetSession();
	FString SerializeTransform(const FTransform& Transform);
	FString SerializeComponents(const class AActor* Actor);

	TArray<FAssetData> WorldAssets;
	TMap<FName, int64> CandidateAssetIds;
	TSet<FName> ProcessedPackages;
	int32 NextWorldIndex = 0;
	int32 ActorsInserted = 0;
	int32 LevelsProcessed = 0;
	int32 LevelsFailed = 0;
	bool bSessionInitialized = false;
	bool bPressureCleanupAttempted = false;
	FString TerminalReason;

	/** Packages whose release GC was deferred, retried before the next map loads. */
	TArray<TWeakObjectPtr<UPackage>> PendingReleasePackages;
	FName PendingReleaseWorld;
	int32 PendingReleaseAttempts = 0;
};
