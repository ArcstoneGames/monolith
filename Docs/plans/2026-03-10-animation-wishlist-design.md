# Animation Wishlist — Implementation Design

**Date:** 2026-03-10
**Module:** MonolithAnimation
**Current state:** 23 actions in `MonolithAnimationActions.cpp/.h`
**Target:** ~66 actions total (+43 new across 8 waves)
**Engine:** UE 5.7

---

## Table of Contents

1. [Architecture & Patterns](#architecture--patterns)
2. [Wave 1 — Read Actions (8)](#wave-1--read-actions-8-actions)
3. [Wave 2 — Notify CRUD (4)](#wave-2--notify-crud-4-actions)
4. [Wave 3 — Curve CRUD (5)](#wave-3--curve-crud-5-actions)
5. [Wave 4 — Skeleton + BlendSpace (6)](#wave-4--skeleton--blendspace-6-actions)
6. [Wave 5 — Creation + Montage (6)](#wave-5--creation--montage-6-actions)
7. [Wave 6 — PoseSearch (5)](#wave-6--posesearch-5-actions)
8. [Wave 7 — Anim Modifiers + Composites (5)](#wave-7--anim-modifiers--composites-5-actions)
9. [Wave 8 — IKRig/ControlRig (DEFERRED)](#wave-8--ikrigcontrolrig-deferred)
10. [Build.cs Changes](#buildcs-changes)
11. [File Conflict Avoidance Plan](#file-conflict-avoidance-plan)

---

## Architecture & Patterns

### Handler Signature

Every action handler is a static method returning `FMonolithActionResult`:

```cpp
static FMonolithActionResult HandleXxx(const TSharedPtr<FJsonObject>& Params);
```

### Registration Pattern

Actions register in `FMonolithAnimationActions::RegisterActions(FMonolithToolRegistry& Registry)`:

```cpp
Registry.RegisterAction(TEXT("animation"), TEXT("action_name"),
    TEXT("Human-readable description"),
    FMonolithActionHandler::CreateStatic(&HandleActionName),
    FParamSchemaBuilder()
        .Required(TEXT("param"), TEXT("type"), TEXT("desc"))
        .Optional(TEXT("param2"), TEXT("type"), TEXT("desc"), TEXT("default"))
        .Build());
```

### Read Pattern (no mutation)

```cpp
FMonolithActionResult FMonolithAnimationActions::HandleGetXxx(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath = Params->GetStringField(TEXT("asset_path"));
    UAssetType* Asset = FMonolithAssetUtils::LoadAssetByPath<UAssetType>(AssetPath);
    if (!Asset) return FMonolithActionResult::Error(FString::Printf(TEXT("Type not found: %s"), *AssetPath));

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    // ... populate Root ...
    return FMonolithActionResult::Success(Root);
}
```

### Write Pattern (mutation)

```cpp
FMonolithActionResult FMonolithAnimationActions::HandleSetXxx(const TSharedPtr<FJsonObject>& Params)
{
    // ... load asset, validate ...
    GEditor->BeginTransaction(FText::FromString(TEXT("Transaction Name")));
    Asset->Modify();
    // ... mutate ...
    GEditor->EndTransaction();
    Asset->MarkPackageDirty();   // For operations that don't auto-dirty

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    // ... populate result ...
    return FMonolithActionResult::Success(Root);
}
```

### JSON Value Construction Helpers

```cpp
// String array
TArray<TSharedPtr<FJsonValue>> Arr;
Arr.Add(MakeShared<FJsonValueString>(SomeString));
Root->SetArrayField(TEXT("items"), Arr);

// Object array
TArray<TSharedPtr<FJsonValue>> ObjArr;
TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
Obj->SetStringField(TEXT("name"), Name);
ObjArr.Add(MakeShared<FJsonValueObject>(Obj));
Root->SetArrayField(TEXT("items"), ObjArr);

// Number array
Arr.Add(MakeShared<FJsonValueNumber>(Value));
```

### Param Types in FParamSchemaBuilder

- `"string"` — `Params->GetStringField()` / `Params->TryGetStringField()`
- `"number"` — `Params->GetNumberField()` → `double`, cast to `float` with `static_cast<float>()`
- `"integer"` — `Params->GetNumberField()` → cast to `int32` with `static_cast<int32>()`
- `"bool"` — `Params->TryGetBoolField()`
- `"array"` — `Params->TryGetArrayField()` → `const TArray<TSharedPtr<FJsonValue>>*`
- `"object"` — `Params->GetObjectField()`

### Module Startup

In `MonolithAnimationModule.cpp`:
```cpp
void FMonolithAnimationModule::StartupModule()
{
    FMonolithAnimationActions::RegisterActions(FMonolithToolRegistry::Get());
    UE_LOG(LogMonolith, Verbose, TEXT("Monolith — Animation module loaded (N actions)"));
}
```

---

## Wave 1 — Read Actions (8 actions)

**File:** `MonolithAnimationActions.cpp/.h`
**Dependencies:** None new (all existing headers suffice)
**Parallelizable with:** Waves 6, 8

---

### 1.1 `get_sequence_info`

**Handler:** `HandleGetSequenceInfo`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | AnimSequence asset path |

**Return JSON:**
```json
{
  "asset_path": "/Game/...",
  "skeleton": "/Game/.../Skeleton",
  "duration": 2.5,
  "num_frames": 75,
  "num_keys": 76,
  "sample_rate": 30.0,
  "frame_rate": "30/1",
  "has_root_motion": true,
  "root_motion_lock": "AnimFirstFrame",
  "force_root_lock": false,
  "additive_type": "None",
  "interpolation": "Linear",
  "rate_scale": 1.0,
  "is_looping": false,
  "compression_scheme": "PerTrackCompression"
}
```

**UE API calls:**
- `UAnimSequence::GetPlayLength()` → `float` duration
- `UAnimSequence::GetDataModel()->GetNumberOfFrames()` → `int32`
- `UAnimSequence::GetDataModel()->GetNumberOfKeys()` → `int32`
- `UAnimSequence::GetSamplingFrameRate()` → `FFrameRate` (call `.AsDecimal()` for float)
- `UAnimSequence::GetSkeleton()` → `USkeleton*`
- `UAnimSequence::bEnableRootMotion` — `bool` UPROPERTY
- `UAnimSequence::RootMotionRootLock` — `TEnumAsByte<ERootMotionRootLock::Type>` UPROPERTY
- `UAnimSequence::bForceRootLock` — `bool` UPROPERTY
- `UAnimSequence::AdditiveAnimType` — `TEnumAsByte<EAdditiveAnimationType>` UPROPERTY
- `UAnimSequence::Interpolation` — `EAnimInterpolationType` UPROPERTY
- `UAnimSequence::RateScale` — `float` UPROPERTY (on `UAnimSequenceBase`)
- `UAnimSequenceBase::bLoop` — `bool` UPROPERTY
- `UAnimSequence::BoneCompressionSettings` → `GetName()` for scheme name

**Error cases:**
- Asset not found
- Asset is not `UAnimSequence` (could be montage/composite loaded by base path)

**Includes needed:** Already included (`Animation/AnimSequence.h`)

---

### 1.2 `get_sequence_notifies`

**Handler:** `HandleGetSequenceNotifies`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | Animation asset path (sequence, montage, etc.) |

**Return JSON:**
```json
{
  "asset_path": "/Game/...",
  "count": 3,
  "notifies": [
    {
      "index": 0,
      "name": "NotifyName",
      "time": 0.5,
      "duration": 0.0,
      "trigger_time_offset": 0.0,
      "notify_class": "AnimNotify_PlaySound",
      "state_class": "",
      "track_index": 0,
      "track_name": "1",
      "is_state": false
    }
  ]
}
```

**UE API calls:**
- `UAnimSequenceBase::Notifies` — `TArray<FAnimNotifyEvent>` UPROPERTY
- `FAnimNotifyEvent::NotifyName` — `FName`
- `FAnimNotifyEvent::GetTime()` → `float` (or `GetTriggerTime()`)
- `FAnimNotifyEvent::GetDuration()` → `float`
- `FAnimNotifyEvent::TriggerTimeOffset` — `float`
- `FAnimNotifyEvent::Notify` — `UAnimNotify*` (point notify class instance)
- `FAnimNotifyEvent::NotifyStateClass` — `UAnimNotifyState*` (state notify class instance)
- `FAnimNotifyEvent::TrackIndex` — `int32`
- `UAnimSequenceBase::AnimNotifyTracks` — `TArray<FAnimNotifyTrack>` (track names)
- `FAnimNotifyTrack::TrackName` — `FName`

**Error cases:**
- Asset not found (load as `UAnimSequenceBase` to accept sequence/montage/composite)

---

### 1.3 `get_bone_track_keys`

**Handler:** `HandleGetBoneTrackKeys`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | AnimSequence asset path |
| `bone_name` | string | yes | Bone name to read |
| `start_frame` | integer | no | Start frame (default 0) |
| `end_frame` | integer | no | End frame (default -1 = all) |

**Return JSON:**
```json
{
  "bone_name": "spine_01",
  "num_keys": 75,
  "positions": [[0,0,0], [0.1,0,0], ...],
  "rotations": [[0,0,0,1], [0.01,0,0,1], ...],
  "scales": [[1,1,1], [1,1,1], ...]
}
```

**UE API calls:**
- `UAnimSequence::GetDataModel()` → `IAnimationDataModel*`
- `IAnimationDataModel::GetBoneTrackByName(FName)` → `const FBoneAnimationTrack&`
- `IAnimationDataModel::FindBoneTrackIndex(FName)` → `int32` (INDEX_NONE if not found)
- `FBoneAnimationTrack::InternalTrackData` → `FRawAnimSequenceTrack`
- `FRawAnimSequenceTrack::PosKeys` — `TArray<FVector3f>`
- `FRawAnimSequenceTrack::RotKeys` — `TArray<FQuat4f>`
- `FRawAnimSequenceTrack::ScaleKeys` — `TArray<FVector3f>`

**Error cases:**
- Asset not found
- Bone track not found in animation data model
- `start_frame` / `end_frame` out of range

---

### 1.4 `get_sequence_curves`

**Handler:** `HandleGetSequenceCurves`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | AnimSequence asset path |

**Return JSON:**
```json
{
  "asset_path": "/Game/...",
  "count": 2,
  "curves": [
    {
      "name": "CurveName",
      "type": "Float",
      "num_keys": 30
    }
  ]
}
```

**UE API calls:**
- `UAnimSequence::GetDataModel()` → `IAnimationDataModel*`
- `IAnimationDataModel::GetCurveData()` → `const FAnimationCurveData&`
- `FAnimationCurveData::FloatCurves` — `TArray<FFloatCurve>`
- `FAnimationCurveData::TransformCurves` — `TArray<FTransformCurve>`
- `FFloatCurve::GetName()` → `FName`
- `FFloatCurve::FloatCurve` → `FRichCurve` → `.GetNumKeys()`
- `FTransformCurve::GetName()` → `FName`

**Error cases:**
- Asset not found

---

### 1.5 `get_montage_info`

**Handler:** `HandleGetMontageInfo`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | Montage asset path |

**Return JSON:**
```json
{
  "asset_path": "/Game/...",
  "skeleton": "/Game/.../Skeleton",
  "duration": 3.0,
  "rate_scale": 1.0,
  "blend_in_time": 0.25,
  "blend_out_time": 0.25,
  "blend_out_trigger_time": -1.0,
  "enable_auto_blend_out": true,
  "sections": [
    {
      "index": 0,
      "name": "Default",
      "time": 0.0,
      "next_section": "None"
    }
  ],
  "slots": [
    {
      "index": 0,
      "slot_name": "DefaultGroup.DefaultSlot"
    }
  ],
  "notify_count": 2
}
```

**UE API calls:**
- Load as `UAnimMontage`
- `UAnimMontage::CompositeSections` — `TArray<FCompositeSection>` UPROPERTY
- `FCompositeSection::SectionName` — `FName`
- `FCompositeSection::GetTime()` → `float`
- `FCompositeSection::NextSectionName` — `FName`
- `UAnimMontage::SlotAnimTracks` — `TArray<FSlotAnimationTrack>` UPROPERTY
- `FSlotAnimationTrack::SlotName` — `FName`
- `UAnimMontage::BlendIn` — `FAlphaBlend` → `.GetBlendTime()` → `float`
- `UAnimMontage::BlendOut` — `FAlphaBlend` → `.GetBlendTime()` → `float`
- `UAnimMontage::BlendOutTriggerTime` — `float`
- `UAnimMontage::bEnableAutoBlendOut` — `bool`
- `UAnimSequenceBase::RateScale` — `float`
- `UAnimSequenceBase::GetPlayLength()` → `float`
- `UAnimSequenceBase::Notifies.Num()`

**Error cases:**
- Asset not found
- Asset is not a montage

---

### 1.6 `get_blend_space_info`

**Handler:** `HandleGetBlendSpaceInfo`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | BlendSpace asset path |

**Return JSON:**
```json
{
  "asset_path": "/Game/...",
  "skeleton": "/Game/.../Skeleton",
  "is_1d": false,
  "axis_x": {
    "name": "Speed",
    "min": 0.0,
    "max": 600.0,
    "grid_divisions": 4,
    "snap_to_grid": false,
    "wrap_input": false
  },
  "axis_y": {
    "name": "Direction",
    "min": -180.0,
    "max": 180.0,
    "grid_divisions": 4,
    "snap_to_grid": false,
    "wrap_input": true
  },
  "sample_count": 9,
  "samples": [
    {
      "index": 0,
      "animation": "/Game/.../Idle",
      "x": 0.0,
      "y": 0.0,
      "rate_scale": 1.0
    }
  ]
}
```

**UE API calls:**
- Load as `UBlendSpace`
- `UBlendSpace::IsA<UBlendSpace1D>()` — check for 1D
- `UBlendSpace::BlendParameters` — `FBlendParameter[3]` (access via direct member, indices 0=X, 1=Y)
- `FBlendParameter::DisplayName`, `Min`, `Max`, `GridNum`, `bSnapToGrid`, `bWrapInput`
- `UBlendSpace::SampleData` — `TArray<FBlendSample>` or `GetBlendSamples()` → `const TArray<FBlendSample>&`
- `FBlendSample::Animation` — `TObjectPtr<UAnimSequence>`
- `FBlendSample::SampleValue` — `FVector` (X=blend0, Y=blend1)
- `FBlendSample::RateScale` — `float`

**Error cases:**
- Asset not found
- Asset is not a BlendSpace

**Includes needed:** `#include "Animation/BlendSpace.h"` (already included), may need `#include "Animation/BlendSpace1D.h"`

---

### 1.7 `get_skeleton_sockets`

**Handler:** `HandleGetSkeletonSockets`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | Skeleton or SkeletalMesh asset path |

**Return JSON:**
```json
{
  "asset_path": "/Game/...",
  "count": 5,
  "sockets": [
    {
      "name": "weapon_r",
      "bone": "hand_r",
      "location": [10.0, 0.0, 0.0],
      "rotation": [0.0, 90.0, 0.0],
      "scale": [1.0, 1.0, 1.0]
    }
  ]
}
```

**UE API calls:**
- Try load as `USkeleton` first, fall back to `USkeletalMesh`
- `USkeleton::Sockets` — `TArray<USkeletalMeshSocket*>` (protected, use `GetSockets()` if available, or iterate with `USkeleton::GetSocketByIndex(int32)` / `USkeleton::NumSockets()` — note: these are actually on `USkeletalMesh`, for Skeleton use `USkeleton::Sockets` directly)
- Actually: `USkeleton` stores sockets in `Sockets` array (UPROPERTY, protected). Access via the helper or iterate the skeleton's mesh sockets.
- For Skeleton: use `USkeleton::FindSocket(FName)` or iterate `Skeleton->Sockets`
- For SkeletalMesh: `USkeletalMesh::GetSocketByIndex(int32)`, `USkeletalMesh::NumSockets()`
- `USkeletalMeshSocket::SocketName`, `BoneName` — `FName`
- `USkeletalMeshSocket::RelativeLocation` — `FVector`
- `USkeletalMeshSocket::RelativeRotation` — `FRotator`
- `USkeletalMeshSocket::RelativeScale` — `FVector`

**Error cases:**
- Asset not found (neither Skeleton nor SkeletalMesh)

**Includes needed:** Already included (`Engine/SkeletalMeshSocket.h`)

---

### 1.8 `get_abp_info`

**Handler:** `HandleGetAbpInfo`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | AnimBlueprint asset path |

**Return JSON:**
```json
{
  "asset_path": "/Game/...",
  "skeleton": "/Game/.../Skeleton",
  "parent_class": "UAnimInstance",
  "state_machine_count": 2,
  "graph_count": 3,
  "variable_count": 12,
  "interfaces": ["IAnimLayerInterface_Locomotion"],
  "graphs": ["AnimGraph", "EventGraph"],
  "target_skeleton": "/Game/.../Skeleton"
}
```

**UE API calls:**
- Load as `UAnimBlueprint`
- `UAnimBlueprint::TargetSkeleton` (deprecated) or `UAnimBlueprint::GetTargetSkeleton()` → `USkeleton*`
- `UAnimBlueprint::ParentClass` → `UClass*` → `GetName()`
- `UAnimBlueprint::FunctionGraphs` — iterate for graph count
- Count `UAnimGraphNode_StateMachine` nodes across graphs for SM count
- `UBlueprint::NewVariables` — `TArray<FBPVariableDescription>` → `.Num()` for variable count
- `UBlueprint::ImplementedInterfaces` — `TArray<FBPInterfaceDescription>` → interface names

**Error cases:**
- Asset not found
- Asset is not an AnimBlueprint

---

## Wave 2 — Notify CRUD (4 actions)

**File:** `MonolithAnimationActions.cpp/.h`
**Dependencies:** Need `AnimationBlueprintLibrary` module (already in Build.cs)
**Parallelizable with:** Waves 4, 6, 8
**Key header:** `#include "AnimationBlueprintLibrary.h"`

---

### 2.1 `add_notify`

**Handler:** `HandleAddNotify`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | Animation asset path |
| `notify_class` | string | yes | Notify class name (e.g. "AnimNotify_PlaySound") |
| `time` | number | yes | Trigger time in seconds |
| `track_name` | string | no | Notify track name (default "1") |

**Return JSON:**
```json
{
  "index": 3,
  "notify_class": "AnimNotify_PlaySound",
  "time": 0.5,
  "track_name": "1"
}
```

**UE API calls:**
- Load as `UAnimSequenceBase`
- `FindFirstObject<UClass>()` with notify_class to get the UClass
- `NewObject<UAnimNotify>(Seq, NotifyClass)` — create the notify instance
- `UAnimationBlueprintLibrary::AddAnimationNotifyEventObject(AnimSequenceBase, StartTime, Notify, NotifyTrackName)`
  - Signature: `void AddAnimationNotifyEventObject(UAnimSequenceBase*, float StartTime, UAnimNotify* Notify, FName NotifyTrackName)`
  - This handles creating the `FAnimNotifyEvent`, linking it, adding the track if needed
- After: `Seq->RefreshCacheData()` to update internal state

**Error cases:**
- Asset not found
- Notify class not found / not a subclass of `UAnimNotify`
- Time out of range (< 0 or > duration)
- Track name validation (auto-create is OK, library handles this)

**Implementation notes:**
- Wrap in `GEditor->BeginTransaction`/`EndTransaction`
- Call `Seq->Modify()` before
- The returned index is `Seq->Notifies.Num() - 1` after insertion (library appends)

---

### 2.2 `add_notify_state`

**Handler:** `HandleAddNotifyState`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | Animation asset path |
| `notify_class` | string | yes | NotifyState class name (e.g. "AnimNotifyState_Trail") |
| `time` | number | yes | Start time in seconds |
| `duration` | number | yes | Duration in seconds |
| `track_name` | string | no | Notify track name (default "1") |

**Return JSON:**
```json
{
  "index": 4,
  "notify_class": "AnimNotifyState_Trail",
  "time": 0.5,
  "duration": 1.0,
  "track_name": "1"
}
```

**UE API calls:**
- Load as `UAnimSequenceBase`
- `FindFirstObject<UClass>()` with notify_class
- `NewObject<UAnimNotifyState>(Seq, NotifyStateClass)` — create the state notify instance
- `UAnimationBlueprintLibrary::AddAnimationNotifyStateEventObject(AnimSequenceBase, StartTime, Duration, NotifyState, NotifyTrackName)`
  - Signature: `void AddAnimationNotifyStateEventObject(UAnimSequenceBase*, float StartTime, float Duration, UAnimNotifyState* NotifyState, FName NotifyTrackName)`

**Error cases:**
- Asset not found
- Class not found / not a subclass of `UAnimNotifyState`
- Time out of range
- Duration <= 0
- Time + Duration > play length

---

### 2.3 `remove_notify`

**Handler:** `HandleRemoveNotify`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | Animation asset path |
| `notify_index` | integer | yes | Index of notify to remove |

**Return JSON:**
```json
{
  "removed_index": 2,
  "removed_name": "NotifyName",
  "remaining_count": 4
}
```

**UE API calls:**
- Load as `UAnimSequenceBase`
- Validate `Notifies.IsValidIndex(notify_index)`
- Save name before removal: `Seq->Notifies[notify_index].NotifyName`
- `Seq->Notifies.RemoveAt(notify_index)` — direct array removal
- `Seq->RefreshCacheData()` — rebuild internal caches
- `Seq->MarkPackageDirty()`

**Error cases:**
- Asset not found
- Invalid notify index

---

### 2.4 `set_notify_track`

**Handler:** `HandleSetNotifyTrack`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | Animation asset path |
| `notify_index` | integer | yes | Index of notify to move |
| `track_index` | integer | yes | Target track index |

**Return JSON:**
```json
{
  "notify_index": 2,
  "notify_name": "FootstepL",
  "new_track_index": 1,
  "track_name": "Footsteps"
}
```

**UE API calls:**
- Load as `UAnimSequenceBase`
- Validate `Notifies.IsValidIndex(notify_index)`
- Validate `AnimNotifyTracks.IsValidIndex(track_index)`
- `Seq->Notifies[notify_index].TrackIndex = track_index`
- `Seq->RefreshCacheData()`
- Track name: `Seq->AnimNotifyTracks[track_index].TrackName`

**Error cases:**
- Asset not found
- Invalid notify index
- Invalid track index

---

## Wave 3 — Curve CRUD (5 actions)

**File:** `MonolithAnimationActions.cpp/.h`
**Dependencies:** None new. Uses `IAnimationDataController` (from `AnimationDataController` module, linked via `Engine`)
**Parallelizable with:** Waves 4, 6, 8
**Key includes:** `#include "Animation/AnimCurveTypes.h"`, `#include "Animation/AnimData/IAnimationDataModel.h"`

**Critical API pattern for curves:**
```cpp
#include "Animation/Skeleton.h"  // for UAnimationCurveIdentifierExtensions
// Create curve identifier:
FAnimationCurveIdentifier CurveId(FName(*CurveName), ERawCurveTrackTypes::RCT_Float);
// Or use the helper:
FAnimationCurveIdentifier CurveId = UAnimationCurveIdentifierExtensions::GetCurveIdentifier(
    Seq->GetSkeleton(), FName(*CurveName), ERawCurveTrackTypes::RCT_Float);

IAnimationDataController& Controller = Seq->GetController();
Controller.AddCurve(CurveId);
Controller.SetCurveKeys(CurveId, Keys);
Controller.RemoveCurve(CurveId);
```

---

### 3.1 `list_curves`

**Handler:** `HandleListCurves`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | AnimSequence asset path |
| `include_keys` | bool | no | Include key data (default false) |

**Return JSON:**
```json
{
  "asset_path": "/Game/...",
  "count": 2,
  "curves": [
    {
      "name": "EnableFootIK",
      "type": "Float",
      "num_keys": 30,
      "keys": [
        {"time": 0.0, "value": 1.0},
        {"time": 0.5, "value": 0.0}
      ]
    }
  ]
}
```

**UE API calls:**
- `Seq->GetDataModel()->GetCurveData()` → `const FAnimationCurveData&`
- `FAnimationCurveData::FloatCurves` — `TArray<FFloatCurve>`
- `FFloatCurve::GetName()` → `FName`
- `FFloatCurve::FloatCurve` → `FRichCurve`
- `FRichCurve::GetNumKeys()` → `int32`
- `FRichCurve::GetConstRefOfKeys()` → `const TArray<FRichCurveKey>&`
- `FRichCurveKey::Time`, `Value`, `InterpMode`

**Error cases:**
- Asset not found

---

### 3.2 `add_curve`

**Handler:** `HandleAddCurve`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | AnimSequence asset path |
| `curve_name` | string | yes | Name for the new curve |
| `curve_type` | string | no | "Float" or "Transform" (default "Float") |

**Return JSON:**
```json
{
  "curve_name": "MyCurve",
  "curve_type": "Float",
  "success": true
}
```

**UE API calls:**
- `IAnimationDataController& Controller = Seq->GetController()`
- `ERawCurveTrackTypes Type = (curve_type == "Transform") ? ERawCurveTrackTypes::RCT_Transform : ERawCurveTrackTypes::RCT_Float`
- `FAnimationCurveIdentifier CurveId(FName(*CurveName), Type)`
- `Controller.AddCurve(CurveId)` → `bool` success

**Error cases:**
- Asset not found
- Curve already exists (check `Seq->GetDataModel()->FindCurve(CurveId)` beforehand)
- Invalid curve type string

---

### 3.3 `remove_curve`

**Handler:** `HandleRemoveCurve`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | AnimSequence asset path |
| `curve_name` | string | yes | Name of curve to remove |
| `curve_type` | string | no | "Float" or "Transform" (default "Float") |

**Return JSON:**
```json
{
  "curve_name": "MyCurve",
  "removed": true
}
```

**UE API calls:**
- `Controller.RemoveCurve(CurveId)` → `bool` success

**Error cases:**
- Asset not found
- Curve not found

---

### 3.4 `set_curve_keys`

**Handler:** `HandleSetCurveKeys`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | AnimSequence asset path |
| `curve_name` | string | yes | Curve name |
| `keys_json` | string | yes | JSON array: `[{"time":0.0,"value":1.0,"interp":"cubic"}, ...]` |

**Return JSON:**
```json
{
  "curve_name": "MyCurve",
  "num_keys": 10
}
```

**UE API calls:**
- Parse `keys_json` into `TArray<FRichCurveKey>` — set `Time`, `Value`, `InterpMode`
  - Map interp strings: `"constant"` → `RCIM_Constant`, `"linear"` → `RCIM_Linear`, `"cubic"` → `RCIM_Cubic`
- `FAnimationCurveIdentifier CurveId(FName(*CurveName), ERawCurveTrackTypes::RCT_Float)`
- `Controller.SetCurveKeys(CurveId, Keys)` → `bool` success
  - Signature: `bool SetCurveKeys(const FAnimationCurveIdentifier& CurveId, const TArray<FRichCurveKey>& CurveKeys)`

**Error cases:**
- Asset not found
- Curve not found (call `AddCurve` first or error)
- Malformed `keys_json`

---

### 3.5 `get_curve_keys`

**Handler:** `HandleGetCurveKeys`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | AnimSequence asset path |
| `curve_name` | string | yes | Curve name |

**Return JSON:**
```json
{
  "curve_name": "MyCurve",
  "num_keys": 10,
  "keys": [
    {"time": 0.0, "value": 1.0, "interp_mode": "cubic"},
    {"time": 0.5, "value": 0.0, "interp_mode": "linear"}
  ]
}
```

**UE API calls:**
- `Seq->GetDataModel()->FindCurve(CurveId)` → `const FAnimCurveBase*`
- Cast to `const FFloatCurve*` → `.FloatCurve.GetConstRefOfKeys()`
- `FRichCurveKey::Time`, `Value`, `InterpMode`
- Map `ERichCurveInterpMode` → string

**Error cases:**
- Asset not found
- Curve not found

---

## Wave 4 — Skeleton + BlendSpace (6 actions)

**File:** `MonolithAnimationActions.cpp/.h`
**Dependencies:** May need `#include "Animation/BlendSpace1D.h"` for 1D detection
**Parallelizable with:** Waves 2, 3, 6, 8

---

### 4.1 `add_socket`

**Handler:** `HandleAddSocket`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | Skeleton asset path |
| `bone_name` | string | yes | Parent bone name |
| `socket_name` | string | yes | Name for the new socket |
| `location` | array | no | [x, y, z] relative location (default [0,0,0]) |
| `rotation` | array | no | [pitch, yaw, roll] relative rotation (default [0,0,0]) |
| `scale` | array | no | [x, y, z] relative scale (default [1,1,1]) |

**Return JSON:**
```json
{
  "socket_name": "weapon_r",
  "bone_name": "hand_r",
  "location": [10.0, 0.0, 0.0],
  "rotation": [0.0, 90.0, 0.0],
  "scale": [1.0, 1.0, 1.0]
}
```

**UE API calls:**
- Load as `USkeleton`
- Validate bone exists: `Skeleton->GetReferenceSkeleton().FindBoneIndex(FName(*BoneName)) != INDEX_NONE`
- Check socket doesn't already exist: `Skeleton->FindSocket(FName(*SocketName))` should be null
- Create: `USkeletalMeshSocket* Socket = NewObject<USkeletalMeshSocket>(Skeleton)`
- Set: `Socket->SocketName = FName(*SocketName)`, `Socket->BoneName = FName(*BoneName)`
- Set transform: `Socket->RelativeLocation`, `Socket->RelativeRotation`, `Socket->RelativeScale`
- Add: `Skeleton->Sockets.Add(Socket)` (Sockets is a public `TArray<TObjectPtr<USkeletalMeshSocket>>`)

**Error cases:**
- Skeleton not found
- Bone not found in reference skeleton
- Socket name already exists

---

### 4.2 `remove_socket`

**Handler:** `HandleRemoveSocket`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | Skeleton asset path |
| `socket_name` | string | yes | Socket name to remove |

**Return JSON:**
```json
{
  "removed_socket": "weapon_r",
  "success": true
}
```

**UE API calls:**
- Find socket: iterate `Skeleton->Sockets` to find by `SocketName`
- Remove: `Skeleton->Sockets.Remove(Socket)`

**Error cases:**
- Skeleton not found
- Socket not found

---

### 4.3 `set_socket_transform`

**Handler:** `HandleSetSocketTransform`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | Skeleton asset path |
| `socket_name` | string | yes | Socket name |
| `location` | array | no | [x, y, z] |
| `rotation` | array | no | [pitch, yaw, roll] |
| `scale` | array | no | [x, y, z] |

**Return JSON:**
```json
{
  "socket_name": "weapon_r",
  "location": [10.0, 0.0, 0.0],
  "rotation": [0.0, 90.0, 0.0],
  "scale": [1.0, 1.0, 1.0]
}
```

**UE API calls:**
- Find socket by name in `Skeleton->Sockets`
- Set whichever transform components are provided
- Parse array params via `TryGetArrayField` → extract 3 doubles

**Error cases:**
- Skeleton not found
- Socket not found
- At least one of location/rotation/scale must be provided

---

### 4.4 `get_skeleton_curves`

**Handler:** `HandleGetSkeletonCurves`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | Skeleton asset path |

**Return JSON:**
```json
{
  "asset_path": "/Game/...",
  "count": 5,
  "curves": [
    {
      "name": "EnableFootIK",
      "uid": 12345
    }
  ]
}
```

**UE API calls:**
- `USkeleton::GetSmartNameContainer(USkeleton::AnimCurveMappingName)` → `const FSmartNameMapping*`
- `FSmartNameMapping::Iterate()` — iterate all registered curve names
- Or: `USkeleton::GetCurveMetaDataContainer()` → iterate `GetCurveMetaData()`

**Error cases:**
- Skeleton not found

---

### 4.5 `set_blend_space_axis`

**Handler:** `HandleSetBlendSpaceAxis`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | BlendSpace asset path |
| `axis` | string | yes | "X" or "Y" |
| `name` | string | no | Display name |
| `min` | number | no | Minimum value |
| `max` | number | no | Maximum value |
| `grid_divisions` | integer | no | Grid divisions |
| `snap_to_grid` | bool | no | Snap to grid |
| `wrap_input` | bool | no | Wrap input |

**Return JSON:**
```json
{
  "axis": "X",
  "name": "Speed",
  "min": 0.0,
  "max": 600.0,
  "grid_divisions": 4,
  "snap_to_grid": false,
  "wrap_input": false
}
```

**UE API calls:**
- Load as `UBlendSpace`
- Map axis: `"X"` → index 0, `"Y"` → index 1
- Access: `BS->BlendParameters[AxisIndex]` — this is a `FBlendParameter` struct, direct member (not getter)
  - Note: `BlendParameters` is a fixed-size C array `FBlendParameter BlendParameters[3]` inside `UBlendSpace`
- Modify fields: `.DisplayName`, `.Min`, `.Max`, `.GridNum`, `.bSnapToGrid`, `.bWrapInput`
- After modification: `BS->ValidateSampleData()` to revalidate samples against new axis ranges

**Error cases:**
- Asset not found
- Invalid axis (not "X" or "Y")
- min >= max

---

### 4.6 `set_root_motion_settings`

**Handler:** `HandleSetRootMotionSettings`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | AnimSequence asset path |
| `enable_root_motion` | bool | no | Enable/disable root motion extraction |
| `root_motion_lock` | string | no | "AnimFirstFrame", "Zero", "RefPose" |
| `force_root_lock` | bool | no | Force root lock even without root motion |

**Return JSON:**
```json
{
  "asset_path": "/Game/...",
  "enable_root_motion": true,
  "root_motion_lock": "AnimFirstFrame",
  "force_root_lock": false
}
```

**UE API calls:**
- Load as `UAnimSequence`
- `Seq->bEnableRootMotion` — `bool` UPROPERTY (line 320 of AnimSequence.h)
- `Seq->RootMotionRootLock` — `TEnumAsByte<ERootMotionRootLock::Type>` UPROPERTY (line 324)
  - `ERootMotionRootLock::AnimFirstFrame`, `ERootMotionRootLock::Zero`, `ERootMotionRootLock::RefPose`
- `Seq->bForceRootLock` — `bool` UPROPERTY (line 328)
- At least one param must be provided

**Error cases:**
- Asset not found
- Invalid `root_motion_lock` string
- No params provided

---

## Wave 5 — Creation + Montage (6 actions)

**File:** `MonolithAnimationActions.cpp/.h`
**Dependencies:** May need `AssetTools` module for `FAssetToolsModule::GetModule().Get().CreateAsset()`
**Parallelizable with:** Waves 6, 8

---

### 5.1 `create_sequence`

**Handler:** `HandleCreateSequence`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | Desired output path (e.g. "/Game/Anims/NewSeq") |
| `skeleton_path` | string | yes | Skeleton asset path |
| `num_frames` | integer | no | Number of frames (default 30) |
| `frame_rate` | number | no | Frame rate (default 30.0) |

**Return JSON:**
```json
{
  "asset_path": "/Game/Anims/NewSeq",
  "skeleton": "/Game/.../Skeleton",
  "num_frames": 30,
  "frame_rate": 30.0
}
```

**UE API calls:**
- Load skeleton: `FMonolithAssetUtils::LoadAssetByPath<USkeleton>(SkeletonPath)`
- Create package: `UPackage* Package = CreatePackage(*PackagePath)` — use unique name per CLAUDE.md warning
- Create object: `UAnimSequence* NewSeq = NewObject<UAnimSequence>(Package, FName(*AssetName), RF_Public | RF_Standalone)`
- `NewSeq->SetSkeleton(Skeleton)` — sets the skeleton reference
- `IAnimationDataController& Controller = NewSeq->GetController()`
- `Controller.InitializeModel()` — init the data model
- `Controller.SetFrameRate(FFrameRate(FrameRateInt, 1))` — set frame rate
- `Controller.SetNumberOfFrames(NumFrames)` — set frame count
- `FAssetRegistryModule::AssetCreated(NewSeq)` — notify asset registry
- `NewSeq->MarkPackageDirty()`

**Error cases:**
- Skeleton not found
- Package already exists at path (warn, return existing)
- Invalid frame rate or num_frames <= 0

**Build.cs change:** Add `"AssetTools"` to PrivateDependencyModuleNames (may already be pulled transitively)

---

### 5.2 `duplicate_sequence`

**Handler:** `HandleDuplicateSequence`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `source_path` | string | yes | Source AnimSequence path |
| `dest_path` | string | yes | Destination path |

**Return JSON:**
```json
{
  "source_path": "/Game/Anims/Walk",
  "dest_path": "/Game/Anims/Walk_Copy",
  "success": true
}
```

**UE API calls:**
- Load source: `FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(SourcePath)`
- `UEditorAssetLibrary::DuplicateAsset(SourcePath, DestPath)` or manual:
  - `ObjectTools::DuplicateSingleObject(SourceObj, DestPackageName, DestGroupName, DestAssetName)`
- Or simpler: `AssetToolsModule.Get().DuplicateAsset(DestPath, DestPackagePath, SourceObj)`

**Error cases:**
- Source not found
- Destination already exists

**Build.cs change:** May need `"EditorScriptingUtilities"` if using `UEditorAssetLibrary`

---

### 5.3 `create_montage`

**Handler:** `HandleCreateMontage`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | Desired output path |
| `skeleton_path` | string | yes | Skeleton asset path |
| `slot_name` | string | no | Slot name (default "DefaultGroup.DefaultSlot") |

**Return JSON:**
```json
{
  "asset_path": "/Game/Anims/NewMontage",
  "skeleton": "/Game/.../Skeleton",
  "slot_name": "DefaultGroup.DefaultSlot"
}
```

**UE API calls:**
- Create package + `NewObject<UAnimMontage>`
- `NewMontage->SetSkeleton(Skeleton)`
- The constructor already calls `AddSlot(FAnimSlotGroup::DefaultSlotName)` — so default slot exists
- If custom slot: `NewMontage->SlotAnimTracks[0].SlotName = FName(*SlotName)`
- Add default section:
  ```cpp
  FCompositeSection NewSection;
  NewSection.SectionName = TEXT("Default");
  NewSection.SetTime(0.0f);
  NewMontage->CompositeSections.Add(NewSection);
  ```
- `FAssetRegistryModule::AssetCreated(NewMontage)`
- `NewMontage->MarkPackageDirty()`

**Error cases:**
- Skeleton not found
- Path already exists

---

### 5.4 `set_montage_blend`

**Handler:** `HandleSetMontageBlend`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | Montage asset path |
| `blend_in_time` | number | no | Blend in time in seconds |
| `blend_out_time` | number | no | Blend out time in seconds |
| `blend_out_trigger_time` | number | no | Time from end to trigger blend out (-1 = use blend out time) |
| `enable_auto_blend_out` | bool | no | Auto blend out on end |

**Return JSON:**
```json
{
  "asset_path": "/Game/...",
  "blend_in_time": 0.25,
  "blend_out_time": 0.25,
  "blend_out_trigger_time": -1.0,
  "enable_auto_blend_out": true
}
```

**UE API calls:**
- `Montage->BlendIn.SetBlendTime(BlendInTime)` — `FAlphaBlend::SetBlendTime(float)`
- `Montage->BlendOut.SetBlendTime(BlendOutTime)`
- `Montage->BlendOutTriggerTime = BlendOutTriggerTime`
- `Montage->bEnableAutoBlendOut = bAutoBlendOut`

**Error cases:**
- Asset not found
- Not a montage
- No params provided
- Negative blend times

---

### 5.5 `add_montage_slot`

**Handler:** `HandleAddMontageSlot`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | Montage asset path |
| `slot_name` | string | yes | Slot name to add |

**Return JSON:**
```json
{
  "slot_name": "UpperBody",
  "slot_index": 1,
  "total_slots": 2
}
```

**UE API calls:**
- `Montage->AddSlot(FName(*SlotName))` — returns `FSlotAnimationTrack&`
  - Signature: `FSlotAnimationTrack& UAnimMontage::AddSlot(FName SlotName)` (AnimMontage.cpp:82)
  - Appends to `SlotAnimTracks`, sets `SlotName`

**Error cases:**
- Asset not found
- Slot name already exists (check `SlotAnimTracks` first)

---

### 5.6 `set_montage_slot`

**Handler:** `HandleSetMontageSlot`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | Montage asset path |
| `slot_index` | integer | yes | Index of slot track to rename |
| `slot_name` | string | yes | New slot name |

**Return JSON:**
```json
{
  "slot_index": 0,
  "old_slot_name": "DefaultGroup.DefaultSlot",
  "new_slot_name": "UpperBody"
}
```

**UE API calls:**
- Validate index: `SlotAnimTracks.IsValidIndex(slot_index)`
- Save old: `FName OldName = Montage->SlotAnimTracks[slot_index].SlotName`
- Set: `Montage->SlotAnimTracks[slot_index].SlotName = FName(*SlotName)`

**Error cases:**
- Asset not found
- Invalid slot index

---

## Wave 6 — PoseSearch (5 actions)

**File:** NEW — `MonolithPoseSearchActions.cpp` / `MonolithPoseSearchActions.h`
**Module:** Could remain in MonolithAnimation or create a new `MonolithPoseSearch` module
**Recommendation:** Keep in MonolithAnimation but in separate files. Add PoseSearch dependency to Build.cs.
**Parallelizable with:** All other waves (separate files)

**Key includes:**
```cpp
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearch/PoseSearchDerivedData.h"
```

**Registration:** Add a new static class `FMonolithPoseSearchActions` with its own `RegisterActions()`, called from `MonolithAnimationModule.cpp::StartupModule()`.

---

### 6.1 `get_pose_search_schema`

**Handler:** `HandleGetPoseSearchSchema`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | PoseSearchSchema asset path |

**Return JSON:**
```json
{
  "asset_path": "/Game/...",
  "skeleton": "/Game/.../Skeleton",
  "sample_rate": 30.0,
  "channels": [
    {
      "index": 0,
      "type": "Position",
      "bone": "root",
      "weight": 1.0
    }
  ]
}
```

**UE API calls:**
- Load as `UPoseSearchSchema`
- `Schema->GetSkeleton()` → `USkeleton*`
- `Schema->GetSamplingRate()` → `float`
- `Schema->GetChannels()` — iterate channel descriptors
- Channel info: type, bone references, weights

**Error cases:**
- Asset not found
- Not a PoseSearchSchema

---

### 6.2 `get_pose_search_database`

**Handler:** `HandleGetPoseSearchDatabase`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | PoseSearchDatabase asset path |

**Return JSON:**
```json
{
  "asset_path": "/Game/...",
  "schema": "/Game/.../Schema",
  "sequence_count": 15,
  "sequences": [
    {
      "index": 0,
      "animation": "/Game/.../Walk",
      "enabled": true,
      "sampling_range_start": 0.0,
      "sampling_range_end": -1.0
    }
  ]
}
```

**UE API calls:**
- Load as `UPoseSearchDatabase`
- `Database->Schema` → asset path
- `Database->AnimationAssets` — `TArray` of animation entries
- Each entry: `GetAnimationAsset()`, `IsEnabled()`, `GetSamplingRange()`

**Error cases:**
- Asset not found
- Not a PoseSearchDatabase

---

### 6.3 `add_database_sequence`

**Handler:** `HandleAddDatabaseSequence`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | PoseSearchDatabase asset path |
| `anim_path` | string | yes | Animation sequence to add |
| `enabled` | bool | no | Enable for search (default true) |

**Return JSON:**
```json
{
  "index": 15,
  "animation": "/Game/.../Run",
  "enabled": true
}
```

**UE API calls:**
- Load database and animation
- Create new entry and add to `Database->AnimationAssets`
- Set animation reference and enabled state
- `Database->MarkPackageDirty()`

**Error cases:**
- Database not found
- Animation not found
- Skeleton mismatch
- Animation already in database

---

### 6.4 `remove_database_sequence`

**Handler:** `HandleRemoveDatabaseSequence`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | PoseSearchDatabase asset path |
| `sequence_index` | integer | yes | Index to remove |

**Return JSON:**
```json
{
  "removed_index": 5,
  "removed_animation": "/Game/.../OldAnim",
  "remaining_count": 14
}
```

**UE API calls:**
- Validate index against `AnimationAssets` array
- `Database->AnimationAssets.RemoveAt(sequence_index)`
- `Database->MarkPackageDirty()`

**Error cases:**
- Database not found
- Invalid index

---

### 6.5 `get_database_stats`

**Handler:** `HandleGetDatabaseStats`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | PoseSearchDatabase asset path |

**Return JSON:**
```json
{
  "asset_path": "/Game/...",
  "sequence_count": 15,
  "total_pose_count": 4500,
  "schema": "/Game/.../Schema",
  "is_valid": true
}
```

**UE API calls:**
- Load database
- `Database->AnimationAssets.Num()` — sequence count
- `Database->GetSearchIndex()` — may need to check if built
- Search index statistics: pose count, memory footprint

**Error cases:**
- Database not found
- Search index not built (return partial data with `is_valid: false`)

---

## Wave 7 — Anim Modifiers + Composites (5 actions)

**File:** `MonolithAnimationActions.cpp/.h`
**Dependencies:** Need `"AnimationModifiers"` module in Build.cs
**Parallelizable with:** Waves 6, 8

**Key include:** `#include "AnimationModifier.h"`

---

### 7.1 `apply_anim_modifier`

**Handler:** `HandleApplyAnimModifier`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | AnimSequence asset path |
| `modifier_class` | string | yes | Modifier class name or path |

**Return JSON:**
```json
{
  "asset_path": "/Game/.../Walk",
  "modifier_class": "MotionExtractorModifier",
  "success": true
}
```

**UE API calls:**
- Load `UAnimSequence`
- Find modifier class: `FindFirstObject<UClass>(*ModifierClass)` — must be subclass of `UAnimationModifier`
- Create instance: `NewObject<UAnimationModifier>(GetTransientPackage(), ModifierUClass)`
- Apply: `Modifier->ApplyToAnimationSequence(Seq)`
  - Signature: `void UAnimationModifier::ApplyToAnimationSequence(UAnimSequence* AnimSequence) const` (AnimationModifier.cpp:47)
  - This internally calls `Seq->Modify()`, creates a scoped bracket on the controller, calls `OnApply`

**Error cases:**
- Asset not found
- Modifier class not found / not subclass of UAnimationModifier
- Apply throws errors (the function logs warnings/errors internally)

---

### 7.2 `list_anim_modifiers`

**Handler:** `HandleListAnimModifiers`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | AnimSequence asset path |

**Return JSON:**
```json
{
  "asset_path": "/Game/.../Walk",
  "count": 2,
  "modifiers": [
    {
      "index": 0,
      "class": "MotionExtractorModifier",
      "revision_guid": "abc123"
    }
  ]
}
```

**UE API calls:**
- Load `UAnimSequence`
- `Seq->GetAssetUserData<UAnimationModifiersAssetUserData>()` → `UAnimationModifiersAssetUserData*`
- If non-null: `AssetUserData->GetAnimationModifierInstances()` → `const TArray<UAnimationModifier*>&`
- Each modifier: `GetClass()->GetName()`, `GetRevisionGuid()`

**Error cases:**
- Asset not found
- No modifiers applied (return empty array, not error)

---

### 7.3 `get_composite_info`

**Handler:** `HandleGetCompositeInfo`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | AnimComposite asset path |

**Return JSON:**
```json
{
  "asset_path": "/Game/...",
  "skeleton": "/Game/.../Skeleton",
  "duration": 5.0,
  "segment_count": 3,
  "segments": [
    {
      "index": 0,
      "animation": "/Game/.../Walk",
      "start_pos": 0.0,
      "anim_start_time": 0.0,
      "anim_end_time": 2.0,
      "play_rate": 1.0,
      "looping_count": 1
    }
  ]
}
```

**UE API calls:**
- Load as `UAnimComposite`
- `Composite->AnimationTrack` — `FAnimTrack` UPROPERTY (AnimComposite.h:30)
- `FAnimTrack::AnimSegments` — `TArray<FAnimSegment>`
- `FAnimSegment::GetAnimReference()` → `UAnimSequenceBase*`
- `FAnimSegment::StartPos` — `float`
- `FAnimSegment::AnimStartTime` — `float`
- `FAnimSegment::AnimEndTime` — `float`
- `FAnimSegment::AnimPlayRate` — `float`
- `FAnimSegment::LoopingCount` — `int32`

**Error cases:**
- Asset not found
- Not an AnimComposite

**Includes needed:** `#include "Animation/AnimComposite.h"`

---

### 7.4 `add_composite_segment`

**Handler:** `HandleAddCompositeSegment`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | AnimComposite asset path |
| `anim_path` | string | yes | Animation to add as segment |
| `start_pos` | number | no | Start position in composite timeline (default: end of last segment) |
| `play_rate` | number | no | Play rate (default 1.0) |
| `looping_count` | integer | no | Loop count (default 1) |

**Return JSON:**
```json
{
  "index": 3,
  "animation": "/Game/.../Run",
  "start_pos": 2.5,
  "duration": 1.5,
  "play_rate": 1.0,
  "looping_count": 1
}
```

**UE API calls:**
- Load `UAnimComposite` and `UAnimSequenceBase` (the clip)
- Create: `FAnimSegment NewSegment`
- `NewSegment.SetAnimReference(AnimClip, true)` — set reference and validate
- `NewSegment.StartPos = StartPos` (or calculate from end of last segment)
- `NewSegment.AnimPlayRate = PlayRate`
- `NewSegment.LoopingCount = LoopingCount`
- Add: `Composite->AnimationTrack.AnimSegments.Add(NewSegment)`
- `Composite->SetCompositeLength(Composite->AnimationTrack.GetLength())`

**Error cases:**
- Composite not found
- Animation not found
- Skeleton mismatch

---

### 7.5 `remove_composite_segment`

**Handler:** `HandleRemoveCompositeSegment`

**Params:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `asset_path` | string | yes | AnimComposite asset path |
| `segment_index` | integer | yes | Index of segment to remove |

**Return JSON:**
```json
{
  "removed_index": 1,
  "removed_animation": "/Game/.../Walk",
  "remaining_count": 2
}
```

**UE API calls:**
- Validate index: `AnimationTrack.AnimSegments.IsValidIndex(segment_index)`
- Save ref before removal
- `Composite->AnimationTrack.AnimSegments.RemoveAt(segment_index)`
- `Composite->SetCompositeLength(Composite->AnimationTrack.GetLength())`

**Error cases:**
- Composite not found
- Invalid segment index

---

## Wave 8 — IKRig/ControlRig (DEFERRED)

These actions are deferred because IKRig and ControlRig have complex module dependencies and rapidly evolving APIs in UE 5.7. Document the API surface for future implementation.

### API Surface — IKRig

**Module:** `IKRig` (Runtime), `IKRigEditor` (Editor)

**Key classes:**
- `UIKRigDefinition` — the asset, contains goals, solvers, bone settings
- `UIKRigController` — editor-time controller for modifying IKRig assets
  - `AddGoal()`, `RemoveGoal()`, `SetGoalBone()`
  - `AddSolver()`, `RemoveSolver()`, `SetSolverEnabled()`
  - `SetBoneExcluded()`, `GetBoneSettings()`
- `UIKRetargeter` — retarget mapping between two IKRigs
  - `UIKRetargetController` — editor controller for retarget assets

**Potential actions:**
1. `get_ikrig_info` — goals, solvers, bone exclusions, skeleton
2. `get_ikrig_goals` — list all goals with bone/target info
3. `add_ikrig_goal` — add goal to bone
4. `remove_ikrig_goal` — remove goal
5. `get_retarget_info` — source/target rigs, chain mappings

### API Surface — ControlRig

**Module:** `ControlRig` (Runtime), `ControlRigEditor` (Editor)

**Key classes:**
- `UControlRigBlueprint` — the asset (subclass of `UBlueprint`)
- `URigHierarchy` — bone/control/null/connector hierarchy
  - `GetBones()`, `GetControls()`, `GetNulls()`
- `FRigControlElement` — individual control (transform, float, vector, etc.)
- `URigHierarchyController` — editor-time modification

**Potential actions:**
1. `get_control_rig_info` — hierarchy summary, control count, bone count
2. `get_controls` — list all controls with types, defaults, limits
3. `get_rig_hierarchy` — full bone/control/null tree
4. `set_control_value` — set default value for a control
5. `get_rig_graph` — RigVM graph nodes (similar to ABP graph reading)

**Why deferred:**
- ControlRig uses RigVM, not standard Blueprint VM — different graph traversal patterns
- IKRig controller API may have breaking changes between 5.6 and 5.7
- Both modules have significant additional Build.cs dependencies (`ControlRig`, `ControlRigDeveloper`, `RigVM`, `IKRig`, `IKRigDeveloper`, `IKRigEditor`)

---

## Build.cs Changes

### Waves 1-5 (MonolithAnimationActions.cpp)

Current Build.cs is sufficient for Waves 1-4. Changes needed:

**Wave 5 — add:**
```csharp
"AssetTools",           // UAssetToolsModule for create/duplicate
"AssetRegistry"         // FAssetRegistryModule::AssetCreated
```

**Wave 6 — add:**
```csharp
"PoseSearch"            // UPoseSearchDatabase, UPoseSearchSchema
```

**Wave 7 — add:**
```csharp
"AnimationModifiers"    // UAnimationModifier, UAnimationModifiersAssetUserData
```

### Full Build.cs after all waves (excluding Wave 8):

```csharp
PrivateDependencyModuleNames.AddRange(new string[]
{
    "MonolithCore",
    "UnrealEd",
    "AnimGraph",
    "AnimGraphRuntime",
    "BlueprintGraph",
    "AnimationBlueprintLibrary",
    "Json",
    "JsonUtilities",
    // Wave 5
    "AssetTools",
    "AssetRegistry",
    // Wave 6
    "PoseSearch",
    // Wave 7
    "AnimationModifiers"
});
```

---

## File Conflict Avoidance Plan

### File Ownership per Wave

| File | Waves | Notes |
|------|-------|-------|
| `MonolithAnimationActions.cpp` | 1, 2, 3, 4, 5, 7 | Main implementation file |
| `MonolithAnimationActions.h` | 1, 2, 3, 4, 5, 7 | Header declarations |
| `MonolithPoseSearchActions.cpp` | 6 | NEW file, separate class |
| `MonolithPoseSearchActions.h` | 6 | NEW file, separate class |
| `MonolithAnimationModule.cpp` | 6 (add registration call) | Minimal change |
| `MonolithAnimation.Build.cs` | 5, 6, 7 | Dependency additions |

### Parallel Implementation Strategy

**Safe to parallelize (no file conflicts):**
- Wave 6 (PoseSearch) — entirely new files, can run alongside anything
- Wave 8 (IKRig/ControlRig) — deferred/doc only

**Must be serialized (same files):**
- Waves 1 → 2 → 3 → 4 → 5 → 7 all touch `MonolithAnimationActions.cpp/.h`
- However, within the .cpp, each wave adds to different sections. If implementers coordinate:
  - **Wave 1** adds read handlers at the bottom of the file (after existing handlers)
  - **Wave 2** adds notify handlers right after Wave 1's section
  - **Wave 3** adds curve handlers after Wave 2
  - etc.
- The **registration block** in `RegisterActions()` must be serialized — each wave appends its registrations

**Recommended execution order:**
1. **Phase A (parallel):** Wave 1 + Wave 6 — zero conflicts
2. **Phase B (serial):** Wave 2, Wave 3 — builds on Wave 1 file structure
3. **Phase C (parallel):** Wave 4 + Wave 5 — Wave 4 is read/write on skeleton/blendspace, Wave 5 is creation/montage — minimal overlap in the handler code, BUT both touch the same file and registration block, so coordinate carefully
4. **Phase D (serial):** Wave 7 — needs AnimationModifiers dependency, builds on final file state

### Header Growth Plan

After all waves, `MonolithAnimationActions.h` will have ~43 new handler declarations. Organize by section:

```cpp
class FMonolithAnimationActions
{
public:
    static void RegisterActions(FMonolithToolRegistry& Registry);

    // --- Existing (23) ---
    // ... (unchanged) ...

    // --- Wave 1: Read Actions (8) ---
    static FMonolithActionResult HandleGetSequenceInfo(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleGetSequenceNotifies(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleGetBoneTrackKeys(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleGetSequenceCurves(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleGetMontageInfo(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleGetBlendSpaceInfo(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleGetSkeletonSockets(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleGetAbpInfo(const TSharedPtr<FJsonObject>& Params);

    // --- Wave 2: Notify CRUD (4) ---
    static FMonolithActionResult HandleAddNotify(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleAddNotifyState(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleRemoveNotify(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleSetNotifyTrack(const TSharedPtr<FJsonObject>& Params);

    // --- Wave 3: Curve CRUD (5) ---
    static FMonolithActionResult HandleListCurves(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleAddCurve(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleRemoveCurve(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleSetCurveKeys(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleGetCurveKeys(const TSharedPtr<FJsonObject>& Params);

    // --- Wave 4: Skeleton + BlendSpace (6) ---
    static FMonolithActionResult HandleAddSocket(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleRemoveSocket(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleSetSocketTransform(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleGetSkeletonCurves(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleSetBlendSpaceAxis(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleSetRootMotionSettings(const TSharedPtr<FJsonObject>& Params);

    // --- Wave 5: Creation + Montage (6) ---
    static FMonolithActionResult HandleCreateSequence(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleDuplicateSequence(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleCreateMontage(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleSetMontageBlend(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleAddMontageSlot(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleSetMontageSlot(const TSharedPtr<FJsonObject>& Params);

    // --- Wave 7: Anim Modifiers + Composites (5) ---
    static FMonolithActionResult HandleApplyAnimModifier(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleListAnimModifiers(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleGetCompositeInfo(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleAddCompositeSegment(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleRemoveCompositeSegment(const TSharedPtr<FJsonObject>& Params);
};

// Separate class in separate files
class FMonolithPoseSearchActions
{
public:
    static void RegisterActions(FMonolithToolRegistry& Registry);

    // --- Wave 6: PoseSearch (5) ---
    static FMonolithActionResult HandleGetPoseSearchSchema(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleGetPoseSearchDatabase(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleAddDatabaseSequence(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleRemoveDatabaseSequence(const TSharedPtr<FJsonObject>& Params);
    static FMonolithActionResult HandleGetDatabaseStats(const TSharedPtr<FJsonObject>& Params);
};
```

### Module Startup Update

After all waves, `MonolithAnimationModule.cpp`:

```cpp
void FMonolithAnimationModule::StartupModule()
{
    FMonolithAnimationActions::RegisterActions(FMonolithToolRegistry::Get());
    FMonolithPoseSearchActions::RegisterActions(FMonolithToolRegistry::Get());  // Wave 6
    UE_LOG(LogMonolith, Verbose, TEXT("Monolith — Animation module loaded (66 actions)"));
}
```

### Action Count Summary

| Wave | Actions | Running Total |
|------|---------|---------------|
| Existing | 23 | 23 |
| Wave 1 | 8 | 31 |
| Wave 2 | 4 | 35 |
| Wave 3 | 5 | 40 |
| Wave 4 | 6 | 46 |
| Wave 5 | 6 | 52 |
| Wave 6 | 5 | 57 |
| Wave 7 | 5 | 62 |
| Wave 8 | DEFERRED | 62 |
| **Total** | **39 new** | **62** |

Note: The original request said ~43 new actions. The count is 39 implemented + Wave 8 deferred (~5-10 potential actions documented but not designed). If Wave 8 actions are eventually implemented, the total would reach ~67-72.
