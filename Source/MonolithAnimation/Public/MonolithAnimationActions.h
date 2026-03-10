#pragma once

#include "CoreMinimal.h"
#include "MonolithToolRegistry.h"

class UAnimMontage;
class UBlendSpace;
class UAnimBlueprint;
class UAnimSequence;
class USkeleton;
class USkeletalMesh;

/**
 * Animation domain action handlers for Monolith.
 * Ported from AnimationMCPReaderLibrary — 23 proven actions.
 */
class FMonolithAnimationActions
{
public:
	/** Register all animation actions with the tool registry */
	static void RegisterActions(FMonolithToolRegistry& Registry);

	// --- Montage Sections (4) ---
	static FMonolithActionResult HandleAddMontageSection(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleDeleteMontageSection(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleSetSectionNext(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleSetSectionTime(const TSharedPtr<FJsonObject>& Params);

	// --- BlendSpace Samples (3) ---
	static FMonolithActionResult HandleAddBlendSpaceSample(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleEditBlendSpaceSample(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleDeleteBlendSpaceSample(const TSharedPtr<FJsonObject>& Params);

	// --- ABP Graph Reading (7) ---
	static FMonolithActionResult HandleGetStateMachines(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleGetStateInfo(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleGetTransitions(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleGetBlendNodes(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleGetLinkedLayers(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleGetGraphs(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleGetNodes(const TSharedPtr<FJsonObject>& Params);

	// --- Notify Editing (2) ---
	static FMonolithActionResult HandleSetNotifyTime(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleSetNotifyDuration(const TSharedPtr<FJsonObject>& Params);

	// --- Bone Tracks (3) ---
	static FMonolithActionResult HandleSetBoneTrackKeys(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleAddBoneTrack(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleRemoveBoneTrack(const TSharedPtr<FJsonObject>& Params);

	// --- Virtual Bones (2) ---
	static FMonolithActionResult HandleAddVirtualBone(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleRemoveVirtualBones(const TSharedPtr<FJsonObject>& Params);

	// --- Skeleton Info (2) ---
	static FMonolithActionResult HandleGetSkeletonInfo(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleGetSkeletalMeshInfo(const TSharedPtr<FJsonObject>& Params);

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
