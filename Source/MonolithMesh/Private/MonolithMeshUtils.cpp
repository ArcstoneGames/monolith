#include "MonolithMeshUtils.h"
#include "MonolithAssetUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Dom/JsonValue.h"
#include "Editor.h"

namespace MonolithMeshUtils
{

UStaticMesh* LoadStaticMesh(const FString& Path, FString& OutError)
{
	UObject* Obj = FMonolithAssetUtils::LoadAssetByPath(Path);
	if (!Obj)
	{
		OutError = FString::Printf(TEXT("Asset not found: %s"), *Path);
		return nullptr;
	}

	UStaticMesh* SM = Cast<UStaticMesh>(Obj);
	if (!SM)
	{
		OutError = FString::Printf(TEXT("Expected StaticMesh, got %s"), *Obj->GetClass()->GetName());
		return nullptr;
	}

	return SM;
}

USkeletalMesh* LoadSkeletalMesh(const FString& Path, FString& OutError)
{
	UObject* Obj = FMonolithAssetUtils::LoadAssetByPath(Path);
	if (!Obj)
	{
		OutError = FString::Printf(TEXT("Asset not found: %s"), *Path);
		return nullptr;
	}

	USkeletalMesh* SK = Cast<USkeletalMesh>(Obj);
	if (!SK)
	{
		OutError = FString::Printf(TEXT("Expected SkeletalMesh, got %s"), *Obj->GetClass()->GetName());
		return nullptr;
	}

	return SK;
}

bool ParseVector(const TSharedPtr<FJsonObject>& Params, const FString& Key, FVector& Out)
{
	// Try array format: [x, y, z]
	const TArray<TSharedPtr<FJsonValue>>* Arr;
	if (Params->TryGetArrayField(Key, Arr) && Arr->Num() >= 3)
	{
		Out.X = (*Arr)[0]->AsNumber();
		Out.Y = (*Arr)[1]->AsNumber();
		Out.Z = (*Arr)[2]->AsNumber();
		return true;
	}

	// Try object format: {x, y, z}
	const TSharedPtr<FJsonObject>* Obj;
	if (Params->TryGetObjectField(Key, Obj))
	{
		Out.X = (*Obj)->GetNumberField(TEXT("x"));
		Out.Y = (*Obj)->GetNumberField(TEXT("y"));
		Out.Z = (*Obj)->GetNumberField(TEXT("z"));
		return true;
	}

	return false;
}

bool ParseRotator(const TSharedPtr<FJsonObject>& Params, const FString& Key, FRotator& Out)
{
	// Try array format: [pitch, yaw, roll]
	const TArray<TSharedPtr<FJsonValue>>* Arr;
	if (Params->TryGetArrayField(Key, Arr) && Arr->Num() >= 3)
	{
		Out.Pitch = (*Arr)[0]->AsNumber();
		Out.Yaw   = (*Arr)[1]->AsNumber();
		Out.Roll  = (*Arr)[2]->AsNumber();
		return true;
	}

	// Try object format: {pitch, yaw, roll}
	const TSharedPtr<FJsonObject>* Obj;
	if (Params->TryGetObjectField(Key, Obj))
	{
		Out.Pitch = (*Obj)->GetNumberField(TEXT("pitch"));
		Out.Yaw   = (*Obj)->GetNumberField(TEXT("yaw"));
		Out.Roll  = (*Obj)->GetNumberField(TEXT("roll"));
		return true;
	}

	return false;
}

AActor* FindActorByName(const FString& Name, FString& OutError)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		OutError = TEXT("No editor world available");
		return nullptr;
	}

	// Pass 1: Search by actor label (display name in editor)
	TArray<AActor*> LabelMatches;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->GetActorNameOrLabel() == Name || Actor->GetActorLabel() == Name)
		{
			LabelMatches.Add(Actor);
		}
	}

	if (LabelMatches.Num() == 1)
	{
		return LabelMatches[0];
	}

	if (LabelMatches.Num() > 1)
	{
		OutError = FString::Printf(TEXT("Ambiguous actor label '%s'. %d matches:"), *Name, LabelMatches.Num());
		for (AActor* Match : LabelMatches)
		{
			OutError += FString::Printf(TEXT("\n  - %s (%s)"), *Match->GetPathName(), *Match->GetClass()->GetName());
		}
		return nullptr;
	}

	// Pass 2: Search by FName (internal name)
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->GetFName().ToString() == Name)
		{
			return Actor;
		}
	}

	OutError = FString::Printf(TEXT("Actor not found: %s"), *Name);
	return nullptr;
}

UWorld* GetEditorWorld()
{
	if (GEditor)
	{
		// GetEditorWorldContext() gives us the main editor world
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (World)
		{
			return World;
		}
	}
	return nullptr;
}

