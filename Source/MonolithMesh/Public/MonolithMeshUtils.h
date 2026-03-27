#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UStaticMesh;
class USkeletalMesh;
class AActor;
class UWorld;

namespace MonolithMeshUtils
{
	/** Load and validate a StaticMesh from asset path */
	UStaticMesh* LoadStaticMesh(const FString& Path, FString& OutError);

	/** Load and validate a SkeletalMesh from asset path */
	USkeletalMesh* LoadSkeletalMesh(const FString& Path, FString& OutError);

	/** Parse a location vector from JSON params (array of 3 floats or {x,y,z} object) */
	bool ParseVector(const TSharedPtr<FJsonObject>& Params, const FString& Key, FVector& Out);

	/** Parse a rotator from JSON params (array of 3 floats or {pitch,yaw,roll} object) */
	bool ParseRotator(const TSharedPtr<FJsonObject>& Params, const FString& Key, FRotator& Out);

	/** Find an actor by name in the current editor world (checks label first, then internal name) */
	AActor* FindActorByName(const FString& Name, FString& OutError);

	/** Get the current editor world */
	UWorld* GetEditorWorld();

	/** Parsed blockout tags from an actor's tag array */
	struct FBlockoutTags
	{
		FString RoomType;
		TArray<FString> Tags;
		FString Density;
		bool bAllowPhysics = false;
		float FloorHeight = 0.0f;
		bool bHasWalls = false;
		bool bHasCeiling = false;
	};

	/** Parse blockout tags from an actor's tag array */
	FBlockoutTags ParseBlockoutTags(const AActor* Actor);

	/** Build a JSON object from FBoxSphereBounds */
	TSharedPtr<FJsonObject> BoundsToJson(const FBoxSphereBounds& Bounds);

	/** Build a JSON object from an FTransform */
	TSharedPtr<FJsonObject> TransformToJson(const FTransform& Transform);

	/** Case-insensitive FName tag matching (handles FName case folding) */
	bool MatchTag(const FName& A, const FName& B);
}
