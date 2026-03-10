#include "MonolithAnimationActions.h"
#include "MonolithAssetUtils.h"
#include "MonolithParamSchema.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_LinkedAnimLayer.h"
#include "AnimationStateMachineGraph.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimStateConduitNode.h"
#include "AnimStateEntryNode.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimationBlueprintLibrary.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/CurveIdentifier.h"
#include "Animation/AnimComposite.h"
#include "AnimationModifier.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Editor.h"

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void FMonolithAnimationActions::RegisterActions(FMonolithToolRegistry& Registry)
{
	// Montage Sections
	Registry.RegisterAction(TEXT("animation"), TEXT("add_montage_section"),
		TEXT("Add a section to an animation montage"),
		FMonolithActionHandler::CreateStatic(&HandleAddMontageSection),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Montage asset path"))
			.Required(TEXT("section_name"), TEXT("string"), TEXT("Name for the new section"))
			.Required(TEXT("start_time"), TEXT("number"), TEXT("Start time in seconds"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("delete_montage_section"),
		TEXT("Delete a section from an animation montage by index"),
		FMonolithActionHandler::CreateStatic(&HandleDeleteMontageSection),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Montage asset path"))
			.Required(TEXT("section_index"), TEXT("integer"), TEXT("Index of the section to delete"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("set_section_next"),
		TEXT("Set the next section for a montage section"),
		FMonolithActionHandler::CreateStatic(&HandleSetSectionNext),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Montage asset path"))
			.Required(TEXT("section_name"), TEXT("string"), TEXT("Name of the section"))
			.Required(TEXT("next_section_name"), TEXT("string"), TEXT("Name of the next section to play"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("set_section_time"),
		TEXT("Set the start time of a montage section"),
		FMonolithActionHandler::CreateStatic(&HandleSetSectionTime),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Montage asset path"))
			.Required(TEXT("section_name"), TEXT("string"), TEXT("Name of the section"))
			.Required(TEXT("new_time"), TEXT("number"), TEXT("New start time in seconds"))
			.Build());

	// BlendSpace Samples
	Registry.RegisterAction(TEXT("animation"), TEXT("add_blendspace_sample"),
		TEXT("Add a sample to a blend space"),
		FMonolithActionHandler::CreateStatic(&HandleAddBlendSpaceSample),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("BlendSpace asset path"))
			.Required(TEXT("anim_path"), TEXT("string"), TEXT("Animation sequence asset path"))
			.Required(TEXT("x"), TEXT("number"), TEXT("X axis value"))
			.Required(TEXT("y"), TEXT("number"), TEXT("Y axis value"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("edit_blendspace_sample"),
		TEXT("Edit a blend space sample position and optionally its animation"),
		FMonolithActionHandler::CreateStatic(&HandleEditBlendSpaceSample),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("BlendSpace asset path"))
			.Required(TEXT("sample_index"), TEXT("integer"), TEXT("Index of the sample to edit"))
			.Required(TEXT("x"), TEXT("number"), TEXT("New X axis value"))
			.Required(TEXT("y"), TEXT("number"), TEXT("New Y axis value"))
			.Optional(TEXT("anim_path"), TEXT("string"), TEXT("New animation sequence asset path"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("delete_blendspace_sample"),
		TEXT("Delete a sample from a blend space by index"),
		FMonolithActionHandler::CreateStatic(&HandleDeleteBlendSpaceSample),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("BlendSpace asset path"))
			.Required(TEXT("sample_index"), TEXT("integer"), TEXT("Index of the sample to delete"))
			.Build());

	// ABP Graph Reading
	Registry.RegisterAction(TEXT("animation"), TEXT("get_state_machines"),
		TEXT("Get all state machines in an animation blueprint"),
		FMonolithActionHandler::CreateStatic(&HandleGetStateMachines),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_state_info"),
		TEXT("Get detailed info about a state in a state machine"),
		FMonolithActionHandler::CreateStatic(&HandleGetStateInfo),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path"))
			.Required(TEXT("machine_name"), TEXT("string"), TEXT("State machine name"))
			.Required(TEXT("state_name"), TEXT("string"), TEXT("State name"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_transitions"),
		TEXT("Get all transitions in a state machine"),
		FMonolithActionHandler::CreateStatic(&HandleGetTransitions),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path"))
			.Optional(TEXT("machine_name"), TEXT("string"), TEXT("Filter to a specific state machine"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_blend_nodes"),
		TEXT("Get blend nodes in an animation blueprint graph"),
		FMonolithActionHandler::CreateStatic(&HandleGetBlendNodes),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path"))
			.Optional(TEXT("graph_name"), TEXT("string"), TEXT("Filter to a specific graph"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_linked_layers"),
		TEXT("Get linked animation layers in an animation blueprint"),
		FMonolithActionHandler::CreateStatic(&HandleGetLinkedLayers),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_graphs"),
		TEXT("Get all graphs in an animation blueprint"),
		FMonolithActionHandler::CreateStatic(&HandleGetGraphs),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_nodes"),
		TEXT("Get animation nodes with optional class filter"),
		FMonolithActionHandler::CreateStatic(&HandleGetNodes),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path"))
			.Optional(TEXT("node_class_filter"), TEXT("string"), TEXT("Only include nodes whose class contains this substring"))
			.Optional(TEXT("graph_name"), TEXT("string"), TEXT("Filter to a specific graph"))
			.Build());

	// Notify Editing
	Registry.RegisterAction(TEXT("animation"), TEXT("set_notify_time"),
		TEXT("Set the trigger time of an animation notify"),
		FMonolithActionHandler::CreateStatic(&HandleSetNotifyTime),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation asset path"))
			.Required(TEXT("notify_index"), TEXT("integer"), TEXT("Index of the notify"))
			.Required(TEXT("new_time"), TEXT("number"), TEXT("New trigger time in seconds"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("set_notify_duration"),
		TEXT("Set the duration of a state animation notify"),
		FMonolithActionHandler::CreateStatic(&HandleSetNotifyDuration),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation asset path"))
			.Required(TEXT("notify_index"), TEXT("integer"), TEXT("Index of the notify"))
			.Required(TEXT("new_duration"), TEXT("number"), TEXT("New duration in seconds"))
			.Build());

	// Bone Tracks
	Registry.RegisterAction(TEXT("animation"), TEXT("set_bone_track_keys"),
		TEXT("Set position, rotation, and scale keys on a bone track"),
		FMonolithActionHandler::CreateStatic(&HandleSetBoneTrackKeys),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation sequence asset path"))
			.Required(TEXT("bone_name"), TEXT("string"), TEXT("Bone name"))
			.Required(TEXT("positions_json"), TEXT("string"), TEXT("JSON array of position keys"))
			.Required(TEXT("rotations_json"), TEXT("string"), TEXT("JSON array of rotation keys"))
			.Required(TEXT("scales_json"), TEXT("string"), TEXT("JSON array of scale keys"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("add_bone_track"),
		TEXT("Add a bone track to an animation sequence"),
		FMonolithActionHandler::CreateStatic(&HandleAddBoneTrack),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation sequence asset path"))
			.Required(TEXT("bone_name"), TEXT("string"), TEXT("Bone name to add"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("remove_bone_track"),
		TEXT("Remove a bone track from an animation sequence"),
		FMonolithActionHandler::CreateStatic(&HandleRemoveBoneTrack),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation sequence asset path"))
			.Required(TEXT("bone_name"), TEXT("string"), TEXT("Bone name to remove"))
			.Optional(TEXT("include_children"), TEXT("bool"), TEXT("Also remove child bone tracks"), TEXT("false"))
			.Build());

	// Virtual Bones
	Registry.RegisterAction(TEXT("animation"), TEXT("add_virtual_bone"),
		TEXT("Add a virtual bone to a skeleton"),
		FMonolithActionHandler::CreateStatic(&HandleAddVirtualBone),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Skeleton asset path"))
			.Required(TEXT("source_bone"), TEXT("string"), TEXT("Source bone name"))
			.Required(TEXT("target_bone"), TEXT("string"), TEXT("Target bone name"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("remove_virtual_bones"),
		TEXT("Remove virtual bones from a skeleton"),
		FMonolithActionHandler::CreateStatic(&HandleRemoveVirtualBones),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Skeleton asset path"))
			.Required(TEXT("bone_names"), TEXT("array"), TEXT("Array of virtual bone names to remove"))
			.Build());

	// Skeleton Info
	Registry.RegisterAction(TEXT("animation"), TEXT("get_skeleton_info"),
		TEXT("Get skeleton bone hierarchy and virtual bones"),
		FMonolithActionHandler::CreateStatic(&HandleGetSkeletonInfo),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Skeleton asset path"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_skeletal_mesh_info"),
		TEXT("Get skeletal mesh info including morph targets, sockets, LODs, and materials"),
		FMonolithActionHandler::CreateStatic(&HandleGetSkeletalMeshInfo),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Skeletal mesh asset path"))
			.Build());

	// Wave 1 — Read Actions
	Registry.RegisterAction(TEXT("animation"), TEXT("get_sequence_info"),
		TEXT("Get animation sequence metadata (duration, frames, root motion, compression, etc.)"),
		FMonolithActionHandler::CreateStatic(&HandleGetSequenceInfo),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequence asset path"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_sequence_notifies"),
		TEXT("Get all notifies on an animation asset"),
		FMonolithActionHandler::CreateStatic(&HandleGetSequenceNotifies),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation asset path (sequence, montage, composite)"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_bone_track_keys"),
		TEXT("Get position/rotation/scale keys for a bone track"),
		FMonolithActionHandler::CreateStatic(&HandleGetBoneTrackKeys),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequence asset path"))
			.Required(TEXT("bone_name"), TEXT("string"), TEXT("Bone name to read"))
			.Optional(TEXT("start_frame"), TEXT("integer"), TEXT("Start frame (default 0)"), TEXT("0"))
			.Optional(TEXT("end_frame"), TEXT("integer"), TEXT("End frame (default -1 = all)"), TEXT("-1"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_sequence_curves"),
		TEXT("Get float and transform curves on an animation sequence"),
		FMonolithActionHandler::CreateStatic(&HandleGetSequenceCurves),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequence asset path"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_montage_info"),
		TEXT("Get montage metadata including sections, slots, blend settings"),
		FMonolithActionHandler::CreateStatic(&HandleGetMontageInfo),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Montage asset path"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_blend_space_info"),
		TEXT("Get blend space samples and axis settings"),
		FMonolithActionHandler::CreateStatic(&HandleGetBlendSpaceInfo),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("BlendSpace asset path"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_skeleton_sockets"),
		TEXT("Get sockets from a skeleton or skeletal mesh"),
		FMonolithActionHandler::CreateStatic(&HandleGetSkeletonSockets),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Skeleton or SkeletalMesh asset path"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_abp_info"),
		TEXT("Get animation blueprint overview (skeleton, graphs, state machines, variables, interfaces)"),
		FMonolithActionHandler::CreateStatic(&HandleGetAbpInfo),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path"))
			.Build());

	// Wave 2 — Notify CRUD
	Registry.RegisterAction(TEXT("animation"), TEXT("add_notify"),
		TEXT("Add a point notify to an animation asset"),
		FMonolithActionHandler::CreateStatic(&HandleAddNotify),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation asset path"))
			.Required(TEXT("notify_class"), TEXT("string"), TEXT("Notify class name (e.g. AnimNotify_PlaySound)"))
			.Required(TEXT("time"), TEXT("number"), TEXT("Trigger time in seconds"))
			.Optional(TEXT("track_name"), TEXT("string"), TEXT("Notify track name"), TEXT("1"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("add_notify_state"),
		TEXT("Add a state notify (with duration) to an animation asset"),
		FMonolithActionHandler::CreateStatic(&HandleAddNotifyState),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation asset path"))
			.Required(TEXT("notify_class"), TEXT("string"), TEXT("NotifyState class name (e.g. AnimNotifyState_Trail)"))
			.Required(TEXT("time"), TEXT("number"), TEXT("Start time in seconds"))
			.Required(TEXT("duration"), TEXT("number"), TEXT("Duration in seconds"))
			.Optional(TEXT("track_name"), TEXT("string"), TEXT("Notify track name"), TEXT("1"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("remove_notify"),
		TEXT("Remove a notify by index from an animation asset"),
		FMonolithActionHandler::CreateStatic(&HandleRemoveNotify),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation asset path"))
			.Required(TEXT("notify_index"), TEXT("integer"), TEXT("Index of notify to remove"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("set_notify_track"),
		TEXT("Move a notify to a different track"),
		FMonolithActionHandler::CreateStatic(&HandleSetNotifyTrack),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Animation asset path"))
			.Required(TEXT("notify_index"), TEXT("integer"), TEXT("Index of notify to move"))
			.Required(TEXT("track_index"), TEXT("integer"), TEXT("Target track index"))
			.Build());

	// Wave 3 — Curve CRUD
	Registry.RegisterAction(TEXT("animation"), TEXT("list_curves"),
		TEXT("List all animation curves on a sequence (float and transform)"),
		FMonolithActionHandler::CreateStatic(&HandleListCurves),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequence asset path"))
			.Optional(TEXT("include_keys"), TEXT("bool"), TEXT("Include key data in response"), TEXT("false"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("add_curve"),
		TEXT("Add a float or transform curve to an animation sequence"),
		FMonolithActionHandler::CreateStatic(&HandleAddCurve),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequence asset path"))
			.Required(TEXT("curve_name"), TEXT("string"), TEXT("Name for the new curve"))
			.Optional(TEXT("curve_type"), TEXT("string"), TEXT("Float or Transform (default Float)"), TEXT("Float"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("remove_curve"),
		TEXT("Remove a curve from an animation sequence"),
		FMonolithActionHandler::CreateStatic(&HandleRemoveCurve),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequence asset path"))
			.Required(TEXT("curve_name"), TEXT("string"), TEXT("Name of curve to remove"))
			.Optional(TEXT("curve_type"), TEXT("string"), TEXT("Float or Transform (default Float)"), TEXT("Float"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("set_curve_keys"),
		TEXT("Set keys on a float curve (replaces existing keys)"),
		FMonolithActionHandler::CreateStatic(&HandleSetCurveKeys),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequence asset path"))
			.Required(TEXT("curve_name"), TEXT("string"), TEXT("Curve name"))
			.Required(TEXT("keys_json"), TEXT("string"), TEXT("JSON array: [{\"time\":0.0,\"value\":1.0,\"interp\":\"cubic\"}, ...]"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_curve_keys"),
		TEXT("Get all keys from a float curve"),
		FMonolithActionHandler::CreateStatic(&HandleGetCurveKeys),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequence asset path"))
			.Required(TEXT("curve_name"), TEXT("string"), TEXT("Curve name"))
			.Build());

	// Wave 4 — Skeleton + BlendSpace
	Registry.RegisterAction(TEXT("animation"), TEXT("add_socket"),
		TEXT("Add a socket to a skeleton"),
		FMonolithActionHandler::CreateStatic(&HandleAddSocket),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Skeleton asset path"))
			.Required(TEXT("bone_name"), TEXT("string"), TEXT("Parent bone name"))
			.Required(TEXT("socket_name"), TEXT("string"), TEXT("Name for the new socket"))
			.Optional(TEXT("location"), TEXT("array"), TEXT("[x, y, z] relative location"))
			.Optional(TEXT("rotation"), TEXT("array"), TEXT("[pitch, yaw, roll] relative rotation"))
			.Optional(TEXT("scale"), TEXT("array"), TEXT("[x, y, z] relative scale"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("remove_socket"),
		TEXT("Remove a socket from a skeleton"),
		FMonolithActionHandler::CreateStatic(&HandleRemoveSocket),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Skeleton asset path"))
			.Required(TEXT("socket_name"), TEXT("string"), TEXT("Socket name to remove"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("set_socket_transform"),
		TEXT("Set the transform of a skeleton socket"),
		FMonolithActionHandler::CreateStatic(&HandleSetSocketTransform),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Skeleton asset path"))
			.Required(TEXT("socket_name"), TEXT("string"), TEXT("Socket name"))
			.Optional(TEXT("location"), TEXT("array"), TEXT("[x, y, z] relative location"))
			.Optional(TEXT("rotation"), TEXT("array"), TEXT("[pitch, yaw, roll] relative rotation"))
			.Optional(TEXT("scale"), TEXT("array"), TEXT("[x, y, z] relative scale"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_skeleton_curves"),
		TEXT("Get all registered animation curve names from a skeleton"),
		FMonolithActionHandler::CreateStatic(&HandleGetSkeletonCurves),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Skeleton asset path"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("set_blend_space_axis"),
		TEXT("Configure a blend space axis (name, range, grid divisions)"),
		FMonolithActionHandler::CreateStatic(&HandleSetBlendSpaceAxis),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("BlendSpace asset path"))
			.Required(TEXT("axis"), TEXT("string"), TEXT("X or Y"))
			.Optional(TEXT("name"), TEXT("string"), TEXT("Display name for the axis"))
			.Optional(TEXT("min"), TEXT("number"), TEXT("Minimum axis value"))
			.Optional(TEXT("max"), TEXT("number"), TEXT("Maximum axis value"))
			.Optional(TEXT("grid_divisions"), TEXT("integer"), TEXT("Number of grid divisions"))
			.Optional(TEXT("snap_to_grid"), TEXT("bool"), TEXT("Snap samples to grid"))
			.Optional(TEXT("wrap_input"), TEXT("bool"), TEXT("Wrap input outside range"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("set_root_motion_settings"),
		TEXT("Configure root motion settings on an animation sequence"),
		FMonolithActionHandler::CreateStatic(&HandleSetRootMotionSettings),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequence asset path"))
			.Optional(TEXT("enable_root_motion"), TEXT("bool"), TEXT("Enable/disable root motion"))
			.Optional(TEXT("root_motion_lock"), TEXT("string"), TEXT("AnimFirstFrame, Zero, or RefPose"))
			.Optional(TEXT("force_root_lock"), TEXT("bool"), TEXT("Force root lock even without root motion"))
			.Build());

	// Wave 5 — Creation + Montage
	Registry.RegisterAction(TEXT("animation"), TEXT("create_sequence"),
		TEXT("Create a new empty animation sequence"),
		FMonolithActionHandler::CreateStatic(&HandleCreateSequence),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Asset path for the new sequence (e.g. /Game/Anims/MyAnim)"))
			.Required(TEXT("skeleton_path"), TEXT("string"), TEXT("Skeleton asset path"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("duplicate_sequence"),
		TEXT("Duplicate an animation sequence to a new path"),
		FMonolithActionHandler::CreateStatic(&HandleDuplicateSequence),
		FParamSchemaBuilder()
			.Required(TEXT("source_path"), TEXT("string"), TEXT("Source animation asset path"))
			.Required(TEXT("dest_path"), TEXT("string"), TEXT("Destination asset path"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("create_montage"),
		TEXT("Create a new animation montage with skeleton"),
		FMonolithActionHandler::CreateStatic(&HandleCreateMontage),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Asset path for the new montage"))
			.Required(TEXT("skeleton_path"), TEXT("string"), TEXT("Skeleton asset path"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("set_montage_blend"),
		TEXT("Set blend in/out times and auto blend out on a montage"),
		FMonolithActionHandler::CreateStatic(&HandleSetMontageBlend),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Montage asset path"))
			.Optional(TEXT("blend_in_time"), TEXT("number"), TEXT("Blend in duration in seconds"))
			.Optional(TEXT("blend_out_time"), TEXT("number"), TEXT("Blend out duration in seconds"))
			.Optional(TEXT("blend_out_trigger_time"), TEXT("number"), TEXT("Time before end to trigger blend out"))
			.Optional(TEXT("enable_auto_blend_out"), TEXT("bool"), TEXT("Enable automatic blend out"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("add_montage_slot"),
		TEXT("Add a slot track to a montage"),
		FMonolithActionHandler::CreateStatic(&HandleAddMontageSlot),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Montage asset path"))
			.Required(TEXT("slot_name"), TEXT("string"), TEXT("Slot name (e.g. DefaultSlot)"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("set_montage_slot"),
		TEXT("Rename a slot track on a montage by index"),
		FMonolithActionHandler::CreateStatic(&HandleSetMontageSlot),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Montage asset path"))
			.Required(TEXT("slot_index"), TEXT("integer"), TEXT("Index of the slot track"))
			.Required(TEXT("slot_name"), TEXT("string"), TEXT("New slot name"))
			.Build());

	// Wave 7 — Anim Modifiers + Composites
	Registry.RegisterAction(TEXT("animation"), TEXT("apply_anim_modifier"),
		TEXT("Apply an animation modifier class to a sequence"),
		FMonolithActionHandler::CreateStatic(&HandleApplyAnimModifier),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequence asset path"))
			.Required(TEXT("modifier_class"), TEXT("string"), TEXT("Modifier class name (e.g. UAnimationModifier_CreateCurve)"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("list_anim_modifiers"),
		TEXT("List animation modifiers applied to a sequence"),
		FMonolithActionHandler::CreateStatic(&HandleListAnimModifiers),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequence asset path"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("get_composite_info"),
		TEXT("Get segments and metadata from an animation composite"),
		FMonolithActionHandler::CreateStatic(&HandleGetCompositeInfo),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("AnimComposite asset path"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("add_composite_segment"),
		TEXT("Add a segment to an animation composite"),
		FMonolithActionHandler::CreateStatic(&HandleAddCompositeSegment),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("AnimComposite asset path"))
			.Required(TEXT("anim_path"), TEXT("string"), TEXT("Animation sequence to add"))
			.Optional(TEXT("start_pos"), TEXT("number"), TEXT("Start position in composite timeline"), TEXT("0.0"))
			.Optional(TEXT("play_rate"), TEXT("number"), TEXT("Playback rate"), TEXT("1.0"))
			.Optional(TEXT("looping_count"), TEXT("integer"), TEXT("Number of loops"), TEXT("1"))
			.Build());
	Registry.RegisterAction(TEXT("animation"), TEXT("remove_composite_segment"),
		TEXT("Remove a segment from an animation composite by index"),
		FMonolithActionHandler::CreateStatic(&HandleRemoveCompositeSegment),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("AnimComposite asset path"))
			.Required(TEXT("segment_index"), TEXT("integer"), TEXT("Index of the segment to remove"))
			.Build());
}

// ---------------------------------------------------------------------------
// Montage Sections
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithAnimationActions::HandleAddMontageSection(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString SectionName = Params->GetStringField(TEXT("section_name"));
	double StartTime = Params->GetNumberField(TEXT("start_time"));

	UAnimMontage* Montage = FMonolithAssetUtils::LoadAssetByPath<UAnimMontage>(AssetPath);
	if (!Montage) return FMonolithActionResult::Error(FString::Printf(TEXT("Montage not found: %s"), *AssetPath));

	GEditor->BeginTransaction(FText::FromString(TEXT("Add Montage Section")));
	Montage->Modify();

	int32 Index = Montage->AddAnimCompositeSection(FName(*SectionName), static_cast<float>(StartTime));

	GEditor->EndTransaction();

	if (Index == INDEX_NONE)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to add section '%s' (name may already exist)"), *SectionName));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("section_name"), SectionName);
	Root->SetNumberField(TEXT("index"), Index);
	Root->SetNumberField(TEXT("start_time"), StartTime);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleDeleteMontageSection(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	int32 SectionIndex = static_cast<int32>(Params->GetNumberField(TEXT("section_index")));

	UAnimMontage* Montage = FMonolithAssetUtils::LoadAssetByPath<UAnimMontage>(AssetPath);
	if (!Montage) return FMonolithActionResult::Error(FString::Printf(TEXT("Montage not found: %s"), *AssetPath));

	if (!Montage->IsValidSectionIndex(SectionIndex))
		return FMonolithActionResult::Error(FString::Printf(TEXT("Invalid section index: %d"), SectionIndex));

	if (Montage->CompositeSections.Num() <= 1)
		return FMonolithActionResult::Error(TEXT("Cannot delete the last remaining montage section"));

	FName SectionName = Montage->GetSectionName(SectionIndex);

	GEditor->BeginTransaction(FText::FromString(TEXT("Delete Montage Section")));
	Montage->Modify();
	bool bSuccess = Montage->DeleteAnimCompositeSection(SectionIndex);
	GEditor->EndTransaction();

	if (!bSuccess)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to delete section at index %d"), SectionIndex));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("deleted_section"), SectionName.ToString());
	Root->SetNumberField(TEXT("index"), SectionIndex);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleSetSectionNext(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString SectionName = Params->GetStringField(TEXT("section_name"));
	FString NextSectionName = Params->GetStringField(TEXT("next_section_name"));

	UAnimMontage* Montage = FMonolithAssetUtils::LoadAssetByPath<UAnimMontage>(AssetPath);
	if (!Montage) return FMonolithActionResult::Error(FString::Printf(TEXT("Montage not found: %s"), *AssetPath));

	int32 SectionIndex = Montage->GetSectionIndex(FName(*SectionName));
	if (SectionIndex == INDEX_NONE)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Section not found: %s"), *SectionName));

	GEditor->BeginTransaction(FText::FromString(TEXT("Set Section Next")));
	Montage->Modify();
	FCompositeSection& Section = Montage->GetAnimCompositeSection(SectionIndex);
	Section.NextSectionName = FName(*NextSectionName);
	GEditor->EndTransaction();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("section"), SectionName);
	Root->SetStringField(TEXT("next_section"), NextSectionName);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleSetSectionTime(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString SectionName = Params->GetStringField(TEXT("section_name"));
	float NewTime = static_cast<float>(Params->GetNumberField(TEXT("new_time")));

	UAnimMontage* Montage = FMonolithAssetUtils::LoadAssetByPath<UAnimMontage>(AssetPath);
	if (!Montage) return FMonolithActionResult::Error(FString::Printf(TEXT("Montage not found: %s"), *AssetPath));

	int32 SectionIndex = Montage->GetSectionIndex(FName(*SectionName));
	if (SectionIndex == INDEX_NONE)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Section not found: %s"), *SectionName));

	GEditor->BeginTransaction(FText::FromString(TEXT("Set Section Time")));
	Montage->Modify();
	FCompositeSection& Section = Montage->GetAnimCompositeSection(SectionIndex);
	Section.SetTime(NewTime);
	Section.Link(Montage, NewTime, 0);
	GEditor->EndTransaction();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("section"), SectionName);
	Root->SetNumberField(TEXT("new_time"), NewTime);
	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// BlendSpace Samples
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithAnimationActions::HandleAddBlendSpaceSample(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString AnimPath = Params->GetStringField(TEXT("anim_path"));
	float X = static_cast<float>(Params->GetNumberField(TEXT("x")));
	float Y = static_cast<float>(Params->GetNumberField(TEXT("y")));

	UBlendSpace* BS = FMonolithAssetUtils::LoadAssetByPath<UBlendSpace>(AssetPath);
	if (!BS) return FMonolithActionResult::Error(FString::Printf(TEXT("BlendSpace not found: %s"), *AssetPath));

	UAnimSequence* Anim = FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(AnimPath);
	if (!Anim) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimSequence not found: %s"), *AnimPath));

	USkeleton* BSSkeleton = BS->GetSkeleton();
	USkeleton* AnimSkeleton = Anim->GetSkeleton();
	if (BSSkeleton && AnimSkeleton && BSSkeleton != AnimSkeleton)
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Skeleton mismatch: blend space uses '%s' but animation uses '%s'"),
			*BSSkeleton->GetName(), *AnimSkeleton->GetName()));
	}

	GEditor->BeginTransaction(FText::FromString(TEXT("Add BlendSpace Sample")));
	BS->Modify();
	FVector SampleValue(X, Y, 0.0f);
	int32 Index = BS->AddSample(Anim, SampleValue);
	GEditor->EndTransaction();

	if (Index == INDEX_NONE)
		return FMonolithActionResult::Error(TEXT("Failed to add sample"));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("index"), Index);
	Root->SetStringField(TEXT("animation"), AnimPath);
	Root->SetNumberField(TEXT("x"), X);
	Root->SetNumberField(TEXT("y"), Y);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleEditBlendSpaceSample(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	int32 SampleIndex = static_cast<int32>(Params->GetNumberField(TEXT("sample_index")));
	float X = static_cast<float>(Params->GetNumberField(TEXT("x")));
	float Y = static_cast<float>(Params->GetNumberField(TEXT("y")));
	FString AnimPath;
	Params->TryGetStringField(TEXT("anim_path"), AnimPath);

	UBlendSpace* BS = FMonolithAssetUtils::LoadAssetByPath<UBlendSpace>(AssetPath);
	if (!BS) return FMonolithActionResult::Error(FString::Printf(TEXT("BlendSpace not found: %s"), *AssetPath));

	if (!BS->IsValidBlendSampleIndex(SampleIndex))
		return FMonolithActionResult::Error(FString::Printf(TEXT("Invalid sample index: %d"), SampleIndex));

	GEditor->BeginTransaction(FText::FromString(TEXT("Edit BlendSpace Sample")));
	BS->Modify();

	FVector NewValue(X, Y, 0.0f);
	BS->EditSampleValue(SampleIndex, NewValue);

	if (!AnimPath.IsEmpty())
	{
		UAnimSequence* Anim = FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(AnimPath);
		if (Anim)
		{
			BS->DeleteSample(SampleIndex);
			BS->AddSample(Anim, NewValue);
		}
	}

	GEditor->EndTransaction();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("index"), SampleIndex);
	Root->SetNumberField(TEXT("x"), X);
	Root->SetNumberField(TEXT("y"), Y);
	if (!AnimPath.IsEmpty()) Root->SetStringField(TEXT("animation"), AnimPath);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleDeleteBlendSpaceSample(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	int32 SampleIndex = static_cast<int32>(Params->GetNumberField(TEXT("sample_index")));

	UBlendSpace* BS = FMonolithAssetUtils::LoadAssetByPath<UBlendSpace>(AssetPath);
	if (!BS) return FMonolithActionResult::Error(FString::Printf(TEXT("BlendSpace not found: %s"), *AssetPath));

	if (!BS->IsValidBlendSampleIndex(SampleIndex))
		return FMonolithActionResult::Error(FString::Printf(TEXT("Invalid sample index: %d"), SampleIndex));

	GEditor->BeginTransaction(FText::FromString(TEXT("Delete BlendSpace Sample")));
	BS->Modify();
	bool bSuccess = BS->DeleteSample(SampleIndex);
	GEditor->EndTransaction();

	if (!bSuccess)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to delete sample at index %d"), SampleIndex));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("deleted_index"), SampleIndex);
	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// Notify Editing
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithAnimationActions::HandleSetNotifyTime(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	int32 NotifyIndex = static_cast<int32>(Params->GetNumberField(TEXT("notify_index")));
	float NewTime = static_cast<float>(Params->GetNumberField(TEXT("new_time")));

	UAnimSequenceBase* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequenceBase>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("Animation asset not found: %s"), *AssetPath));

	if (!Seq->Notifies.IsValidIndex(NotifyIndex))
		return FMonolithActionResult::Error(FString::Printf(TEXT("Invalid notify index: %d (total: %d)"), NotifyIndex, Seq->Notifies.Num()));

	GEditor->BeginTransaction(FText::FromString(TEXT("Set Notify Time")));
	Seq->Modify();
	Seq->Notifies[NotifyIndex].SetTime(NewTime);
	GEditor->EndTransaction();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("index"), NotifyIndex);
	Root->SetNumberField(TEXT("new_time"), NewTime);
	Root->SetStringField(TEXT("notify_name"), Seq->Notifies[NotifyIndex].NotifyName.ToString());
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleSetNotifyDuration(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	int32 NotifyIndex = static_cast<int32>(Params->GetNumberField(TEXT("notify_index")));
	float NewDuration = static_cast<float>(Params->GetNumberField(TEXT("new_duration")));

	UAnimSequenceBase* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequenceBase>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("Animation asset not found: %s"), *AssetPath));

	if (!Seq->Notifies.IsValidIndex(NotifyIndex))
		return FMonolithActionResult::Error(FString::Printf(TEXT("Invalid notify index: %d (total: %d)"), NotifyIndex, Seq->Notifies.Num()));

	if (!Seq->Notifies[NotifyIndex].NotifyStateClass)
		return FMonolithActionResult::Error(TEXT("Notify is not a state notify (no duration)"));

	GEditor->BeginTransaction(FText::FromString(TEXT("Set Notify Duration")));
	Seq->Modify();
	Seq->Notifies[NotifyIndex].SetDuration(NewDuration);
	GEditor->EndTransaction();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("index"), NotifyIndex);
	Root->SetNumberField(TEXT("new_duration"), NewDuration);
	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// Bone Tracks
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithAnimationActions::HandleAddBoneTrack(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString BoneName = Params->GetStringField(TEXT("bone_name"));

	UAnimSequence* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimSequence not found: %s"), *AssetPath));

	IAnimationDataController& Controller = Seq->GetController();

	GEditor->BeginTransaction(FText::FromString(TEXT("Add Bone Track")));
	Seq->Modify();
	Controller.AddBoneCurve(FName(*BoneName));
	GEditor->EndTransaction();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("bone_name"), BoneName);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleRemoveBoneTrack(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString BoneName = Params->GetStringField(TEXT("bone_name"));
	bool bIncludeChildren = false;
	Params->TryGetBoolField(TEXT("include_children"), bIncludeChildren);

	UAnimSequence* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimSequence not found: %s"), *AssetPath));

	IAnimationDataController& Controller = Seq->GetController();

	// Null check skeleton for include_children
	if (bIncludeChildren && !Seq->GetSkeleton())
		return FMonolithActionResult::Error(TEXT("Skeleton is null — cannot resolve children"));

	TArray<FName> BonesToRemove;
	FName TargetBone(*BoneName);
	BonesToRemove.Add(TargetBone);

	if (bIncludeChildren && Seq->GetSkeleton())
	{
		const FReferenceSkeleton& RefSkel = Seq->GetSkeleton()->GetReferenceSkeleton();
		int32 BoneIndex = RefSkel.FindBoneIndex(TargetBone);
		if (BoneIndex != INDEX_NONE)
		{
			for (int32 i = 0; i < RefSkel.GetNum(); ++i)
			{
				int32 ParentIdx = i;
				while (ParentIdx != INDEX_NONE)
				{
					if (ParentIdx == BoneIndex && i != BoneIndex)
					{
						BonesToRemove.Add(RefSkel.GetBoneName(i));
						break;
					}
					ParentIdx = RefSkel.GetParentIndex(ParentIdx);
				}
			}
		}
	}

	GEditor->BeginTransaction(FText::FromString(TEXT("Remove Bone Track")));
	Seq->Modify();
	int32 RemovedCount = 0;
	for (const FName& Bone : BonesToRemove)
	{
		if (Controller.RemoveBoneTrack(Bone))
		{
			++RemovedCount;
		}
	}
	GEditor->EndTransaction();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("bone_name"), BoneName);
	Root->SetBoolField(TEXT("include_children"), bIncludeChildren);
	Root->SetNumberField(TEXT("removed_count"), RemovedCount);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleSetBoneTrackKeys(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString BoneName = Params->GetStringField(TEXT("bone_name"));
	FString PositionsJson = Params->GetStringField(TEXT("positions_json"));
	FString RotationsJson = Params->GetStringField(TEXT("rotations_json"));
	FString ScalesJson = Params->GetStringField(TEXT("scales_json"));

	UAnimSequence* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimSequence not found: %s"), *AssetPath));

	// Parse positions: [[x,y,z], ...]
	TArray<FVector> Positions;
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PositionsJson);
		TArray<TSharedPtr<FJsonValue>> Arr;
		if (FJsonSerializer::Deserialize(Reader, Arr))
		{
			for (const auto& Val : Arr)
			{
				const TArray<TSharedPtr<FJsonValue>>* Inner;
				if (Val->TryGetArray(Inner) && Inner->Num() >= 3)
				{
					Positions.Add(FVector((*Inner)[0]->AsNumber(), (*Inner)[1]->AsNumber(), (*Inner)[2]->AsNumber()));
				}
			}
		}
	}

	// Parse rotations: [[x,y,z,w], ...]
	TArray<FQuat> Rotations;
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RotationsJson);
		TArray<TSharedPtr<FJsonValue>> Arr;
		if (FJsonSerializer::Deserialize(Reader, Arr))
		{
			for (const auto& Val : Arr)
			{
				const TArray<TSharedPtr<FJsonValue>>* Inner;
				if (Val->TryGetArray(Inner) && Inner->Num() >= 4)
				{
					Rotations.Add(FQuat((*Inner)[0]->AsNumber(), (*Inner)[1]->AsNumber(), (*Inner)[2]->AsNumber(), (*Inner)[3]->AsNumber()));
				}
			}
		}
	}

	// Parse scales: [[x,y,z], ...]
	TArray<FVector> Scales;
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ScalesJson);
		TArray<TSharedPtr<FJsonValue>> Arr;
		if (FJsonSerializer::Deserialize(Reader, Arr))
		{
			for (const auto& Val : Arr)
			{
				const TArray<TSharedPtr<FJsonValue>>* Inner;
				if (Val->TryGetArray(Inner) && Inner->Num() >= 3)
				{
					Scales.Add(FVector((*Inner)[0]->AsNumber(), (*Inner)[1]->AsNumber(), (*Inner)[2]->AsNumber()));
				}
			}
		}
	}

	IAnimationDataController& Controller = Seq->GetController();

	GEditor->BeginTransaction(FText::FromString(TEXT("Set Bone Track Keys")));
	Seq->Modify();

	int32 NumKeys = FMath::Max3(Positions.Num(), Rotations.Num(), Scales.Num());
	Controller.SetBoneTrackKeys(FName(*BoneName), Positions, Rotations, Scales);

	GEditor->EndTransaction();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("bone_name"), BoneName);
	Root->SetNumberField(TEXT("num_keys"), NumKeys);
	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// Virtual Bones
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithAnimationActions::HandleAddVirtualBone(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString SourceBone = Params->GetStringField(TEXT("source_bone"));
	FString TargetBone = Params->GetStringField(TEXT("target_bone"));

	USkeleton* Skeleton = FMonolithAssetUtils::LoadAssetByPath<USkeleton>(AssetPath);
	if (!Skeleton) return FMonolithActionResult::Error(FString::Printf(TEXT("Skeleton not found: %s"), *AssetPath));

	const FReferenceSkeleton& RefSkel = Skeleton->GetReferenceSkeleton();
	if (RefSkel.FindBoneIndex(FName(*SourceBone)) == INDEX_NONE)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Source bone not found in skeleton: %s"), *SourceBone));
	}
	if (RefSkel.FindBoneIndex(FName(*TargetBone)) == INDEX_NONE)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Target bone not found in skeleton: %s"), *TargetBone));
	}

	FName VBoneName;

	GEditor->BeginTransaction(FText::FromString(TEXT("Add Virtual Bone")));
	Skeleton->Modify();
	bool bSuccess = Skeleton->AddNewVirtualBone(FName(*SourceBone), FName(*TargetBone), VBoneName);
	GEditor->EndTransaction();

	if (!bSuccess)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to add virtual bone from '%s' to '%s'"), *SourceBone, *TargetBone));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("virtual_bone"), VBoneName.ToString());
	Root->SetStringField(TEXT("source"), SourceBone);
	Root->SetStringField(TEXT("target"), TargetBone);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleRemoveVirtualBones(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	USkeleton* Skeleton = FMonolithAssetUtils::LoadAssetByPath<USkeleton>(AssetPath);
	if (!Skeleton) return FMonolithActionResult::Error(FString::Printf(TEXT("Skeleton not found: %s"), *AssetPath));

	// Extract bone names from JSON array
	TArray<FString> BoneNames;
	const TArray<TSharedPtr<FJsonValue>>* BoneNamesArray;
	if (Params->TryGetArrayField(TEXT("bone_names"), BoneNamesArray))
	{
		for (const auto& Val : *BoneNamesArray)
		{
			BoneNames.Add(Val->AsString());
		}
	}

	GEditor->BeginTransaction(FText::FromString(TEXT("Remove Virtual Bones")));
	Skeleton->Modify();

	TArray<FString> Removed;
	TArray<FString> NotFound;
	if (BoneNames.Num() == 0)
	{
		TArray<FName> AllVBNames;
		for (const FVirtualBone& VB : Skeleton->GetVirtualBones())
		{
			AllVBNames.Add(VB.VirtualBoneName);
		}
		if (AllVBNames.Num() > 0)
		{
			Skeleton->RemoveVirtualBones(AllVBNames);
		}
		Removed.Add(TEXT("all"));
	}
	else
	{
		for (const FString& BoneName : BoneNames)
		{
			bool bFound = false;
			for (const FVirtualBone& VB : Skeleton->GetVirtualBones())
			{
				if (VB.VirtualBoneName == FName(*BoneName))
				{
					bFound = true;
					break;
				}
			}
			if (bFound)
			{
				Skeleton->RemoveVirtualBones({FName(*BoneName)});
				Removed.Add(BoneName);
			}
			else
			{
				NotFound.Add(BoneName);
			}
		}

		if (Removed.Num() == 0 && NotFound.Num() > 0)
		{
			GEditor->EndTransaction();
			return FMonolithActionResult::Error(TEXT("No virtual bones found matching the given names"));
		}
	}

	GEditor->EndTransaction();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> RemovedArr;
	for (const FString& R : Removed)
		RemovedArr.Add(MakeShared<FJsonValueString>(R));
	Root->SetArrayField(TEXT("removed"), RemovedArr);

	if (NotFound.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> NotFoundArr;
		for (const FString& NF : NotFound)
			NotFoundArr.Add(MakeShared<FJsonValueString>(NF));
		Root->SetArrayField(TEXT("not_found"), NotFoundArr);
	}

	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// Skeleton Info
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithAnimationActions::HandleGetSkeletonInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	USkeleton* Skeleton = FMonolithAssetUtils::LoadAssetByPath<USkeleton>(AssetPath);
	if (!Skeleton) return FMonolithActionResult::Error(FString::Printf(TEXT("Skeleton not found: %s"), *AssetPath));

	const FReferenceSkeleton& RefSkel = Skeleton->GetReferenceSkeleton();
	int32 BoneCount = RefSkel.GetNum();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetNumberField(TEXT("bone_count"), BoneCount);

	TArray<TSharedPtr<FJsonValue>> BonesArr;
	for (int32 i = 0; i < BoneCount; ++i)
	{
		TSharedPtr<FJsonObject> BoneObj = MakeShared<FJsonObject>();
		BoneObj->SetNumberField(TEXT("index"), i);
		BoneObj->SetStringField(TEXT("name"), RefSkel.GetBoneName(i).ToString());
		BoneObj->SetNumberField(TEXT("parent_index"), RefSkel.GetParentIndex(i));
		if (RefSkel.GetParentIndex(i) != INDEX_NONE)
			BoneObj->SetStringField(TEXT("parent_name"), RefSkel.GetBoneName(RefSkel.GetParentIndex(i)).ToString());
		BonesArr.Add(MakeShared<FJsonValueObject>(BoneObj));
	}
	Root->SetArrayField(TEXT("bones"), BonesArr);

	const TArray<FVirtualBone>& VBones = Skeleton->GetVirtualBones();
	TArray<TSharedPtr<FJsonValue>> VBonesArr;
	for (const FVirtualBone& VB : VBones)
	{
		TSharedPtr<FJsonObject> VBObj = MakeShared<FJsonObject>();
		VBObj->SetStringField(TEXT("name"), VB.VirtualBoneName.ToString());
		VBObj->SetStringField(TEXT("source"), VB.SourceBoneName.ToString());
		VBObj->SetStringField(TEXT("target"), VB.TargetBoneName.ToString());
		VBonesArr.Add(MakeShared<FJsonValueObject>(VBObj));
	}
	Root->SetArrayField(TEXT("virtual_bones"), VBonesArr);

	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleGetSkeletalMeshInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	USkeletalMesh* Mesh = FMonolithAssetUtils::LoadAssetByPath<USkeletalMesh>(AssetPath);
	if (!Mesh) return FMonolithActionResult::Error(FString::Printf(TEXT("SkeletalMesh not found: %s"), *AssetPath));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);

	USkeleton* Skel = Mesh->GetSkeleton();
	Root->SetStringField(TEXT("skeleton"), Skel ? Skel->GetPathName() : TEXT("None"));

	// Morph targets
	TArray<TSharedPtr<FJsonValue>> MorphArr;
	TArray<FString> MorphNames = Mesh->K2_GetAllMorphTargetNames();
	for (const FString& MorphName : MorphNames)
	{
		MorphArr.Add(MakeShared<FJsonValueString>(MorphName));
	}
	Root->SetArrayField(TEXT("morph_targets"), MorphArr);
	Root->SetNumberField(TEXT("morph_target_count"), MorphArr.Num());

	// Sockets
	TArray<TSharedPtr<FJsonValue>> SocketArr;
	for (int32 i = 0; i < Mesh->NumSockets(); ++i)
	{
		USkeletalMeshSocket* Sock = Mesh->GetSocketByIndex(i);
		if (!Sock) continue;
		TSharedPtr<FJsonObject> SockObj = MakeShared<FJsonObject>();
		SockObj->SetStringField(TEXT("name"), Sock->SocketName.ToString());
		SockObj->SetStringField(TEXT("bone"), Sock->BoneName.ToString());
		SocketArr.Add(MakeShared<FJsonValueObject>(SockObj));
	}
	Root->SetArrayField(TEXT("sockets"), SocketArr);
	Root->SetNumberField(TEXT("socket_count"), SocketArr.Num());

	// LOD count
	int32 LODCount = 0;
	if (FSkeletalMeshRenderData* RenderData = Mesh->GetResourceForRendering())
	{
		LODCount = RenderData->LODRenderData.Num();
	}
	Root->SetNumberField(TEXT("lod_count"), LODCount);

	// Materials
	TArray<TSharedPtr<FJsonValue>> MatArr;
	for (int32 i = 0; i < Mesh->GetMaterials().Num(); ++i)
	{
		const FSkeletalMaterial& MatSlot = Mesh->GetMaterials()[i];
		TSharedPtr<FJsonObject> MatObj = MakeShared<FJsonObject>();
		MatObj->SetNumberField(TEXT("index"), i);
		MatObj->SetStringField(TEXT("name"), MatSlot.MaterialSlotName.ToString());
		MatObj->SetStringField(TEXT("material"), MatSlot.MaterialInterface ? MatSlot.MaterialInterface->GetPathName() : TEXT("None"));
		MatArr.Add(MakeShared<FJsonValueObject>(MatObj));
	}
	Root->SetArrayField(TEXT("materials"), MatArr);
	Root->SetNumberField(TEXT("material_count"), MatArr.Num());

	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// ABP Graph Reading
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithAnimationActions::HandleGetStateMachines(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UAnimBlueprint* ABP = FMonolithAssetUtils::LoadAssetByPath<UAnimBlueprint>(AssetPath);
	if (!ABP) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimBlueprint not found: %s"), *AssetPath));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	TArray<TSharedPtr<FJsonValue>> MachinesArr;

	for (UEdGraph* Graph : ABP->FunctionGraphs)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node);
			if (!SMNode) continue;

			UAnimationStateMachineGraph* SMGraph = Cast<UAnimationStateMachineGraph>(SMNode->EditorStateMachineGraph);
			if (!SMGraph) continue;

			TSharedPtr<FJsonObject> MachineObj = MakeShared<FJsonObject>();
			FString SMName = SMNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
			int32 NewlineIdx = INDEX_NONE;
			if (SMName.FindChar(TEXT('\n'), NewlineIdx))
			{
				SMName.LeftInline(NewlineIdx);
			}
			MachineObj->SetStringField(TEXT("name"), SMName);
			MachineObj->SetStringField(TEXT("graph"), Graph->GetName());

			if (SMGraph->EntryNode)
			{
				for (UEdGraphPin* Pin : SMGraph->EntryNode->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
					{
						UAnimStateNode* EntryState = Cast<UAnimStateNode>(Pin->LinkedTo[0]->GetOwningNode());
						if (EntryState)
						{
							MachineObj->SetStringField(TEXT("entry_state"), EntryState->GetStateName());
						}
					}
				}
			}

			TArray<TSharedPtr<FJsonValue>> StatesArr;
			TArray<TSharedPtr<FJsonValue>> TransitionsArr;

			for (UEdGraphNode* SMChildNode : SMGraph->Nodes)
			{
				if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(SMChildNode))
				{
					TSharedPtr<FJsonObject> StateObj = MakeShared<FJsonObject>();
					StateObj->SetStringField(TEXT("name"), StateNode->GetStateName());
					TArray<TSharedPtr<FJsonValue>> PosArr;
					PosArr.Add(MakeShared<FJsonValueNumber>(StateNode->NodePosX));
					PosArr.Add(MakeShared<FJsonValueNumber>(StateNode->NodePosY));
					StateObj->SetArrayField(TEXT("position"), PosArr);
					StatesArr.Add(MakeShared<FJsonValueObject>(StateObj));
				}
				else if (UAnimStateTransitionNode* TransNode = Cast<UAnimStateTransitionNode>(SMChildNode))
				{
					TSharedPtr<FJsonObject> TransObj = MakeShared<FJsonObject>();
					UAnimStateNodeBase* PrevState = TransNode->GetPreviousState();
					UAnimStateNodeBase* NextState = TransNode->GetNextState();
					TransObj->SetStringField(TEXT("from"), PrevState ? PrevState->GetStateName() : TEXT("?"));
					TransObj->SetStringField(TEXT("to"), NextState ? NextState->GetStateName() : TEXT("?"));
					TransObj->SetStringField(TEXT("from_type"), PrevState ? (Cast<UAnimStateConduitNode>(PrevState) ? TEXT("conduit") : TEXT("state")) : TEXT("unknown"));
					TransObj->SetStringField(TEXT("to_type"), NextState ? (Cast<UAnimStateConduitNode>(NextState) ? TEXT("conduit") : TEXT("state")) : TEXT("unknown"));
					TransObj->SetNumberField(TEXT("cross_fade_duration"), TransNode->CrossfadeDuration);
					TransObj->SetStringField(TEXT("blend_mode"),
						TransNode->BlendMode == EAlphaBlendOption::Linear ? TEXT("Linear") :
						TransNode->BlendMode == EAlphaBlendOption::Cubic ? TEXT("Cubic") : TEXT("Other"));
					TransObj->SetBoolField(TEXT("bidirectional"), TransNode->Bidirectional);
					TransitionsArr.Add(MakeShared<FJsonValueObject>(TransObj));
				}
			}

			MachineObj->SetArrayField(TEXT("states"), StatesArr);
			MachineObj->SetArrayField(TEXT("transitions"), TransitionsArr);
			MachineObj->SetNumberField(TEXT("state_count"), StatesArr.Num());
			MachineObj->SetNumberField(TEXT("transition_count"), TransitionsArr.Num());
			MachinesArr.Add(MakeShared<FJsonValueObject>(MachineObj));
		}
	}

	Root->SetArrayField(TEXT("state_machines"), MachinesArr);
	Root->SetNumberField(TEXT("count"), MachinesArr.Num());
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleGetStateInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString MachineName = Params->GetStringField(TEXT("machine_name"));
	FString StateName = Params->GetStringField(TEXT("state_name"));

	if (MachineName.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("Missing required parameter: machine_name"));
	}
	if (StateName.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("Missing required parameter: state_name"));
	}

	UAnimBlueprint* ABP = FMonolithAssetUtils::LoadAssetByPath<UAnimBlueprint>(AssetPath);
	if (!ABP) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimBlueprint not found: %s"), *AssetPath));

	for (UEdGraph* Graph : ABP->FunctionGraphs)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node);
			if (!SMNode) continue;

			FString SMTitle = SMNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
			int32 NewlineIdx = INDEX_NONE;
			if (SMTitle.FindChar(TEXT('\n'), NewlineIdx))
			{
				SMTitle.LeftInline(NewlineIdx);
			}
			if (SMTitle != MachineName) continue;

			UAnimationStateMachineGraph* SMGraph = Cast<UAnimationStateMachineGraph>(SMNode->EditorStateMachineGraph);
			if (!SMGraph) continue;

			for (UEdGraphNode* SMChildNode : SMGraph->Nodes)
			{
				UAnimStateNode* StateNode = Cast<UAnimStateNode>(SMChildNode);
				if (!StateNode || StateNode->GetStateName() != StateName) continue;

				TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
				Root->SetStringField(TEXT("state_name"), StateName);
				Root->SetStringField(TEXT("machine_name"), MachineName);

				TArray<TSharedPtr<FJsonValue>> PosArr;
				PosArr.Add(MakeShared<FJsonValueNumber>(StateNode->NodePosX));
				PosArr.Add(MakeShared<FJsonValueNumber>(StateNode->NodePosY));
				Root->SetArrayField(TEXT("position"), PosArr);

				UEdGraph* StateGraph = StateNode->GetBoundGraph();
				TArray<TSharedPtr<FJsonValue>> NodesArr;
				if (StateGraph)
				{
					for (UEdGraphNode* InnerNode : StateGraph->Nodes)
					{
						UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(InnerNode);
						if (!AnimNode) continue;
						TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
						NodeObj->SetStringField(TEXT("class"), AnimNode->GetClass()->GetName());
						NodeObj->SetStringField(TEXT("title"), AnimNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
						NodesArr.Add(MakeShared<FJsonValueObject>(NodeObj));
					}
				}
				Root->SetArrayField(TEXT("nodes"), NodesArr);
				Root->SetNumberField(TEXT("node_count"), NodesArr.Num());

				return FMonolithActionResult::Success(Root);
			}
		}
	}

	return FMonolithActionResult::Error(FString::Printf(TEXT("State '%s' not found in machine '%s'"), *StateName, *MachineName));
}

FMonolithActionResult FMonolithAnimationActions::HandleGetTransitions(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString MachineName = Params->GetStringField(TEXT("machine_name"));

	UAnimBlueprint* ABP = FMonolithAssetUtils::LoadAssetByPath<UAnimBlueprint>(AssetPath);
	if (!ABP) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimBlueprint not found: %s"), *AssetPath));

	// Helper lambda to collect transitions from a state machine graph
	auto CollectTransitions = [](UAnimationStateMachineGraph* SMGraph, TArray<TSharedPtr<FJsonValue>>& OutArr)
	{
		for (UEdGraphNode* SMChildNode : SMGraph->Nodes)
		{
			UAnimStateTransitionNode* TransNode = Cast<UAnimStateTransitionNode>(SMChildNode);
			if (!TransNode) continue;

			TSharedPtr<FJsonObject> TransObj = MakeShared<FJsonObject>();
			UAnimStateNodeBase* PrevState = TransNode->GetPreviousState();
			UAnimStateNodeBase* NextState = TransNode->GetNextState();
			TransObj->SetStringField(TEXT("from"), PrevState ? PrevState->GetStateName() : TEXT("?"));
			TransObj->SetStringField(TEXT("to"), NextState ? NextState->GetStateName() : TEXT("?"));
			TransObj->SetStringField(TEXT("from_type"), PrevState ? (Cast<UAnimStateConduitNode>(PrevState) ? TEXT("conduit") : TEXT("state")) : TEXT("unknown"));
			TransObj->SetStringField(TEXT("to_type"), NextState ? (Cast<UAnimStateConduitNode>(NextState) ? TEXT("conduit") : TEXT("state")) : TEXT("unknown"));
			TransObj->SetNumberField(TEXT("cross_fade_duration"), TransNode->CrossfadeDuration);
			TransObj->SetStringField(TEXT("blend_mode"),
				TransNode->BlendMode == EAlphaBlendOption::Linear ? TEXT("Linear") :
				TransNode->BlendMode == EAlphaBlendOption::Cubic ? TEXT("Cubic") : TEXT("Other"));
			TransObj->SetBoolField(TEXT("bidirectional"), TransNode->Bidirectional);

			UEdGraph* RuleGraph = TransNode->GetBoundGraph();
			TArray<TSharedPtr<FJsonValue>> RuleNodesArr;
			if (RuleGraph)
			{
				for (UEdGraphNode* RuleNode : RuleGraph->Nodes)
				{
					TSharedPtr<FJsonObject> RuleObj = MakeShared<FJsonObject>();
					RuleObj->SetStringField(TEXT("class"), RuleNode->GetClass()->GetName());
					RuleObj->SetStringField(TEXT("title"), RuleNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
					RuleNodesArr.Add(MakeShared<FJsonValueObject>(RuleObj));
				}
			}
			TransObj->SetArrayField(TEXT("rule_nodes"), RuleNodesArr);
			OutArr.Add(MakeShared<FJsonValueObject>(TransObj));
		}
	};

	// If machine_name is empty, return transitions from ALL state machines
	bool bMatchAll = MachineName.IsEmpty();
	TArray<TSharedPtr<FJsonValue>> AllTransitionsArr;
	bool bFoundAny = false;

	for (UEdGraph* Graph : ABP->FunctionGraphs)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node);
			if (!SMNode) continue;

			FString SMTitle = SMNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
			int32 NewlineIdx = INDEX_NONE;
			if (SMTitle.FindChar(TEXT('\n'), NewlineIdx))
			{
				SMTitle.LeftInline(NewlineIdx);
			}
			if (!bMatchAll && SMTitle != MachineName) continue;

			UAnimationStateMachineGraph* SMGraph = Cast<UAnimationStateMachineGraph>(SMNode->EditorStateMachineGraph);
			if (!SMGraph) continue;

			bFoundAny = true;

			if (!bMatchAll)
			{
				// Specific machine match — return immediately
				TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
				Root->SetStringField(TEXT("machine_name"), MachineName);
				TArray<TSharedPtr<FJsonValue>> TransitionsArr;
				CollectTransitions(SMGraph, TransitionsArr);
				Root->SetArrayField(TEXT("transitions"), TransitionsArr);
				Root->SetNumberField(TEXT("count"), TransitionsArr.Num());
				return FMonolithActionResult::Success(Root);
			}

			// Collect from all machines
			CollectTransitions(SMGraph, AllTransitionsArr);
		}
	}

	// Return collected results (may be empty)
	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("machine_name"), MachineName);
	Result->SetArrayField(TEXT("transitions"), AllTransitionsArr);
	Result->SetNumberField(TEXT("count"), AllTransitionsArr.Num());
	return FMonolithActionResult::Success(Result);
}

FMonolithActionResult FMonolithAnimationActions::HandleGetBlendNodes(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString GraphName;
	Params->TryGetStringField(TEXT("graph_name"), GraphName);

	UAnimBlueprint* ABP = FMonolithAssetUtils::LoadAssetByPath<UAnimBlueprint>(AssetPath);
	if (!ABP) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimBlueprint not found: %s"), *AssetPath));

	for (UEdGraph* Graph : ABP->FunctionGraphs)
	{
		if (!Graph) continue;
		if (!GraphName.IsEmpty() && Graph->GetName() != GraphName) continue;

		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("graph_name"), Graph->GetName());
		TArray<TSharedPtr<FJsonValue>> NodesArr;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node);
			if (!AnimNode) continue;

			TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
			NodeObj->SetStringField(TEXT("class"), AnimNode->GetClass()->GetName());
			NodeObj->SetStringField(TEXT("title"), AnimNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

			TArray<TSharedPtr<FJsonValue>> PinsArr;
			for (UEdGraphPin* Pin : AnimNode->Pins)
			{
				if (!Pin || Pin->LinkedTo.Num() == 0) continue;
				TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
				PinObj->SetStringField(TEXT("name"), Pin->GetName());
				PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
				PinObj->SetNumberField(TEXT("connections"), Pin->LinkedTo.Num());
				PinsArr.Add(MakeShared<FJsonValueObject>(PinObj));
			}
			NodeObj->SetArrayField(TEXT("connected_pins"), PinsArr);
			NodesArr.Add(MakeShared<FJsonValueObject>(NodeObj));
		}

		Root->SetArrayField(TEXT("nodes"), NodesArr);
		Root->SetNumberField(TEXT("count"), NodesArr.Num());
		return FMonolithActionResult::Success(Root);
	}

	return FMonolithActionResult::Error(FString::Printf(TEXT("Graph '%s' not found"), *GraphName));
}

FMonolithActionResult FMonolithAnimationActions::HandleGetGraphs(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UAnimBlueprint* ABP = FMonolithAssetUtils::LoadAssetByPath<UAnimBlueprint>(AssetPath);
	if (!ABP) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimBlueprint not found: %s"), *AssetPath));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	TArray<TSharedPtr<FJsonValue>> GraphsArr;

	for (UEdGraph* Graph : ABP->FunctionGraphs)
	{
		if (!Graph) continue;

		TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
		GraphObj->SetStringField(TEXT("name"), Graph->GetName());
		GraphObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

		int32 AnimNodeCount = 0;
		int32 StateMachineCount = 0;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Cast<UAnimGraphNode_StateMachine>(Node)) StateMachineCount++;
			if (Cast<UAnimGraphNode_Base>(Node)) AnimNodeCount++;
		}
		GraphObj->SetNumberField(TEXT("anim_node_count"), AnimNodeCount);
		GraphObj->SetNumberField(TEXT("state_machine_count"), StateMachineCount);
		GraphsArr.Add(MakeShared<FJsonValueObject>(GraphObj));
	}

	Root->SetArrayField(TEXT("graphs"), GraphsArr);
	Root->SetNumberField(TEXT("count"), GraphsArr.Num());
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleGetNodes(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString NodeClassFilter;
	Params->TryGetStringField(TEXT("node_class_filter"), NodeClassFilter);
	FString GraphFilter;
	Params->TryGetStringField(TEXT("graph_name"), GraphFilter);

	UAnimBlueprint* ABP = FMonolithAssetUtils::LoadAssetByPath<UAnimBlueprint>(AssetPath);
	if (!ABP) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimBlueprint not found: %s"), *AssetPath));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	if (!NodeClassFilter.IsEmpty())
	{
		Root->SetStringField(TEXT("filter_class"), NodeClassFilter);
	}
	if (!GraphFilter.IsEmpty())
	{
		Root->SetStringField(TEXT("graph_name"), GraphFilter);
	}
	TArray<TSharedPtr<FJsonValue>> NodesArr;

	for (UEdGraph* Graph : ABP->FunctionGraphs)
	{
		if (!Graph) continue;
		if (!GraphFilter.IsEmpty() && Graph->GetName() != GraphFilter) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node);
			if (!AnimNode) continue;

			FString ClassName = AnimNode->GetClass()->GetName();
			if (!NodeClassFilter.IsEmpty() && !ClassName.Contains(NodeClassFilter)) continue;

			TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
			NodeObj->SetStringField(TEXT("class"), ClassName);
			NodeObj->SetStringField(TEXT("name"), AnimNode->GetName());
			NodeObj->SetStringField(TEXT("title"), AnimNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
			NodeObj->SetStringField(TEXT("graph"), Graph->GetName());

			TArray<TSharedPtr<FJsonValue>> PinsArr;
			for (UEdGraphPin* Pin : AnimNode->Pins)
			{
				if (!Pin || Pin->LinkedTo.Num() == 0) continue;
				TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
				PinObj->SetStringField(TEXT("name"), Pin->GetName());
				PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
				PinObj->SetNumberField(TEXT("connections"), Pin->LinkedTo.Num());
				PinsArr.Add(MakeShared<FJsonValueObject>(PinObj));
			}
			NodeObj->SetArrayField(TEXT("connected_pins"), PinsArr);
			NodesArr.Add(MakeShared<FJsonValueObject>(NodeObj));
		}
	}

	Root->SetArrayField(TEXT("nodes"), NodesArr);
	Root->SetNumberField(TEXT("count"), NodesArr.Num());
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleGetLinkedLayers(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UAnimBlueprint* ABP = FMonolithAssetUtils::LoadAssetByPath<UAnimBlueprint>(AssetPath);
	if (!ABP) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimBlueprint not found: %s"), *AssetPath));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	TArray<TSharedPtr<FJsonValue>> LayersArr;

	for (UEdGraph* Graph : ABP->FunctionGraphs)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_LinkedAnimLayer* LayerNode = Cast<UAnimGraphNode_LinkedAnimLayer>(Node);
			if (!LayerNode) continue;

			TSharedPtr<FJsonObject> LayerObj = MakeShared<FJsonObject>();
			LayerObj->SetStringField(TEXT("title"), LayerNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
			LayerObj->SetStringField(TEXT("graph"), Graph->GetName());
			LayerObj->SetStringField(TEXT("class"), LayerNode->GetClass()->GetName());
			LayersArr.Add(MakeShared<FJsonValueObject>(LayerObj));
		}
	}

	Root->SetArrayField(TEXT("linked_layers"), LayersArr);
	Root->SetNumberField(TEXT("count"), LayersArr.Num());
	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// Wave 1 — Read Actions
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithAnimationActions::HandleGetSequenceInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UAnimSequence* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimSequence not found: %s"), *AssetPath));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);

	USkeleton* Skel = Seq->GetSkeleton();
	Root->SetStringField(TEXT("skeleton"), Skel ? Skel->GetPathName() : TEXT("None"));

	Root->SetNumberField(TEXT("duration"), Seq->GetPlayLength());

	const IAnimationDataModel* DataModel = Seq->GetDataModel();
	if (DataModel)
	{
		Root->SetNumberField(TEXT("num_frames"), DataModel->GetNumberOfFrames());
		Root->SetNumberField(TEXT("num_keys"), DataModel->GetNumberOfKeys());
	}

	FFrameRate SampleRate = Seq->GetSamplingFrameRate();
	Root->SetNumberField(TEXT("sample_rate"), SampleRate.AsDecimal());
	Root->SetStringField(TEXT("frame_rate"), FString::Printf(TEXT("%d/%d"), SampleRate.Numerator, SampleRate.Denominator));

	Root->SetBoolField(TEXT("has_root_motion"), Seq->bEnableRootMotion);
	Root->SetBoolField(TEXT("force_root_lock"), Seq->bForceRootLock);

	FString RootMotionLockStr;
	switch (Seq->RootMotionRootLock.GetValue())
	{
	case ERootMotionRootLock::RefPose: RootMotionLockStr = TEXT("RefPose"); break;
	case ERootMotionRootLock::AnimFirstFrame: RootMotionLockStr = TEXT("AnimFirstFrame"); break;
	case ERootMotionRootLock::Zero: RootMotionLockStr = TEXT("Zero"); break;
	default: RootMotionLockStr = TEXT("Unknown"); break;
	}
	Root->SetStringField(TEXT("root_motion_lock"), RootMotionLockStr);

	FString AdditiveStr;
	switch (Seq->AdditiveAnimType.GetValue())
	{
	case EAdditiveAnimationType::AAT_None: AdditiveStr = TEXT("None"); break;
	case EAdditiveAnimationType::AAT_LocalSpaceBase: AdditiveStr = TEXT("LocalSpaceBase"); break;
	case EAdditiveAnimationType::AAT_RotationOffsetMeshSpace: AdditiveStr = TEXT("RotationOffsetMeshSpace"); break;
	default: AdditiveStr = TEXT("Unknown"); break;
	}
	Root->SetStringField(TEXT("additive_type"), AdditiveStr);

	Root->SetStringField(TEXT("interpolation"),
		Seq->Interpolation == EAnimInterpolationType::Linear ? TEXT("Linear") : TEXT("Step"));

	Root->SetNumberField(TEXT("rate_scale"), Seq->RateScale);
	Root->SetBoolField(TEXT("is_looping"), Seq->bLoop);

	// Compression scheme - use CurveCompressionCodec as a proxy if available
	Root->SetStringField(TEXT("compression_scheme"), TEXT("Default"));

	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleGetSequenceNotifies(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UAnimSequenceBase* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequenceBase>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("Animation asset not found: %s"), *AssetPath));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);

	TArray<TSharedPtr<FJsonValue>> NotifiesArr;
	for (int32 i = 0; i < Seq->Notifies.Num(); ++i)
	{
		const FAnimNotifyEvent& Event = Seq->Notifies[i];
		TSharedPtr<FJsonObject> NotifyObj = MakeShared<FJsonObject>();

		NotifyObj->SetNumberField(TEXT("index"), i);
		NotifyObj->SetStringField(TEXT("name"), Event.NotifyName.ToString());
		NotifyObj->SetNumberField(TEXT("time"), Event.GetTime());
		NotifyObj->SetNumberField(TEXT("duration"), Event.GetDuration());
		NotifyObj->SetNumberField(TEXT("trigger_time_offset"), Event.TriggerTimeOffset);
		NotifyObj->SetStringField(TEXT("notify_class"),
			Event.Notify ? Event.Notify->GetClass()->GetName() : TEXT(""));
		NotifyObj->SetStringField(TEXT("state_class"),
			Event.NotifyStateClass ? Event.NotifyStateClass->GetClass()->GetName() : TEXT(""));
		NotifyObj->SetNumberField(TEXT("track_index"), Event.TrackIndex);

		// Get track name if valid
		FString TrackName;
		if (Seq->AnimNotifyTracks.IsValidIndex(Event.TrackIndex))
		{
			TrackName = Seq->AnimNotifyTracks[Event.TrackIndex].TrackName.ToString();
		}
		NotifyObj->SetStringField(TEXT("track_name"), TrackName);
		NotifyObj->SetBoolField(TEXT("is_state"), Event.NotifyStateClass != nullptr);

		NotifiesArr.Add(MakeShared<FJsonValueObject>(NotifyObj));
	}

	Root->SetArrayField(TEXT("notifies"), NotifiesArr);
	Root->SetNumberField(TEXT("count"), NotifiesArr.Num());
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleGetBoneTrackKeys(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString BoneName = Params->GetStringField(TEXT("bone_name"));
	int32 StartFrame = 0;
	int32 EndFrame = -1;

	double TempVal;
	if (Params->TryGetNumberField(TEXT("start_frame"), TempVal))
		StartFrame = static_cast<int32>(TempVal);
	if (Params->TryGetNumberField(TEXT("end_frame"), TempVal))
		EndFrame = static_cast<int32>(TempVal);

	UAnimSequence* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimSequence not found: %s"), *AssetPath));

	const IAnimationDataModel* DataModel = Seq->GetDataModel();
	if (!DataModel) return FMonolithActionResult::Error(TEXT("No animation data model"));

	const FRawAnimSequenceTrack* RawTrackPtr = nullptr;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const TArray<FBoneAnimationTrack>& BoneTracks = DataModel->GetBoneAnimationTracks();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	for (const FBoneAnimationTrack& Track : BoneTracks)
	{
		if (Track.Name == FName(*BoneName))
		{
			RawTrackPtr = &Track.InternalTrackData;
			break;
		}
	}
	if (!RawTrackPtr)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Bone track not found: %s"), *BoneName));

	const FRawAnimSequenceTrack& RawTrack = *RawTrackPtr;

	int32 NumPosKeys = RawTrack.PosKeys.Num();
	int32 NumRotKeys = RawTrack.RotKeys.Num();
	int32 NumScaleKeys = RawTrack.ScaleKeys.Num();
	int32 MaxKeys = FMath::Max3(NumPosKeys, NumRotKeys, NumScaleKeys);

	if (EndFrame < 0 || EndFrame >= MaxKeys)
		EndFrame = MaxKeys - 1;
	if (StartFrame < 0) StartFrame = 0;
	if (StartFrame > EndFrame)
		return FMonolithActionResult::Error(FString::Printf(TEXT("start_frame (%d) > end_frame (%d)"), StartFrame, EndFrame));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("bone_name"), BoneName);
	Root->SetNumberField(TEXT("num_keys"), MaxKeys);

	// Positions
	TArray<TSharedPtr<FJsonValue>> PosArr;
	for (int32 i = StartFrame; i <= EndFrame && i < NumPosKeys; ++i)
	{
		TArray<TSharedPtr<FJsonValue>> Vec;
		Vec.Add(MakeShared<FJsonValueNumber>(RawTrack.PosKeys[i].X));
		Vec.Add(MakeShared<FJsonValueNumber>(RawTrack.PosKeys[i].Y));
		Vec.Add(MakeShared<FJsonValueNumber>(RawTrack.PosKeys[i].Z));
		PosArr.Add(MakeShared<FJsonValueArray>(Vec));
	}
	Root->SetArrayField(TEXT("positions"), PosArr);

	// Rotations
	TArray<TSharedPtr<FJsonValue>> RotArr;
	for (int32 i = StartFrame; i <= EndFrame && i < NumRotKeys; ++i)
	{
		TArray<TSharedPtr<FJsonValue>> Quat;
		Quat.Add(MakeShared<FJsonValueNumber>(RawTrack.RotKeys[i].X));
		Quat.Add(MakeShared<FJsonValueNumber>(RawTrack.RotKeys[i].Y));
		Quat.Add(MakeShared<FJsonValueNumber>(RawTrack.RotKeys[i].Z));
		Quat.Add(MakeShared<FJsonValueNumber>(RawTrack.RotKeys[i].W));
		RotArr.Add(MakeShared<FJsonValueArray>(Quat));
	}
	Root->SetArrayField(TEXT("rotations"), RotArr);

	// Scales
	TArray<TSharedPtr<FJsonValue>> ScaleArr;
	for (int32 i = StartFrame; i <= EndFrame && i < NumScaleKeys; ++i)
	{
		TArray<TSharedPtr<FJsonValue>> Vec;
		Vec.Add(MakeShared<FJsonValueNumber>(RawTrack.ScaleKeys[i].X));
		Vec.Add(MakeShared<FJsonValueNumber>(RawTrack.ScaleKeys[i].Y));
		Vec.Add(MakeShared<FJsonValueNumber>(RawTrack.ScaleKeys[i].Z));
		ScaleArr.Add(MakeShared<FJsonValueArray>(Vec));
	}
	Root->SetArrayField(TEXT("scales"), ScaleArr);

	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleGetSequenceCurves(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UAnimSequence* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimSequence not found: %s"), *AssetPath));

	const IAnimationDataModel* DataModel = Seq->GetDataModel();
	if (!DataModel) return FMonolithActionResult::Error(TEXT("No animation data model"));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);

	TArray<TSharedPtr<FJsonValue>> CurvesArr;

	const FAnimationCurveData& CurveData = DataModel->GetCurveData();

	// Float curves
	for (const FFloatCurve& Curve : CurveData.FloatCurves)
	{
		TSharedPtr<FJsonObject> CurveObj = MakeShared<FJsonObject>();
		CurveObj->SetStringField(TEXT("name"), Curve.GetName().ToString());
		CurveObj->SetStringField(TEXT("type"), TEXT("Float"));
		CurveObj->SetNumberField(TEXT("num_keys"), Curve.FloatCurve.GetNumKeys());
		CurvesArr.Add(MakeShared<FJsonValueObject>(CurveObj));
	}

	// Transform curves
	for (const FTransformCurve& Curve : CurveData.TransformCurves)
	{
		TSharedPtr<FJsonObject> CurveObj = MakeShared<FJsonObject>();
		CurveObj->SetStringField(TEXT("name"), Curve.GetName().ToString());
		CurveObj->SetStringField(TEXT("type"), TEXT("Transform"));
		CurveObj->SetNumberField(TEXT("num_keys"), 0); // Transform curves don't have a simple key count
		CurvesArr.Add(MakeShared<FJsonValueObject>(CurveObj));
	}

	Root->SetArrayField(TEXT("curves"), CurvesArr);
	Root->SetNumberField(TEXT("count"), CurvesArr.Num());
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleGetMontageInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UAnimMontage* Montage = FMonolithAssetUtils::LoadAssetByPath<UAnimMontage>(AssetPath);
	if (!Montage) return FMonolithActionResult::Error(FString::Printf(TEXT("Montage not found: %s"), *AssetPath));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);

	USkeleton* Skel = Montage->GetSkeleton();
	Root->SetStringField(TEXT("skeleton"), Skel ? Skel->GetPathName() : TEXT("None"));
	Root->SetNumberField(TEXT("duration"), Montage->GetPlayLength());
	Root->SetNumberField(TEXT("rate_scale"), Montage->RateScale);
	Root->SetNumberField(TEXT("blend_in_time"), Montage->BlendIn.GetBlendTime());
	Root->SetNumberField(TEXT("blend_out_time"), Montage->BlendOut.GetBlendTime());
	Root->SetNumberField(TEXT("blend_out_trigger_time"), Montage->BlendOutTriggerTime);
	Root->SetBoolField(TEXT("enable_auto_blend_out"), Montage->bEnableAutoBlendOut);

	// Sections
	TArray<TSharedPtr<FJsonValue>> SectionsArr;
	for (int32 i = 0; i < Montage->CompositeSections.Num(); ++i)
	{
		const FCompositeSection& Section = Montage->CompositeSections[i];
		TSharedPtr<FJsonObject> SectionObj = MakeShared<FJsonObject>();
		SectionObj->SetNumberField(TEXT("index"), i);
		SectionObj->SetStringField(TEXT("name"), Section.SectionName.ToString());
		SectionObj->SetNumberField(TEXT("time"), Section.GetTime());
		SectionObj->SetStringField(TEXT("next_section"), Section.NextSectionName.ToString());
		SectionsArr.Add(MakeShared<FJsonValueObject>(SectionObj));
	}
	Root->SetArrayField(TEXT("sections"), SectionsArr);

	// Slots
	TArray<TSharedPtr<FJsonValue>> SlotsArr;
	for (int32 i = 0; i < Montage->SlotAnimTracks.Num(); ++i)
	{
		TSharedPtr<FJsonObject> SlotObj = MakeShared<FJsonObject>();
		SlotObj->SetNumberField(TEXT("index"), i);
		SlotObj->SetStringField(TEXT("slot_name"), Montage->SlotAnimTracks[i].SlotName.ToString());
		SlotsArr.Add(MakeShared<FJsonValueObject>(SlotObj));
	}
	Root->SetArrayField(TEXT("slots"), SlotsArr);

	Root->SetNumberField(TEXT("notify_count"), Montage->Notifies.Num());

	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleGetBlendSpaceInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UBlendSpace* BS = FMonolithAssetUtils::LoadAssetByPath<UBlendSpace>(AssetPath);
	if (!BS) return FMonolithActionResult::Error(FString::Printf(TEXT("BlendSpace not found: %s"), *AssetPath));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);

	USkeleton* Skel = BS->GetSkeleton();
	Root->SetStringField(TEXT("skeleton"), Skel ? Skel->GetPathName() : TEXT("None"));
	Root->SetBoolField(TEXT("is_1d"), BS->IsA<UBlendSpace1D>());

	// Axis X
	auto MakeAxisObj = [](const FBlendParameter& Param) -> TSharedPtr<FJsonObject>
	{
		TSharedPtr<FJsonObject> AxisObj = MakeShared<FJsonObject>();
		AxisObj->SetStringField(TEXT("name"), Param.DisplayName);
		AxisObj->SetNumberField(TEXT("min"), Param.Min);
		AxisObj->SetNumberField(TEXT("max"), Param.Max);
		AxisObj->SetNumberField(TEXT("grid_divisions"), Param.GridNum);
		AxisObj->SetBoolField(TEXT("snap_to_grid"), Param.bSnapToGrid);
		AxisObj->SetBoolField(TEXT("wrap_input"), Param.bWrapInput);
		return AxisObj;
	};

	Root->SetObjectField(TEXT("axis_x"), MakeAxisObj(BS->GetBlendParameter(0)));
	Root->SetObjectField(TEXT("axis_y"), MakeAxisObj(BS->GetBlendParameter(1)));

	// Samples
	const TArray<FBlendSample>& Samples = BS->GetBlendSamples();
	TArray<TSharedPtr<FJsonValue>> SamplesArr;
	for (int32 i = 0; i < Samples.Num(); ++i)
	{
		const FBlendSample& Sample = Samples[i];
		TSharedPtr<FJsonObject> SampleObj = MakeShared<FJsonObject>();
		SampleObj->SetNumberField(TEXT("index"), i);
		SampleObj->SetStringField(TEXT("animation"),
			Sample.Animation ? Sample.Animation->GetPathName() : TEXT("None"));
		SampleObj->SetNumberField(TEXT("x"), Sample.SampleValue.X);
		SampleObj->SetNumberField(TEXT("y"), Sample.SampleValue.Y);
		SampleObj->SetNumberField(TEXT("rate_scale"), Sample.RateScale);
		SamplesArr.Add(MakeShared<FJsonValueObject>(SampleObj));
	}
	Root->SetArrayField(TEXT("samples"), SamplesArr);
	Root->SetNumberField(TEXT("sample_count"), SamplesArr.Num());

	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleGetSkeletonSockets(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	// Try skeleton first, then skeletal mesh
	TArray<USkeletalMeshSocket*> SocketList;
	FString SourceType;

	USkeleton* Skeleton = FMonolithAssetUtils::LoadAssetByPath<USkeleton>(AssetPath);
	if (Skeleton)
	{
		SocketList = Skeleton->Sockets;
		SourceType = TEXT("Skeleton");
	}
	else
	{
		USkeletalMesh* Mesh = FMonolithAssetUtils::LoadAssetByPath<USkeletalMesh>(AssetPath);
		if (!Mesh)
			return FMonolithActionResult::Error(FString::Printf(TEXT("Skeleton or SkeletalMesh not found: %s"), *AssetPath));

		for (int32 i = 0; i < Mesh->NumSockets(); ++i)
		{
			if (USkeletalMeshSocket* Sock = Mesh->GetSocketByIndex(i))
				SocketList.Add(Sock);
		}
		SourceType = TEXT("SkeletalMesh");
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetStringField(TEXT("source_type"), SourceType);

	TArray<TSharedPtr<FJsonValue>> SocketsArr;
	for (USkeletalMeshSocket* Sock : SocketList)
	{
		if (!Sock) continue;
		TSharedPtr<FJsonObject> SockObj = MakeShared<FJsonObject>();
		SockObj->SetStringField(TEXT("name"), Sock->SocketName.ToString());
		SockObj->SetStringField(TEXT("bone"), Sock->BoneName.ToString());

		TArray<TSharedPtr<FJsonValue>> LocArr;
		LocArr.Add(MakeShared<FJsonValueNumber>(Sock->RelativeLocation.X));
		LocArr.Add(MakeShared<FJsonValueNumber>(Sock->RelativeLocation.Y));
		LocArr.Add(MakeShared<FJsonValueNumber>(Sock->RelativeLocation.Z));
		SockObj->SetArrayField(TEXT("location"), LocArr);

		TArray<TSharedPtr<FJsonValue>> RotArr;
		RotArr.Add(MakeShared<FJsonValueNumber>(Sock->RelativeRotation.Pitch));
		RotArr.Add(MakeShared<FJsonValueNumber>(Sock->RelativeRotation.Yaw));
		RotArr.Add(MakeShared<FJsonValueNumber>(Sock->RelativeRotation.Roll));
		SockObj->SetArrayField(TEXT("rotation"), RotArr);

		TArray<TSharedPtr<FJsonValue>> ScaleArr;
		ScaleArr.Add(MakeShared<FJsonValueNumber>(Sock->RelativeScale.X));
		ScaleArr.Add(MakeShared<FJsonValueNumber>(Sock->RelativeScale.Y));
		ScaleArr.Add(MakeShared<FJsonValueNumber>(Sock->RelativeScale.Z));
		SockObj->SetArrayField(TEXT("scale"), ScaleArr);

		SocketsArr.Add(MakeShared<FJsonValueObject>(SockObj));
	}

	Root->SetArrayField(TEXT("sockets"), SocketsArr);
	Root->SetNumberField(TEXT("count"), SocketsArr.Num());
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleGetAbpInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UAnimBlueprint* ABP = FMonolithAssetUtils::LoadAssetByPath<UAnimBlueprint>(AssetPath);
	if (!ABP) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimBlueprint not found: %s"), *AssetPath));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);

	USkeleton* Skel = ABP->TargetSkeleton;
	Root->SetStringField(TEXT("skeleton"), Skel ? Skel->GetPathName() : TEXT("None"));
	Root->SetStringField(TEXT("target_skeleton"), Skel ? Skel->GetPathName() : TEXT("None"));
	Root->SetStringField(TEXT("parent_class"),
		ABP->ParentClass ? ABP->ParentClass->GetName() : TEXT("None"));

	// Count state machines
	int32 StateMachineCount = 0;
	TArray<TSharedPtr<FJsonValue>> GraphNames;
	for (UEdGraph* Graph : ABP->FunctionGraphs)
	{
		if (!Graph) continue;
		GraphNames.Add(MakeShared<FJsonValueString>(Graph->GetName()));
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Cast<UAnimGraphNode_StateMachine>(Node))
				++StateMachineCount;
		}
	}
	for (UEdGraph* Graph : ABP->UbergraphPages)
	{
		if (!Graph) continue;
		GraphNames.Add(MakeShared<FJsonValueString>(Graph->GetName()));
	}

	Root->SetNumberField(TEXT("state_machine_count"), StateMachineCount);
	Root->SetNumberField(TEXT("graph_count"), GraphNames.Num());
	Root->SetArrayField(TEXT("graphs"), GraphNames);
	Root->SetNumberField(TEXT("variable_count"), ABP->NewVariables.Num());

	// Interfaces
	TArray<TSharedPtr<FJsonValue>> InterfacesArr;
	for (const FBPInterfaceDescription& Iface : ABP->ImplementedInterfaces)
	{
		if (Iface.Interface)
			InterfacesArr.Add(MakeShared<FJsonValueString>(Iface.Interface->GetName()));
	}
	Root->SetArrayField(TEXT("interfaces"), InterfacesArr);

	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// Wave 2 — Notify CRUD
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithAnimationActions::HandleAddNotify(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString NotifyClassName = Params->GetStringField(TEXT("notify_class"));
	float Time = static_cast<float>(Params->GetNumberField(TEXT("time")));
	FString TrackName = TEXT("1");
	Params->TryGetStringField(TEXT("track_name"), TrackName);

	UAnimSequenceBase* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequenceBase>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("Animation asset not found: %s"), *AssetPath));

	if (Time < 0.f || Time > Seq->GetPlayLength())
		return FMonolithActionResult::Error(FString::Printf(TEXT("Time %.3f out of range [0, %.3f]"), Time, Seq->GetPlayLength()));

	UClass* NotifyClass = FindFirstObject<UClass>(*NotifyClassName, EFindFirstObjectOptions::NativeFirst);
	if (!NotifyClass)
		NotifyClass = FindFirstObject<UClass>(*FString::Printf(TEXT("AnimNotify_%s"), *NotifyClassName), EFindFirstObjectOptions::NativeFirst);
	if (!NotifyClass || !NotifyClass->IsChildOf(UAnimNotify::StaticClass()))
		return FMonolithActionResult::Error(FString::Printf(TEXT("Notify class not found or not a UAnimNotify subclass: %s"), *NotifyClassName));

	UAnimNotify* NewNotify = NewObject<UAnimNotify>(Seq, NotifyClass);
	if (!NewNotify)
		return FMonolithActionResult::Error(TEXT("Failed to create notify instance"));

	GEditor->BeginTransaction(FText::FromString(TEXT("Add Notify")));
	Seq->Modify();

	UAnimationBlueprintLibrary::AddAnimationNotifyEventObject(Seq, Time, NewNotify, FName(*TrackName));
	Seq->RefreshCacheData();

	GEditor->EndTransaction();
	Seq->MarkPackageDirty();

	int32 NewIndex = Seq->Notifies.Num() - 1;

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("index"), NewIndex);
	Root->SetStringField(TEXT("notify_class"), NotifyClass->GetName());
	Root->SetNumberField(TEXT("time"), Time);
	Root->SetStringField(TEXT("track_name"), TrackName);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleAddNotifyState(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString NotifyClassName = Params->GetStringField(TEXT("notify_class"));
	float Time = static_cast<float>(Params->GetNumberField(TEXT("time")));
	float Duration = static_cast<float>(Params->GetNumberField(TEXT("duration")));
	FString TrackName = TEXT("1");
	Params->TryGetStringField(TEXT("track_name"), TrackName);

	UAnimSequenceBase* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequenceBase>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("Animation asset not found: %s"), *AssetPath));

	if (Time < 0.f || Time > Seq->GetPlayLength())
		return FMonolithActionResult::Error(FString::Printf(TEXT("Time %.3f out of range [0, %.3f]"), Time, Seq->GetPlayLength()));
	if (Duration <= 0.f)
		return FMonolithActionResult::Error(TEXT("Duration must be > 0"));
	if (Time + Duration > Seq->GetPlayLength())
		return FMonolithActionResult::Error(FString::Printf(TEXT("Time + Duration (%.3f) exceeds play length (%.3f)"), Time + Duration, Seq->GetPlayLength()));

	UClass* NotifyClass = FindFirstObject<UClass>(*NotifyClassName, EFindFirstObjectOptions::NativeFirst);
	if (!NotifyClass)
		NotifyClass = FindFirstObject<UClass>(*FString::Printf(TEXT("AnimNotifyState_%s"), *NotifyClassName), EFindFirstObjectOptions::NativeFirst);
	if (!NotifyClass || !NotifyClass->IsChildOf(UAnimNotifyState::StaticClass()))
		return FMonolithActionResult::Error(FString::Printf(TEXT("NotifyState class not found or not a UAnimNotifyState subclass: %s"), *NotifyClassName));

	UAnimNotifyState* NewNotifyState = NewObject<UAnimNotifyState>(Seq, NotifyClass);
	if (!NewNotifyState)
		return FMonolithActionResult::Error(TEXT("Failed to create notify state instance"));

	GEditor->BeginTransaction(FText::FromString(TEXT("Add Notify State")));
	Seq->Modify();

	UAnimationBlueprintLibrary::AddAnimationNotifyStateEventObject(Seq, Time, Duration, NewNotifyState, FName(*TrackName));
	Seq->RefreshCacheData();

	GEditor->EndTransaction();
	Seq->MarkPackageDirty();

	int32 NewIndex = Seq->Notifies.Num() - 1;

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("index"), NewIndex);
	Root->SetStringField(TEXT("notify_class"), NotifyClass->GetName());
	Root->SetNumberField(TEXT("time"), Time);
	Root->SetNumberField(TEXT("duration"), Duration);
	Root->SetStringField(TEXT("track_name"), TrackName);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleRemoveNotify(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	int32 NotifyIndex = static_cast<int32>(Params->GetNumberField(TEXT("notify_index")));

	UAnimSequenceBase* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequenceBase>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("Animation asset not found: %s"), *AssetPath));

	if (!Seq->Notifies.IsValidIndex(NotifyIndex))
		return FMonolithActionResult::Error(FString::Printf(TEXT("Invalid notify index: %d (total: %d)"), NotifyIndex, Seq->Notifies.Num()));

	FName RemovedName = Seq->Notifies[NotifyIndex].NotifyName;

	GEditor->BeginTransaction(FText::FromString(TEXT("Remove Notify")));
	Seq->Modify();

	Seq->Notifies.RemoveAt(NotifyIndex);
	Seq->RefreshCacheData();

	GEditor->EndTransaction();
	Seq->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("removed_index"), NotifyIndex);
	Root->SetStringField(TEXT("removed_name"), RemovedName.ToString());
	Root->SetNumberField(TEXT("remaining_count"), Seq->Notifies.Num());
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleSetNotifyTrack(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	int32 NotifyIndex = static_cast<int32>(Params->GetNumberField(TEXT("notify_index")));
	int32 TrackIndex = static_cast<int32>(Params->GetNumberField(TEXT("track_index")));

	UAnimSequenceBase* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequenceBase>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("Animation asset not found: %s"), *AssetPath));

	if (!Seq->Notifies.IsValidIndex(NotifyIndex))
		return FMonolithActionResult::Error(FString::Printf(TEXT("Invalid notify index: %d (total: %d)"), NotifyIndex, Seq->Notifies.Num()));

	if (!Seq->AnimNotifyTracks.IsValidIndex(TrackIndex))
		return FMonolithActionResult::Error(FString::Printf(TEXT("Invalid track index: %d (total: %d)"), TrackIndex, Seq->AnimNotifyTracks.Num()));

	GEditor->BeginTransaction(FText::FromString(TEXT("Set Notify Track")));
	Seq->Modify();

	Seq->Notifies[NotifyIndex].TrackIndex = TrackIndex;
	Seq->RefreshCacheData();

	GEditor->EndTransaction();
	Seq->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("notify_index"), NotifyIndex);
	Root->SetStringField(TEXT("notify_name"), Seq->Notifies[NotifyIndex].NotifyName.ToString());
	Root->SetNumberField(TEXT("new_track_index"), TrackIndex);
	Root->SetStringField(TEXT("track_name"), Seq->AnimNotifyTracks[TrackIndex].TrackName.ToString());
	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// Wave 3 — Curve CRUD (5 actions)
// ---------------------------------------------------------------------------

static FString InterpModeToString(ERichCurveInterpMode Mode)
{
	switch (Mode)
	{
	case RCIM_Constant: return TEXT("constant");
	case RCIM_Linear:   return TEXT("linear");
	case RCIM_Cubic:    return TEXT("cubic");
	default:            return TEXT("unknown");
	}
}

static ERichCurveInterpMode StringToInterpMode(const FString& Str)
{
	if (Str.Equals(TEXT("constant"), ESearchCase::IgnoreCase)) return RCIM_Constant;
	if (Str.Equals(TEXT("linear"), ESearchCase::IgnoreCase))   return RCIM_Linear;
	return RCIM_Cubic; // default
}

FMonolithActionResult FMonolithAnimationActions::HandleListCurves(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	bool bIncludeKeys = Params->HasField(TEXT("include_keys")) && Params->GetBoolField(TEXT("include_keys"));

	UAnimSequence* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimSequence not found: %s"), *AssetPath));

	const IAnimationDataModel* DataModel = Seq->GetDataModel();
	if (!DataModel) return FMonolithActionResult::Error(TEXT("No animation data model"));

	TArray<TSharedPtr<FJsonValue>> CurvesArray;

	// Float curves
	for (const FFloatCurve& Curve : DataModel->GetFloatCurves())
	{
		TSharedPtr<FJsonObject> CurveObj = MakeShared<FJsonObject>();
		CurveObj->SetStringField(TEXT("name"), Curve.GetName().ToString());
		CurveObj->SetStringField(TEXT("type"), TEXT("Float"));
		CurveObj->SetNumberField(TEXT("num_keys"), Curve.FloatCurve.GetNumKeys());

		if (Curve.FloatCurve.GetNumKeys() > 0)
		{
			const TArray<FRichCurveKey>& Keys = Curve.FloatCurve.GetConstRefOfKeys();
			float MinVal = Keys[0].Value, MaxVal = Keys[0].Value;
			for (const FRichCurveKey& Key : Keys)
			{
				MinVal = FMath::Min(MinVal, Key.Value);
				MaxVal = FMath::Max(MaxVal, Key.Value);
			}
			CurveObj->SetNumberField(TEXT("min_value"), MinVal);
			CurveObj->SetNumberField(TEXT("max_value"), MaxVal);

			if (bIncludeKeys)
			{
				TArray<TSharedPtr<FJsonValue>> KeysArray;
				for (const FRichCurveKey& Key : Keys)
				{
					TSharedPtr<FJsonObject> KeyObj = MakeShared<FJsonObject>();
					KeyObj->SetNumberField(TEXT("time"), Key.Time);
					KeyObj->SetNumberField(TEXT("value"), Key.Value);
					KeyObj->SetStringField(TEXT("interp_mode"), InterpModeToString(Key.InterpMode));
					KeysArray.Add(MakeShared<FJsonValueObject>(KeyObj));
				}
				CurveObj->SetArrayField(TEXT("keys"), KeysArray);
			}
		}

		CurvesArray.Add(MakeShared<FJsonValueObject>(CurveObj));
	}

	// Transform curves
	for (const FTransformCurve& Curve : DataModel->GetTransformCurves())
	{
		TSharedPtr<FJsonObject> CurveObj = MakeShared<FJsonObject>();
		CurveObj->SetStringField(TEXT("name"), Curve.GetName().ToString());
		CurveObj->SetStringField(TEXT("type"), TEXT("Transform"));
		CurveObj->SetNumberField(TEXT("num_keys"), 0); // Transform curves have sub-curves
		CurvesArray.Add(MakeShared<FJsonValueObject>(CurveObj));
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetNumberField(TEXT("count"), CurvesArray.Num());
	Root->SetArrayField(TEXT("curves"), CurvesArray);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleAddCurve(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString CurveName = Params->GetStringField(TEXT("curve_name"));
	FString CurveTypeStr = Params->HasField(TEXT("curve_type")) ? Params->GetStringField(TEXT("curve_type")) : TEXT("Float");

	UAnimSequence* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimSequence not found: %s"), *AssetPath));

	ERawCurveTrackTypes CurveType = CurveTypeStr.Equals(TEXT("Transform"), ESearchCase::IgnoreCase)
		? ERawCurveTrackTypes::RCT_Transform
		: ERawCurveTrackTypes::RCT_Float;

	const FAnimationCurveIdentifier CurveId(FName(*CurveName), CurveType);

	// Check if curve already exists
	if (Seq->GetDataModel()->FindCurve(CurveId))
		return FMonolithActionResult::Error(FString::Printf(TEXT("Curve '%s' already exists"), *CurveName));

	IAnimationDataController& Controller = Seq->GetController();
	Controller.OpenBracket(FText::FromString(TEXT("Add Curve")));
	bool bSuccess = Controller.AddCurve(CurveId);
	Controller.CloseBracket();

	if (!bSuccess)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to add curve '%s'"), *CurveName));

	Seq->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("curve_name"), CurveName);
	Root->SetStringField(TEXT("curve_type"), CurveTypeStr);
	Root->SetBoolField(TEXT("success"), true);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleRemoveCurve(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString CurveName = Params->GetStringField(TEXT("curve_name"));
	FString CurveTypeStr = Params->HasField(TEXT("curve_type")) ? Params->GetStringField(TEXT("curve_type")) : TEXT("Float");

	UAnimSequence* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimSequence not found: %s"), *AssetPath));

	ERawCurveTrackTypes CurveType = CurveTypeStr.Equals(TEXT("Transform"), ESearchCase::IgnoreCase)
		? ERawCurveTrackTypes::RCT_Transform
		: ERawCurveTrackTypes::RCT_Float;

	const FAnimationCurveIdentifier CurveId(FName(*CurveName), CurveType);

	if (!Seq->GetDataModel()->FindCurve(CurveId))
		return FMonolithActionResult::Error(FString::Printf(TEXT("Curve '%s' not found"), *CurveName));

	IAnimationDataController& Controller = Seq->GetController();
	Controller.OpenBracket(FText::FromString(TEXT("Remove Curve")));
	bool bSuccess = Controller.RemoveCurve(CurveId);
	Controller.CloseBracket();

	if (!bSuccess)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to remove curve '%s'"), *CurveName));

	Seq->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("curve_name"), CurveName);
	Root->SetBoolField(TEXT("removed"), true);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleSetCurveKeys(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString CurveName = Params->GetStringField(TEXT("curve_name"));
	FString KeysJson = Params->GetStringField(TEXT("keys_json"));

	UAnimSequence* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimSequence not found: %s"), *AssetPath));

	const FAnimationCurveIdentifier CurveId(FName(*CurveName), ERawCurveTrackTypes::RCT_Float);

	if (!Seq->GetDataModel()->FindCurve(CurveId))
		return FMonolithActionResult::Error(FString::Printf(TEXT("Curve '%s' not found — add it first"), *CurveName));

	// Parse keys JSON
	TArray<TSharedPtr<FJsonValue>> JsonKeys;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(KeysJson);
	if (!FJsonSerializer::Deserialize(Reader, JsonKeys))
		return FMonolithActionResult::Error(TEXT("Failed to parse keys_json — expected JSON array"));

	TArray<FRichCurveKey> Keys;
	for (const TSharedPtr<FJsonValue>& KeyVal : JsonKeys)
	{
		const TSharedPtr<FJsonObject>* KeyObjPtr;
		if (!KeyVal->TryGetObject(KeyObjPtr)) continue;
		const TSharedPtr<FJsonObject>& KeyObj = *KeyObjPtr;

		FRichCurveKey Key;
		Key.Time = static_cast<float>(KeyObj->GetNumberField(TEXT("time")));
		Key.Value = static_cast<float>(KeyObj->GetNumberField(TEXT("value")));
		if (KeyObj->HasField(TEXT("interp")))
			Key.InterpMode = StringToInterpMode(KeyObj->GetStringField(TEXT("interp")));
		else
			Key.InterpMode = RCIM_Cubic;
		Keys.Add(Key);
	}

	IAnimationDataController& Controller = Seq->GetController();
	Controller.OpenBracket(FText::FromString(TEXT("Set Curve Keys")));
	bool bSuccess = Controller.SetCurveKeys(CurveId, Keys);
	Controller.CloseBracket();

	if (!bSuccess)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to set keys on curve '%s'"), *CurveName));

	Seq->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("curve_name"), CurveName);
	Root->SetNumberField(TEXT("num_keys"), Keys.Num());
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleGetCurveKeys(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString CurveName = Params->GetStringField(TEXT("curve_name"));

	UAnimSequence* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimSequence not found: %s"), *AssetPath));

	const FAnimationCurveIdentifier CurveId(FName(*CurveName), ERawCurveTrackTypes::RCT_Float);
	const FAnimCurveBase* CurveBase = Seq->GetDataModel()->FindCurve(CurveId);
	if (!CurveBase)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Curve '%s' not found"), *CurveName));

	const FFloatCurve* FloatCurve = static_cast<const FFloatCurve*>(CurveBase);
	const TArray<FRichCurveKey>& Keys = FloatCurve->FloatCurve.GetConstRefOfKeys();

	TArray<TSharedPtr<FJsonValue>> KeysArray;
	for (const FRichCurveKey& Key : Keys)
	{
		TSharedPtr<FJsonObject> KeyObj = MakeShared<FJsonObject>();
		KeyObj->SetNumberField(TEXT("time"), Key.Time);
		KeyObj->SetNumberField(TEXT("value"), Key.Value);
		KeyObj->SetStringField(TEXT("interp_mode"), InterpModeToString(Key.InterpMode));
		KeysArray.Add(MakeShared<FJsonValueObject>(KeyObj));
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("curve_name"), CurveName);
	Root->SetNumberField(TEXT("num_keys"), Keys.Num());
	Root->SetArrayField(TEXT("keys"), KeysArray);
	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// Wave 4 — Skeleton + BlendSpace (6 actions)
// ---------------------------------------------------------------------------

static FVector ParseVectorFromJsonArray(const TArray<TSharedPtr<FJsonValue>>& Arr, const FVector& Default)
{
	if (Arr.Num() < 3) return Default;
	return FVector(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
}

static FRotator ParseRotatorFromJsonArray(const TArray<TSharedPtr<FJsonValue>>& Arr, const FRotator& Default)
{
	if (Arr.Num() < 3) return Default;
	return FRotator(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
}

FMonolithActionResult FMonolithAnimationActions::HandleAddSocket(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString BoneName = Params->GetStringField(TEXT("bone_name"));
	FString SocketName = Params->GetStringField(TEXT("socket_name"));

	USkeleton* Skeleton = FMonolithAssetUtils::LoadAssetByPath<USkeleton>(AssetPath);
	if (!Skeleton) return FMonolithActionResult::Error(FString::Printf(TEXT("Skeleton not found: %s"), *AssetPath));

	// Validate bone exists
	if (Skeleton->GetReferenceSkeleton().FindBoneIndex(FName(*BoneName)) == INDEX_NONE)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Bone '%s' not found in skeleton"), *BoneName));

	// Check socket doesn't already exist
	if (Skeleton->FindSocket(FName(*SocketName)))
		return FMonolithActionResult::Error(FString::Printf(TEXT("Socket '%s' already exists"), *SocketName));

	USkeletalMeshSocket* Socket = NewObject<USkeletalMeshSocket>(Skeleton);
	Socket->SocketName = FName(*SocketName);
	Socket->BoneName = FName(*BoneName);

	// Parse optional transform
	const TArray<TSharedPtr<FJsonValue>>* LocationArr;
	if (Params->TryGetArrayField(TEXT("location"), LocationArr))
		Socket->RelativeLocation = ParseVectorFromJsonArray(*LocationArr, FVector::ZeroVector);

	const TArray<TSharedPtr<FJsonValue>>* RotationArr;
	if (Params->TryGetArrayField(TEXT("rotation"), RotationArr))
		Socket->RelativeRotation = ParseRotatorFromJsonArray(*RotationArr, FRotator::ZeroRotator);

	const TArray<TSharedPtr<FJsonValue>>* ScaleArr;
	if (Params->TryGetArrayField(TEXT("scale"), ScaleArr))
		Socket->RelativeScale = ParseVectorFromJsonArray(*ScaleArr, FVector::OneVector);
	else
		Socket->RelativeScale = FVector::OneVector;

	Skeleton->Sockets.Add(Socket);
	Skeleton->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("socket_name"), SocketName);
	Root->SetStringField(TEXT("bone_name"), BoneName);
	TArray<TSharedPtr<FJsonValue>> LocArr;
	LocArr.Add(MakeShared<FJsonValueNumber>(Socket->RelativeLocation.X));
	LocArr.Add(MakeShared<FJsonValueNumber>(Socket->RelativeLocation.Y));
	LocArr.Add(MakeShared<FJsonValueNumber>(Socket->RelativeLocation.Z));
	Root->SetArrayField(TEXT("location"), LocArr);
	TArray<TSharedPtr<FJsonValue>> RotArr;
	RotArr.Add(MakeShared<FJsonValueNumber>(Socket->RelativeRotation.Pitch));
	RotArr.Add(MakeShared<FJsonValueNumber>(Socket->RelativeRotation.Yaw));
	RotArr.Add(MakeShared<FJsonValueNumber>(Socket->RelativeRotation.Roll));
	Root->SetArrayField(TEXT("rotation"), RotArr);
	TArray<TSharedPtr<FJsonValue>> SclArr;
	SclArr.Add(MakeShared<FJsonValueNumber>(Socket->RelativeScale.X));
	SclArr.Add(MakeShared<FJsonValueNumber>(Socket->RelativeScale.Y));
	SclArr.Add(MakeShared<FJsonValueNumber>(Socket->RelativeScale.Z));
	Root->SetArrayField(TEXT("scale"), SclArr);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleRemoveSocket(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString SocketName = Params->GetStringField(TEXT("socket_name"));

	USkeleton* Skeleton = FMonolithAssetUtils::LoadAssetByPath<USkeleton>(AssetPath);
	if (!Skeleton) return FMonolithActionResult::Error(FString::Printf(TEXT("Skeleton not found: %s"), *AssetPath));

	USkeletalMeshSocket* FoundSocket = nullptr;
	for (USkeletalMeshSocket* Socket : Skeleton->Sockets)
	{
		if (Socket && Socket->SocketName == FName(*SocketName))
		{
			FoundSocket = Socket;
			break;
		}
	}

	if (!FoundSocket)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Socket '%s' not found"), *SocketName));

	Skeleton->Sockets.Remove(FoundSocket);
	Skeleton->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("removed_socket"), SocketName);
	Root->SetBoolField(TEXT("success"), true);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleSetSocketTransform(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString SocketName = Params->GetStringField(TEXT("socket_name"));

	USkeleton* Skeleton = FMonolithAssetUtils::LoadAssetByPath<USkeleton>(AssetPath);
	if (!Skeleton) return FMonolithActionResult::Error(FString::Printf(TEXT("Skeleton not found: %s"), *AssetPath));

	USkeletalMeshSocket* FoundSocket = nullptr;
	for (USkeletalMeshSocket* Socket : Skeleton->Sockets)
	{
		if (Socket && Socket->SocketName == FName(*SocketName))
		{
			FoundSocket = Socket;
			break;
		}
	}

	if (!FoundSocket)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Socket '%s' not found"), *SocketName));

	bool bAnySet = false;

	const TArray<TSharedPtr<FJsonValue>>* LocationArr;
	if (Params->TryGetArrayField(TEXT("location"), LocationArr))
	{
		FoundSocket->RelativeLocation = ParseVectorFromJsonArray(*LocationArr, FoundSocket->RelativeLocation);
		bAnySet = true;
	}

	const TArray<TSharedPtr<FJsonValue>>* RotationArr;
	if (Params->TryGetArrayField(TEXT("rotation"), RotationArr))
	{
		FoundSocket->RelativeRotation = ParseRotatorFromJsonArray(*RotationArr, FoundSocket->RelativeRotation);
		bAnySet = true;
	}

	const TArray<TSharedPtr<FJsonValue>>* ScaleArr;
	if (Params->TryGetArrayField(TEXT("scale"), ScaleArr))
	{
		FoundSocket->RelativeScale = ParseVectorFromJsonArray(*ScaleArr, FoundSocket->RelativeScale);
		bAnySet = true;
	}

	if (!bAnySet)
		return FMonolithActionResult::Error(TEXT("At least one of location, rotation, or scale must be provided"));

	Skeleton->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("socket_name"), SocketName);
	TArray<TSharedPtr<FJsonValue>> LocArr;
	LocArr.Add(MakeShared<FJsonValueNumber>(FoundSocket->RelativeLocation.X));
	LocArr.Add(MakeShared<FJsonValueNumber>(FoundSocket->RelativeLocation.Y));
	LocArr.Add(MakeShared<FJsonValueNumber>(FoundSocket->RelativeLocation.Z));
	Root->SetArrayField(TEXT("location"), LocArr);
	TArray<TSharedPtr<FJsonValue>> RotArr;
	RotArr.Add(MakeShared<FJsonValueNumber>(FoundSocket->RelativeRotation.Pitch));
	RotArr.Add(MakeShared<FJsonValueNumber>(FoundSocket->RelativeRotation.Yaw));
	RotArr.Add(MakeShared<FJsonValueNumber>(FoundSocket->RelativeRotation.Roll));
	Root->SetArrayField(TEXT("rotation"), RotArr);
	TArray<TSharedPtr<FJsonValue>> SclArr;
	SclArr.Add(MakeShared<FJsonValueNumber>(FoundSocket->RelativeScale.X));
	SclArr.Add(MakeShared<FJsonValueNumber>(FoundSocket->RelativeScale.Y));
	SclArr.Add(MakeShared<FJsonValueNumber>(FoundSocket->RelativeScale.Z));
	Root->SetArrayField(TEXT("scale"), SclArr);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleGetSkeletonCurves(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	USkeleton* Skeleton = FMonolithAssetUtils::LoadAssetByPath<USkeleton>(AssetPath);
	if (!Skeleton) return FMonolithActionResult::Error(FString::Printf(TEXT("Skeleton not found: %s"), *AssetPath));

	TArray<TSharedPtr<FJsonValue>> CurvesArray;
	Skeleton->ForEachCurveMetaData([&CurvesArray](FName CurveName, const FCurveMetaData& MetaData)
	{
		TSharedPtr<FJsonObject> CurveObj = MakeShared<FJsonObject>();
		CurveObj->SetStringField(TEXT("name"), CurveName.ToString());
		CurvesArray.Add(MakeShared<FJsonValueObject>(CurveObj));
	});

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetNumberField(TEXT("count"), CurvesArray.Num());
	Root->SetArrayField(TEXT("curves"), CurvesArray);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleSetBlendSpaceAxis(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString AxisStr = Params->GetStringField(TEXT("axis"));

	UBlendSpace* BS = FMonolithAssetUtils::LoadAssetByPath<UBlendSpace>(AssetPath);
	if (!BS) return FMonolithActionResult::Error(FString::Printf(TEXT("BlendSpace not found: %s"), *AssetPath));

	int32 AxisIndex;
	if (AxisStr.Equals(TEXT("X"), ESearchCase::IgnoreCase))
		AxisIndex = 0;
	else if (AxisStr.Equals(TEXT("Y"), ESearchCase::IgnoreCase))
		AxisIndex = 1;
	else
		return FMonolithActionResult::Error(FString::Printf(TEXT("Invalid axis '%s' — must be X or Y"), *AxisStr));

	// Access BlendParameters via UProperty reflection since it's protected
	FBlendParameter* BlendParam = nullptr;
	{
		FProperty* Prop = UBlendSpace::StaticClass()->FindPropertyByName(TEXT("BlendParameters"));
		if (Prop)
		{
			// BlendParameters is a C array of FBlendParameter[3], offset points to first element
			BlendParam = reinterpret_cast<FBlendParameter*>(Prop->ContainerPtrToValuePtr<uint8>(BS));
			BlendParam += AxisIndex;
		}
	}

	if (!BlendParam)
		return FMonolithActionResult::Error(TEXT("Failed to access BlendParameters via reflection"));

	GEditor->BeginTransaction(FText::FromString(TEXT("Set Blend Space Axis")));
	BS->Modify();

	if (Params->HasField(TEXT("name")))
		BlendParam->DisplayName = Params->GetStringField(TEXT("name"));
	if (Params->HasField(TEXT("min")))
		BlendParam->Min = static_cast<float>(Params->GetNumberField(TEXT("min")));
	if (Params->HasField(TEXT("max")))
		BlendParam->Max = static_cast<float>(Params->GetNumberField(TEXT("max")));
	if (Params->HasField(TEXT("grid_divisions")))
		BlendParam->GridNum = static_cast<int32>(Params->GetNumberField(TEXT("grid_divisions")));
	if (Params->HasField(TEXT("snap_to_grid")))
		BlendParam->bSnapToGrid = Params->GetBoolField(TEXT("snap_to_grid"));
	if (Params->HasField(TEXT("wrap_input")))
		BlendParam->bWrapInput = Params->GetBoolField(TEXT("wrap_input"));

	// Validate min < max
	if (BlendParam->Min >= BlendParam->Max)
	{
		GEditor->EndTransaction();
		return FMonolithActionResult::Error(FString::Printf(TEXT("min (%.2f) must be less than max (%.2f)"), BlendParam->Min, BlendParam->Max));
	}

	BS->ValidateSampleData();

	GEditor->EndTransaction();
	BS->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("axis"), AxisStr.ToUpper());
	Root->SetStringField(TEXT("name"), BlendParam->DisplayName);
	Root->SetNumberField(TEXT("min"), BlendParam->Min);
	Root->SetNumberField(TEXT("max"), BlendParam->Max);
	Root->SetNumberField(TEXT("grid_divisions"), BlendParam->GridNum);
	Root->SetBoolField(TEXT("snap_to_grid"), BlendParam->bSnapToGrid);
	Root->SetBoolField(TEXT("wrap_input"), BlendParam->bWrapInput);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleSetRootMotionSettings(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UAnimSequence* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimSequence not found: %s"), *AssetPath));

	bool bAnySet = false;

	GEditor->BeginTransaction(FText::FromString(TEXT("Set Root Motion Settings")));
	Seq->Modify();

	if (Params->HasField(TEXT("enable_root_motion")))
	{
		Seq->bEnableRootMotion = Params->GetBoolField(TEXT("enable_root_motion"));
		bAnySet = true;
	}

	if (Params->HasField(TEXT("root_motion_lock")))
	{
		FString LockStr = Params->GetStringField(TEXT("root_motion_lock"));
		if (LockStr.Equals(TEXT("AnimFirstFrame"), ESearchCase::IgnoreCase))
			Seq->RootMotionRootLock = ERootMotionRootLock::AnimFirstFrame;
		else if (LockStr.Equals(TEXT("Zero"), ESearchCase::IgnoreCase))
			Seq->RootMotionRootLock = ERootMotionRootLock::Zero;
		else if (LockStr.Equals(TEXT("RefPose"), ESearchCase::IgnoreCase))
			Seq->RootMotionRootLock = ERootMotionRootLock::RefPose;
		else
		{
			GEditor->EndTransaction();
			return FMonolithActionResult::Error(FString::Printf(TEXT("Invalid root_motion_lock: '%s' — use AnimFirstFrame, Zero, or RefPose"), *LockStr));
		}
		bAnySet = true;
	}

	if (Params->HasField(TEXT("force_root_lock")))
	{
		Seq->bForceRootLock = Params->GetBoolField(TEXT("force_root_lock"));
		bAnySet = true;
	}

	if (!bAnySet)
	{
		GEditor->EndTransaction();
		return FMonolithActionResult::Error(TEXT("At least one of enable_root_motion, root_motion_lock, or force_root_lock must be provided"));
	}

	GEditor->EndTransaction();
	Seq->MarkPackageDirty();

	// Map enum back to string
	FString LockName;
	switch (Seq->RootMotionRootLock)
	{
	case ERootMotionRootLock::AnimFirstFrame: LockName = TEXT("AnimFirstFrame"); break;
	case ERootMotionRootLock::Zero:           LockName = TEXT("Zero"); break;
	case ERootMotionRootLock::RefPose:        LockName = TEXT("RefPose"); break;
	default:                                  LockName = TEXT("Unknown"); break;
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetBoolField(TEXT("enable_root_motion"), Seq->bEnableRootMotion);
	Root->SetStringField(TEXT("root_motion_lock"), LockName);
	Root->SetBoolField(TEXT("force_root_lock"), Seq->bForceRootLock);
	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// Wave 5 — Creation + Montage
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithAnimationActions::HandleCreateSequence(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString SkeletonPath = Params->GetStringField(TEXT("skeleton_path"));

	USkeleton* Skeleton = FMonolithAssetUtils::LoadAssetByPath<USkeleton>(SkeletonPath);
	if (!Skeleton) return FMonolithActionResult::Error(FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));

	// Extract asset name from path
	FString AssetName;
	int32 LastSlash;
	if (!AssetPath.FindLastChar('/', LastSlash) || LastSlash == AssetPath.Len() - 1)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Invalid asset path: %s"), *AssetPath));
	}
	AssetName = AssetPath.Mid(LastSlash + 1);

	// Check if asset already exists
	if (FMonolithAssetUtils::LoadAssetByPath<UObject>(AssetPath))
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Asset already exists at '%s'"), *AssetPath));
	}

	UPackage* Pkg = CreatePackage(*AssetPath);
	if (!Pkg)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to create package at '%s'"), *AssetPath));
	}

	UAnimSequence* Seq = NewObject<UAnimSequence>(Pkg, FName(*AssetName), RF_Public | RF_Standalone);
	if (!Seq)
	{
		return FMonolithActionResult::Error(TEXT("Failed to create UAnimSequence object"));
	}

	Seq->SetSkeleton(Skeleton);
	FAssetRegistryModule::AssetCreated(Seq);
	Pkg->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), Seq->GetPathName());
	Root->SetStringField(TEXT("asset_name"), AssetName);
	Root->SetStringField(TEXT("skeleton"), Skeleton->GetPathName());
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleDuplicateSequence(const TSharedPtr<FJsonObject>& Params)
{
	FString SourcePath = Params->GetStringField(TEXT("source_path"));
	FString DestPath = Params->GetStringField(TEXT("dest_path"));

	// Check source exists
	UObject* SourceObj = FMonolithAssetUtils::LoadAssetByPath<UObject>(SourcePath);
	if (!SourceObj)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Source asset not found: %s"), *SourcePath));
	}

	// Check dest doesn't exist
	if (FMonolithAssetUtils::LoadAssetByPath<UObject>(DestPath))
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Asset already exists at '%s'"), *DestPath));
	}

	UObject* DuplicatedObj = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestPath);
	if (!DuplicatedObj)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to duplicate '%s' to '%s'"), *SourcePath, *DestPath));
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("source_path"), SourcePath);
	Root->SetStringField(TEXT("dest_path"), DuplicatedObj->GetPathName());
	Root->SetStringField(TEXT("asset_class"), DuplicatedObj->GetClass()->GetName());
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleCreateMontage(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString SkeletonPath = Params->GetStringField(TEXT("skeleton_path"));

	USkeleton* Skeleton = FMonolithAssetUtils::LoadAssetByPath<USkeleton>(SkeletonPath);
	if (!Skeleton) return FMonolithActionResult::Error(FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));

	// Extract asset name from path
	FString AssetName;
	int32 LastSlash;
	if (!AssetPath.FindLastChar('/', LastSlash) || LastSlash == AssetPath.Len() - 1)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Invalid asset path: %s"), *AssetPath));
	}
	AssetName = AssetPath.Mid(LastSlash + 1);

	// Check if asset already exists
	if (FMonolithAssetUtils::LoadAssetByPath<UObject>(AssetPath))
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Asset already exists at '%s'"), *AssetPath));
	}

	UPackage* Pkg = CreatePackage(*AssetPath);
	if (!Pkg)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to create package at '%s'"), *AssetPath));
	}

	UAnimMontage* Montage = NewObject<UAnimMontage>(Pkg, FName(*AssetName), RF_Public | RF_Standalone);
	if (!Montage)
	{
		return FMonolithActionResult::Error(TEXT("Failed to create UAnimMontage object"));
	}

	Montage->SetSkeleton(Skeleton);

	// Add default slot if none exists
	if (Montage->SlotAnimTracks.Num() == 0)
	{
		Montage->AddSlot(FName(TEXT("DefaultSlot")));
	}

	// Add default section at time 0
	Montage->AddAnimCompositeSection(FName(TEXT("Default")), 0.0f);

	FAssetRegistryModule::AssetCreated(Montage);
	Pkg->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), Montage->GetPathName());
	Root->SetStringField(TEXT("asset_name"), AssetName);
	Root->SetStringField(TEXT("skeleton"), Skeleton->GetPathName());
	Root->SetNumberField(TEXT("slot_count"), Montage->SlotAnimTracks.Num());
	Root->SetNumberField(TEXT("section_count"), Montage->CompositeSections.Num());
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleSetMontageBlend(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UAnimMontage* Montage = FMonolithAssetUtils::LoadAssetByPath<UAnimMontage>(AssetPath);
	if (!Montage) return FMonolithActionResult::Error(FString::Printf(TEXT("Montage not found: %s"), *AssetPath));

	bool bAnySet = false;

	GEditor->BeginTransaction(FText::FromString(TEXT("Set Montage Blend")));
	Montage->Modify();

	if (Params->HasField(TEXT("blend_in_time")))
	{
		Montage->BlendIn.SetBlendTime(static_cast<float>(Params->GetNumberField(TEXT("blend_in_time"))));
		bAnySet = true;
	}
	if (Params->HasField(TEXT("blend_out_time")))
	{
		Montage->BlendOut.SetBlendTime(static_cast<float>(Params->GetNumberField(TEXT("blend_out_time"))));
		bAnySet = true;
	}
	if (Params->HasField(TEXT("blend_out_trigger_time")))
	{
		Montage->BlendOutTriggerTime = static_cast<float>(Params->GetNumberField(TEXT("blend_out_trigger_time")));
		bAnySet = true;
	}
	if (Params->HasField(TEXT("enable_auto_blend_out")))
	{
		Montage->bEnableAutoBlendOut = Params->GetBoolField(TEXT("enable_auto_blend_out"));
		bAnySet = true;
	}

	if (!bAnySet)
	{
		GEditor->EndTransaction();
		return FMonolithActionResult::Error(TEXT("At least one of blend_in_time, blend_out_time, blend_out_trigger_time, or enable_auto_blend_out must be provided"));
	}

	GEditor->EndTransaction();
	Montage->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetNumberField(TEXT("blend_in_time"), Montage->BlendIn.GetBlendTime());
	Root->SetNumberField(TEXT("blend_out_time"), Montage->BlendOut.GetBlendTime());
	Root->SetNumberField(TEXT("blend_out_trigger_time"), Montage->BlendOutTriggerTime);
	Root->SetBoolField(TEXT("enable_auto_blend_out"), Montage->bEnableAutoBlendOut);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleAddMontageSlot(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString SlotName = Params->GetStringField(TEXT("slot_name"));

	UAnimMontage* Montage = FMonolithAssetUtils::LoadAssetByPath<UAnimMontage>(AssetPath);
	if (!Montage) return FMonolithActionResult::Error(FString::Printf(TEXT("Montage not found: %s"), *AssetPath));

	// Check if slot already exists
	for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
	{
		if (Track.SlotName == FName(*SlotName))
		{
			return FMonolithActionResult::Error(FString::Printf(TEXT("Slot '%s' already exists on montage"), *SlotName));
		}
	}

	GEditor->BeginTransaction(FText::FromString(TEXT("Add Montage Slot")));
	Montage->Modify();

	Montage->AddSlot(FName(*SlotName));

	GEditor->EndTransaction();
	Montage->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetStringField(TEXT("slot_name"), SlotName);
	Root->SetNumberField(TEXT("slot_count"), Montage->SlotAnimTracks.Num());
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleSetMontageSlot(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	int32 SlotIndex = static_cast<int32>(Params->GetNumberField(TEXT("slot_index")));
	FString SlotName = Params->GetStringField(TEXT("slot_name"));

	UAnimMontage* Montage = FMonolithAssetUtils::LoadAssetByPath<UAnimMontage>(AssetPath);
	if (!Montage) return FMonolithActionResult::Error(FString::Printf(TEXT("Montage not found: %s"), *AssetPath));

	if (SlotIndex < 0 || SlotIndex >= Montage->SlotAnimTracks.Num())
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Invalid slot index %d (montage has %d slots)"), SlotIndex, Montage->SlotAnimTracks.Num()));
	}

	FName OldName = Montage->SlotAnimTracks[SlotIndex].SlotName;

	GEditor->BeginTransaction(FText::FromString(TEXT("Set Montage Slot")));
	Montage->Modify();
	Montage->SlotAnimTracks[SlotIndex].SlotName = FName(*SlotName);
	GEditor->EndTransaction();
	Montage->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetNumberField(TEXT("slot_index"), SlotIndex);
	Root->SetStringField(TEXT("old_slot_name"), OldName.ToString());
	Root->SetStringField(TEXT("new_slot_name"), SlotName);
	return FMonolithActionResult::Success(Root);
}

// ---------------------------------------------------------------------------
// Wave 7 — Anim Modifiers + Composites
// ---------------------------------------------------------------------------

FMonolithActionResult FMonolithAnimationActions::HandleApplyAnimModifier(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ModifierClass = Params->GetStringField(TEXT("modifier_class"));

	UAnimSequence* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimSequence not found: %s"), *AssetPath));

	// Try to find the class — with and without U prefix
	UClass* ModifierUClass = FindFirstObject<UClass>(*ModifierClass, EFindFirstObjectOptions::NativeFirst);
	if (!ModifierUClass && !ModifierClass.StartsWith(TEXT("U")))
	{
		ModifierUClass = FindFirstObject<UClass>(*(TEXT("U") + ModifierClass), EFindFirstObjectOptions::NativeFirst);
	}
	if (!ModifierUClass)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Modifier class not found: %s"), *ModifierClass));
	}

	if (!ModifierUClass->IsChildOf(UAnimationModifier::StaticClass()))
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Class '%s' is not a UAnimationModifier subclass"), *ModifierClass));
	}

	UAnimationModifier* Modifier = NewObject<UAnimationModifier>(GetTransientPackage(), ModifierUClass);
	if (!Modifier)
	{
		return FMonolithActionResult::Error(TEXT("Failed to create modifier instance"));
	}

	Modifier->ApplyToAnimationSequence(Seq);
	Seq->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetStringField(TEXT("modifier_class"), ModifierUClass->GetName());
	Root->SetStringField(TEXT("status"), TEXT("applied"));
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleListAnimModifiers(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UAnimSequence* Seq = FMonolithAssetUtils::LoadAssetByPath<UAnimSequence>(AssetPath);
	if (!Seq) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimSequence not found: %s"), *AssetPath));

	TArray<TSharedPtr<FJsonValue>> ModArray;

	// Iterate asset user data to find animation modifiers
	const TArray<UAssetUserData*>* UserDataArray = Seq->GetAssetUserDataArray();
	if (UserDataArray)
	{
		for (UAssetUserData* UserData : *UserDataArray)
		{
			if (!UserData) continue;

			// Check if this is the AnimationModifiersAssetUserData
			if (UserData->GetClass()->GetName().Contains(TEXT("AnimationModifiersAssetUserData")))
			{
				// Use reflection to get the modifiers array
				FArrayProperty* ModifiersProp = CastField<FArrayProperty>(UserData->GetClass()->FindPropertyByName(TEXT("AnimationModifierInstances")));
				if (ModifiersProp)
				{
					FScriptArrayHelper ArrayHelper(ModifiersProp, ModifiersProp->ContainerPtrToValuePtr<void>(UserData));
					FObjectProperty* InnerProp = CastField<FObjectProperty>(ModifiersProp->Inner);
					if (InnerProp)
					{
						for (int32 i = 0; i < ArrayHelper.Num(); ++i)
						{
							UObject* ModObj = InnerProp->GetObjectPropertyValue(ArrayHelper.GetElementPtr(i));
							if (ModObj)
							{
								TSharedPtr<FJsonObject> ModJson = MakeShared<FJsonObject>();
								ModJson->SetNumberField(TEXT("index"), i);
								ModJson->SetStringField(TEXT("class"), ModObj->GetClass()->GetName());
								ModJson->SetStringField(TEXT("name"), ModObj->GetName());
								ModArray.Add(MakeShared<FJsonValueObject>(ModJson));
							}
						}
					}
				}
			}
		}
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetArrayField(TEXT("modifiers"), ModArray);
	Root->SetNumberField(TEXT("count"), ModArray.Num());
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleGetCompositeInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UAnimComposite* Composite = FMonolithAssetUtils::LoadAssetByPath<UAnimComposite>(AssetPath);
	if (!Composite) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimComposite not found: %s"), *AssetPath));

	TArray<TSharedPtr<FJsonValue>> SegArray;
	const TArray<FAnimSegment>& Segments = Composite->AnimationTrack.AnimSegments;
	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		const FAnimSegment& Seg = Segments[i];
		TSharedPtr<FJsonObject> SegJson = MakeShared<FJsonObject>();
		SegJson->SetNumberField(TEXT("index"), i);

		UAnimSequenceBase* AnimRef = Seg.GetAnimReference();
		SegJson->SetStringField(TEXT("anim_reference"), AnimRef ? AnimRef->GetPathName() : TEXT("None"));
		SegJson->SetNumberField(TEXT("start_pos"), Seg.StartPos);
		SegJson->SetNumberField(TEXT("anim_start_time"), Seg.AnimStartTime);
		SegJson->SetNumberField(TEXT("anim_end_time"), Seg.AnimEndTime);
		SegJson->SetNumberField(TEXT("anim_play_rate"), Seg.AnimPlayRate);
		SegJson->SetNumberField(TEXT("looping_count"), Seg.LoopingCount);

		SegArray.Add(MakeShared<FJsonValueObject>(SegJson));
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetStringField(TEXT("skeleton"), Composite->GetSkeleton() ? Composite->GetSkeleton()->GetPathName() : TEXT("None"));
	Root->SetNumberField(TEXT("duration"), Composite->GetPlayLength());
	Root->SetArrayField(TEXT("segments"), SegArray);
	Root->SetNumberField(TEXT("segment_count"), Segments.Num());
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleAddCompositeSegment(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString AnimPath = Params->GetStringField(TEXT("anim_path"));
	float StartPos = Params->HasField(TEXT("start_pos")) ? static_cast<float>(Params->GetNumberField(TEXT("start_pos"))) : 0.0f;
	float PlayRate = Params->HasField(TEXT("play_rate")) ? static_cast<float>(Params->GetNumberField(TEXT("play_rate"))) : 1.0f;
	int32 LoopingCount = Params->HasField(TEXT("looping_count")) ? static_cast<int32>(Params->GetNumberField(TEXT("looping_count"))) : 1;

	UAnimComposite* Composite = FMonolithAssetUtils::LoadAssetByPath<UAnimComposite>(AssetPath);
	if (!Composite) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimComposite not found: %s"), *AssetPath));

	UAnimSequenceBase* Anim = FMonolithAssetUtils::LoadAssetByPath<UAnimSequenceBase>(AnimPath);
	if (!Anim) return FMonolithActionResult::Error(FString::Printf(TEXT("Animation not found: %s"), *AnimPath));

	GEditor->BeginTransaction(FText::FromString(TEXT("Add Composite Segment")));
	Composite->Modify();

	FAnimSegment NewSeg;
	NewSeg.SetAnimReference(Anim);
	NewSeg.StartPos = StartPos;
	NewSeg.AnimStartTime = 0.0f;
	NewSeg.AnimEndTime = Anim->GetPlayLength();
	NewSeg.AnimPlayRate = PlayRate;
	NewSeg.LoopingCount = LoopingCount;

	int32 NewIndex = Composite->AnimationTrack.AnimSegments.Add(NewSeg);

	GEditor->EndTransaction();
	Composite->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetNumberField(TEXT("segment_index"), NewIndex);
	Root->SetStringField(TEXT("anim_reference"), AnimPath);
	Root->SetNumberField(TEXT("start_pos"), StartPos);
	Root->SetNumberField(TEXT("play_rate"), PlayRate);
	Root->SetNumberField(TEXT("looping_count"), LoopingCount);
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithAnimationActions::HandleRemoveCompositeSegment(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	int32 SegmentIndex = static_cast<int32>(Params->GetNumberField(TEXT("segment_index")));

	UAnimComposite* Composite = FMonolithAssetUtils::LoadAssetByPath<UAnimComposite>(AssetPath);
	if (!Composite) return FMonolithActionResult::Error(FString::Printf(TEXT("AnimComposite not found: %s"), *AssetPath));

	TArray<FAnimSegment>& Segments = Composite->AnimationTrack.AnimSegments;
	if (SegmentIndex < 0 || SegmentIndex >= Segments.Num())
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Invalid segment index %d (composite has %d segments)"), SegmentIndex, Segments.Num()));
	}

	// Capture info before removal
	UAnimSequenceBase* AnimRef = Segments[SegmentIndex].GetAnimReference();
	FString AnimName = AnimRef ? AnimRef->GetPathName() : TEXT("None");

	GEditor->BeginTransaction(FText::FromString(TEXT("Remove Composite Segment")));
	Composite->Modify();

	Segments.RemoveAt(SegmentIndex);

	GEditor->EndTransaction();
	Composite->MarkPackageDirty();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetNumberField(TEXT("removed_index"), SegmentIndex);
	Root->SetStringField(TEXT("removed_anim"), AnimName);
	Root->SetNumberField(TEXT("remaining_segments"), Segments.Num());
	return FMonolithActionResult::Success(Root);
}