MonolithMeshUtils::FBlockoutTags ParseBlockoutTags(const AActor* Actor)
{
	FBlockoutTags Result;
	if (!Actor) return Result;

	for (const FName& Tag : Actor->Tags)
	{
		FString TagStr = Tag.ToString();

		// Parse Monolith.Room:{type}
		if (TagStr.StartsWith(TEXT("Monolith.Room:"), ESearchCase::IgnoreCase))
		{
			Result.RoomType = TagStr.Mid(14); // len("Monolith.Room:") = 14
		}
		// Parse Monolith.Tag:{tag}
		else if (TagStr.StartsWith(TEXT("Monolith.Tag:"), ESearchCase::IgnoreCase))
		{
			Result.Tags.Add(TagStr.Mid(13)); // len("Monolith.Tag:") = 13
		}
		// Parse Monolith.Density:{density}
		else if (TagStr.StartsWith(TEXT("Monolith.Density:"), ESearchCase::IgnoreCase))
		{
			Result.Density = TagStr.Mid(17); // len("Monolith.Density:") = 17
		}
		// Parse Monolith.AllowPhysics
		else if (TagStr.Equals(TEXT("Monolith.AllowPhysics"), ESearchCase::IgnoreCase))
		{
			Result.bAllowPhysics = true;
		}
		// Parse Monolith.FloorHeight:{value}
		else if (TagStr.StartsWith(TEXT("Monolith.FloorHeight:"), ESearchCase::IgnoreCase))
		{
			FString Val = TagStr.Mid(21); // len("Monolith.FloorHeight:") = 21
			Result.FloorHeight = FCString::Atof(*Val);
		}
		// Parse Monolith.HasWalls
		else if (TagStr.Equals(TEXT("Monolith.HasWalls"), ESearchCase::IgnoreCase))
		{
			Result.bHasWalls = true;
		}
		// Parse Monolith.HasCeiling
		else if (TagStr.Equals(TEXT("Monolith.HasCeiling"), ESearchCase::IgnoreCase))
		{
			Result.bHasCeiling = true;
		}
	}

	return Result;
}

TSharedPtr<FJsonObject> BoundsToJson(const FBoxSphereBounds& Bounds)
{
	auto Result = MakeShared<FJsonObject>();

	FVector Min = Bounds.Origin - Bounds.BoxExtent;
	FVector Max = Bounds.Origin + Bounds.BoxExtent;
	FVector Extent = Bounds.BoxExtent * 2.0;

	TArray<TSharedPtr<FJsonValue>> MinArr;
	MinArr.Add(MakeShared<FJsonValueNumber>(Min.X));
	MinArr.Add(MakeShared<FJsonValueNumber>(Min.Y));
	MinArr.Add(MakeShared<FJsonValueNumber>(Min.Z));
	Result->SetArrayField(TEXT("min"), MinArr);

	TArray<TSharedPtr<FJsonValue>> MaxArr;
	MaxArr.Add(MakeShared<FJsonValueNumber>(Max.X));
	MaxArr.Add(MakeShared<FJsonValueNumber>(Max.Y));
	MaxArr.Add(MakeShared<FJsonValueNumber>(Max.Z));
	Result->SetArrayField(TEXT("max"), MaxArr);

	TArray<TSharedPtr<FJsonValue>> ExtArr;
	ExtArr.Add(MakeShared<FJsonValueNumber>(Extent.X));
	ExtArr.Add(MakeShared<FJsonValueNumber>(Extent.Y));
	ExtArr.Add(MakeShared<FJsonValueNumber>(Extent.Z));
	Result->SetArrayField(TEXT("extent"), ExtArr);

	TArray<TSharedPtr<FJsonValue>> CenterArr;
	CenterArr.Add(MakeShared<FJsonValueNumber>(Bounds.Origin.X));
	CenterArr.Add(MakeShared<FJsonValueNumber>(Bounds.Origin.Y));
	CenterArr.Add(MakeShared<FJsonValueNumber>(Bounds.Origin.Z));
	Result->SetArrayField(TEXT("center"), CenterArr);

	return Result;
}

TSharedPtr<FJsonObject> TransformToJson(const FTransform& Transform)
{
	auto Result = MakeShared<FJsonObject>();

	FVector Loc = Transform.GetLocation();
	FRotator Rot = Transform.Rotator();
	FVector Scale = Transform.GetScale3D();

	TArray<TSharedPtr<FJsonValue>> LocArr;
	LocArr.Add(MakeShared<FJsonValueNumber>(Loc.X));
	LocArr.Add(MakeShared<FJsonValueNumber>(Loc.Y));
	LocArr.Add(MakeShared<FJsonValueNumber>(Loc.Z));
	Result->SetArrayField(TEXT("location"), LocArr);

	TArray<TSharedPtr<FJsonValue>> RotArr;
	RotArr.Add(MakeShared<FJsonValueNumber>(Rot.Pitch));
	RotArr.Add(MakeShared<FJsonValueNumber>(Rot.Yaw));
	RotArr.Add(MakeShared<FJsonValueNumber>(Rot.Roll));
	Result->SetArrayField(TEXT("rotation"), RotArr);

	TArray<TSharedPtr<FJsonValue>> ScaleArr;
	ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.X));
	ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.Y));
	ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.Z));
	Result->SetArrayField(TEXT("scale"), ScaleArr);

	return Result;
}

bool MatchTag(const FName& A, const FName& B)
{
	return A.IsEqual(B, ENameCase::IgnoreCase);
}

} // namespace MonolithMeshUtils
