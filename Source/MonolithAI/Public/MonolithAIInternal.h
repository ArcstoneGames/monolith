#pragma once

#include "CoreMinimal.h"
#include "MonolithToolRegistry.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "GameplayTagContainer.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMonolithAI, Log, All);

namespace MonolithAI
{
	// Load a Blackboard asset from the "asset_path" param. Returns nullptr + sets ErrorMsg on failure.
	UBlackboardData* LoadBlackboardFromParams(const TSharedPtr<FJsonObject>& Params, FString& OutAssetPath, FString& OutError);

	// Load a BehaviorTree asset from the "asset_path" param. Returns nullptr + sets ErrorMsg on failure.
	UBehaviorTree* LoadBehaviorTreeFromParams(const TSharedPtr<FJsonObject>& Params, FString& OutAssetPath, FString& OutError);

	// Load an AIController Blueprint from the "asset_path" param. Returns nullptr + sets ErrorMsg on failure.
	UBlueprint* LoadAIControllerFromParams(const TSharedPtr<FJsonObject>& Params, FString& OutAssetPath, FString& OutError);

	// Load any UObject asset from an asset path param
	UObject* LoadAssetFromPath(const FString& AssetPath, FString& OutError);

	// Get or create a package for asset creation
	UPackage* GetOrCreatePackage(const FString& SavePath, FString& OutError);

	// Validate required string param, return error result if missing
	bool RequireStringParam(const TSharedPtr<FJsonObject>& Params, const FString& ParamName, FString& OutValue, FMonolithActionResult& OutError);

	/**
	 * Check that no asset exists at the given path (on disk or in memory).
	 * Uses AssetRegistry (disk check) + FindObject (memory check).
	 * Returns true if path is free. Sets OutError if blocked.
	 */
	bool EnsureAssetPathFree(const FString& PackagePath, const FString& AssetName, FString& OutError);

	// Build a standard success result with asset_path field
	TSharedPtr<FJsonObject> MakeAssetResult(const FString& AssetPath, const FString& Message = TEXT(""));

	// Parse a JSON array field into TArray<FString>
	TArray<FString> ParseStringArray(const TSharedPtr<FJsonObject>& Params, const FString& FieldName);

	// Get the PIE world, if one exists
	UWorld* GetPIEWorld();

	// Find an actor in PIE by label, name, or path
	AActor* FindActorInPIE(const FString& ActorIdentifier);

	// Get the project Source directory path
	FString GetProjectSourceDir();
}
