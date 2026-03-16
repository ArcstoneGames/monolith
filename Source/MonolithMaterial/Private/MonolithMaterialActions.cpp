#include "MonolithMaterialActions.h"
#include "MonolithToolRegistry.h"
#include "MonolithParamSchema.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Materials/MaterialExpressionMaterialAttributeLayers.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialInstanceConstant.h"
#include "EditorAssetLibrary.h"
#include "MaterialEditingLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"
#include "MaterialShared.h"
#include "RHIShaderPlatform.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "ObjectTools.h"
#include "ImageUtils.h"
#include "UObject/SavePackage.h"

// ============================================================================
// Registration
// ============================================================================

void FMonolithMaterialActions::RegisterActions(FMonolithToolRegistry& Registry)
{
	Registry.RegisterAction(TEXT("material"), TEXT("get_all_expressions"),
		TEXT("Get all expression nodes in a base material"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::GetAllExpressions),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("get_expression_details"),
		TEXT("Get full property reflection, inputs, and outputs for a single expression"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::GetExpressionDetails),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Required(TEXT("expression_name"), TEXT("string"), TEXT("Name of the expression node"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("get_full_connection_graph"),
		TEXT("Get the complete connection graph (all wires) of a material"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::GetFullConnectionGraph),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("disconnect_expression"),
		TEXT("Disconnect inputs or outputs on a named expression"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::DisconnectExpression),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Required(TEXT("expression_name"), TEXT("string"), TEXT("Name of the expression to disconnect"))
			.Optional(TEXT("input_name"), TEXT("string"), TEXT("Specific input to disconnect"))
			.Optional(TEXT("disconnect_outputs"), TEXT("bool"), TEXT("Also disconnect outputs"), TEXT("false"))
			.Optional(TEXT("target_expression"), TEXT("string"), TEXT("Only disconnect from this specific downstream expression (requires disconnect_outputs=true)"))
			.Optional(TEXT("output_index"), TEXT("integer"), TEXT("Only disconnect connections using this output index"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("build_material_graph"),
		TEXT("Build entire material graph from JSON spec in a single undo transaction"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::BuildMaterialGraph),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Required(TEXT("graph_spec"), TEXT("object"), TEXT("JSON specification of the material graph"))
			.Optional(TEXT("clear_existing"), TEXT("bool"), TEXT("Clear existing expressions before building"), TEXT("false"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("begin_transaction"),
		TEXT("Begin a named undo transaction for batching edits"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::BeginTransaction),
		FParamSchemaBuilder()
			.Required(TEXT("transaction_name"), TEXT("string"), TEXT("Name for the undo transaction"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("end_transaction"),
		TEXT("End the current undo transaction"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::EndTransaction),
		MakeShared<FJsonObject>());

	Registry.RegisterAction(TEXT("material"), TEXT("export_material_graph"),
		TEXT("Export complete material graph to JSON (round-trippable with build_material_graph)"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::ExportMaterialGraph),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Optional(TEXT("include_properties"), TEXT("bool"), TEXT("Include full property data"), TEXT("true"))
			.Optional(TEXT("include_positions"), TEXT("bool"), TEXT("Include node positions"), TEXT("true"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("import_material_graph"),
		TEXT("Import material graph from JSON string. Mode: overwrite or merge"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::ImportMaterialGraph),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Required(TEXT("graph_json"), TEXT("string"), TEXT("JSON string of the material graph"))
			.Optional(TEXT("mode"), TEXT("string"), TEXT("Import mode: overwrite or merge"), TEXT("overwrite"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("validate_material"),
		TEXT("Validate material graph health and optionally auto-fix issues"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::ValidateMaterial),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Optional(TEXT("fix_issues"), TEXT("bool"), TEXT("Auto-fix detected issues"), TEXT("false"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("render_preview"),
		TEXT("Render material preview to PNG file"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::RenderPreview),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Optional(TEXT("resolution"), TEXT("integer"), TEXT("Preview resolution in pixels"), TEXT("256"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("get_thumbnail"),
		TEXT("Get material thumbnail as base64-encoded PNG"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::GetThumbnail),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Optional(TEXT("resolution"), TEXT("integer"), TEXT("Thumbnail resolution"), TEXT("256"))
			.Optional(TEXT("save_to_file"), TEXT("string"), TEXT("Optional file path to save PNG to disk"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("create_custom_hlsl_node"),
		TEXT("Create a Custom HLSL expression node with inputs, outputs, and code"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::CreateCustomHLSLNode),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Required(TEXT("code"), TEXT("string"), TEXT("HLSL code for the custom node"))
			.Optional(TEXT("description"), TEXT("string"), TEXT("Node description"))
			.Optional(TEXT("output_type"), TEXT("string"), TEXT("Output type (float, float2, float3, float4)"))
			.Optional(TEXT("pos_x"), TEXT("integer"), TEXT("Node X position in graph"))
			.Optional(TEXT("pos_y"), TEXT("integer"), TEXT("Node Y position in graph"))
			.Optional(TEXT("inputs"), TEXT("array"), TEXT("Array of input pin definitions"))
			.Optional(TEXT("additional_outputs"), TEXT("array"), TEXT("Array of additional output pin definitions"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("get_layer_info"),
		TEXT("Get Material Layer or Material Layer Blend info"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::GetLayerInfo),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material Layer or Layer Blend asset path"))
			.Build());

	// --- Wave 2: Asset creation & properties ---

	Registry.RegisterAction(TEXT("material"), TEXT("create_material"),
		TEXT("Create a new empty material asset at the specified path"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::CreateMaterial),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Asset path e.g. /Game/Materials/M_MyMaterial"))
			.Optional(TEXT("blend_mode"), TEXT("string"), TEXT("Opaque, Masked, Translucent, Additive, Modulate, AlphaComposite"), TEXT("Opaque"))
			.Optional(TEXT("shading_model"), TEXT("string"), TEXT("DefaultLit, Unlit, Subsurface, SubsurfaceProfile, ClearCoat, TwoSidedFoliage"), TEXT("DefaultLit"))
			.Optional(TEXT("material_domain"), TEXT("string"), TEXT("Surface, DeferredDecal, PostProcess, LightFunction, UI"), TEXT("Surface"))
			.Optional(TEXT("two_sided"), TEXT("bool"), TEXT("Enable two-sided rendering"), TEXT("false"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("create_material_instance"),
		TEXT("Create a material instance constant from a parent material"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::CreateMaterialInstance),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Path for the new instance e.g. /Game/Materials/MI_MyInstance"))
			.Required(TEXT("parent_material"), TEXT("string"), TEXT("Path to parent material or material instance"))
			.Optional(TEXT("scalar_parameters"), TEXT("object"), TEXT("Map of scalar param name to float value"))
			.Optional(TEXT("vector_parameters"), TEXT("object"), TEXT("Map of vector param name to {R,G,B,A} object"))
			.Optional(TEXT("texture_parameters"), TEXT("object"), TEXT("Map of texture param name to texture asset path"))
			.Optional(TEXT("static_switch_parameters"), TEXT("object"), TEXT("Map of static switch param name to bool value"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("set_material_property"),
		TEXT("Set top-level material properties (blend mode, shading model, domain, two-sided, opacity mask clip, usage flags, etc.)"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::SetMaterialProperty),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Optional(TEXT("blend_mode"), TEXT("string"), TEXT("Opaque, Masked, Translucent, Additive, Modulate, AlphaComposite"))
			.Optional(TEXT("shading_model"), TEXT("string"), TEXT("DefaultLit, Unlit, Subsurface, SubsurfaceProfile, ClearCoat, TwoSidedFoliage"))
			.Optional(TEXT("material_domain"), TEXT("string"), TEXT("Surface, DeferredDecal, PostProcess, LightFunction, UI"))
			.Optional(TEXT("two_sided"), TEXT("bool"), TEXT("Enable two-sided rendering"))
			.Optional(TEXT("opacity_mask_clip_value"), TEXT("number"), TEXT("Clip value for masked blend mode"))
			.Optional(TEXT("dithered_lod_transition"), TEXT("bool"), TEXT("Enable dithered LOD transition"))
			.Optional(TEXT("fully_rough"), TEXT("bool"), TEXT("Enable fully rough (disables specular)"))
			.Optional(TEXT("cast_shadow_as_masked"), TEXT("bool"), TEXT("Cast shadow as masked"))
			.Optional(TEXT("output_velocity"), TEXT("bool"), TEXT("Output per-pixel velocity"))
			.Optional(TEXT("used_with_skeletal_mesh"), TEXT("bool"), TEXT("Mark as used with skeletal meshes"))
			.Optional(TEXT("used_with_particle_sprites"), TEXT("bool"), TEXT("Mark as used with particle sprites"))
			.Optional(TEXT("used_with_niagara_sprites"), TEXT("bool"), TEXT("Mark as used with Niagara sprites"))
			.Optional(TEXT("used_with_niagara_meshes"), TEXT("bool"), TEXT("Mark as used with Niagara meshes"))
			.Optional(TEXT("used_with_niagara_ribbons"), TEXT("bool"), TEXT("Mark as used with Niagara ribbons"))
			.Optional(TEXT("used_with_morph_targets"), TEXT("bool"), TEXT("Mark as used with morph targets"))
			.Optional(TEXT("used_with_instanced_static_meshes"), TEXT("bool"), TEXT("Mark as used with instanced static meshes"))
			.Optional(TEXT("used_with_static_lighting"), TEXT("bool"), TEXT("Mark as used with static lighting"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("delete_expression"),
		TEXT("Delete a material expression node by name, cleaning up all connections"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::DeleteExpression),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Required(TEXT("expression_name"), TEXT("string"), TEXT("Name of the expression to delete"))
			.Build());

	// --- Wave 2B: Parameter management, recompile, duplicate ---

	Registry.RegisterAction(TEXT("material"), TEXT("get_material_parameters"),
		TEXT("List all parameters in a material or material instance with types, defaults, and groups"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::GetMaterialParameters),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material or material instance asset path"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("set_instance_parameter"),
		TEXT("Set a parameter override on an existing material instance"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::SetInstanceParameter),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material instance asset path"))
			.Required(TEXT("parameter_name"), TEXT("string"), TEXT("Parameter name to set"))
			.Optional(TEXT("scalar_value"), TEXT("number"), TEXT("Float value for scalar parameters"))
			.Optional(TEXT("vector_value"), TEXT("object"), TEXT("{R,G,B,A} object for vector parameters"))
			.Optional(TEXT("texture_value"), TEXT("string"), TEXT("Texture asset path for texture parameters"))
			.Optional(TEXT("switch_value"), TEXT("bool"), TEXT("Boolean for static switch parameters"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("recompile_material"),
		TEXT("Force recompile a material and return success/failure"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::RecompileMaterial),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("duplicate_material"),
		TEXT("Duplicate an existing material or material instance to a new path"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::DuplicateMaterial),
		FParamSchemaBuilder()
			.Required(TEXT("source_path"), TEXT("string"), TEXT("Path of the material to duplicate"))
			.Required(TEXT("dest_path"), TEXT("string"), TEXT("Destination path for the copy"))
			.Build());

	// --- Wave 2C: Advanced utilities ---

	Registry.RegisterAction(TEXT("material"), TEXT("get_compilation_stats"),
		TEXT("Get shader compilation statistics: instruction count, texture samplers, errors"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::GetCompilationStats),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("set_expression_property"),
		TEXT("Set a property value on an existing material expression node"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::SetExpressionProperty),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Required(TEXT("expression_name"), TEXT("string"), TEXT("Name of the expression node"))
			.Required(TEXT("property_name"), TEXT("string"), TEXT("Property to set (e.g. R, ParameterName, SamplerType, Texture)"))
			.Required(TEXT("value"), TEXT("string"), TEXT("Value as string (parsed via ImportText for complex types) or number"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("connect_expressions"),
		TEXT("Connect an expression output to another expression input or a material output property"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::ConnectExpressions),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Required(TEXT("from_expression"), TEXT("string"), TEXT("Source expression name"))
			.Optional(TEXT("from_output"), TEXT("string"), TEXT("Source output pin name (empty = default)"))
			.Optional(TEXT("to_expression"), TEXT("string"), TEXT("Target expression name (for expr-to-expr)"))
			.Optional(TEXT("to_input"), TEXT("string"), TEXT("Target input pin name (empty = default)"))
			.Optional(TEXT("to_property"), TEXT("string"), TEXT("Material property name: BaseColor, Roughness, etc. (for expr-to-material)"))
			.Build());

	// --- Wave 3: Free wins (graph utilities & inspection) ---

	Registry.RegisterAction(TEXT("material"), TEXT("auto_layout"),
		TEXT("Auto-layout all expression nodes in a material or material function graph"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::AutoLayout),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material or material function asset path"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("duplicate_expression"),
		TEXT("Duplicate an expression node within the same material. Output connections are NOT duplicated (input connections are preserved)"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::DuplicateExpression),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Required(TEXT("expression_name"), TEXT("string"), TEXT("Name of the expression to duplicate"))
			.Optional(TEXT("offset_x"), TEXT("integer"), TEXT("X offset from original position"), TEXT("50"))
			.Optional(TEXT("offset_y"), TEXT("integer"), TEXT("Y offset from original position"), TEXT("50"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("list_expression_classes"),
		TEXT("List all available material expression classes with pin counts. First call may take 1-2s (cached after)"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::ListExpressionClasses),
		FParamSchemaBuilder()
			.Optional(TEXT("filter"), TEXT("string"), TEXT("Substring filter on class name (case-insensitive)"))
			.Optional(TEXT("category"), TEXT("string"), TEXT("Filter by MenuCategories substring"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("get_expression_connections"),
		TEXT("Get all input connections and output consumers for a single expression"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::GetExpressionConnections),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Required(TEXT("expression_name"), TEXT("string"), TEXT("Name of the expression"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("move_expression"),
		TEXT("Move one or more expression nodes to new positions"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::MoveExpression),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Optional(TEXT("expression_name"), TEXT("string"), TEXT("Single expression to move"))
			.Optional(TEXT("pos_x"), TEXT("integer"), TEXT("X position (or offset if relative=true)"))
			.Optional(TEXT("pos_y"), TEXT("integer"), TEXT("Y position (or offset if relative=true)"))
			.Optional(TEXT("relative"), TEXT("bool"), TEXT("Treat pos_x/pos_y as offsets from current position"), TEXT("false"))
			.Optional(TEXT("expressions"), TEXT("array"), TEXT("Batch: array of {name, x, y} objects"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("get_material_properties"),
		TEXT("Get top-level material properties: blend mode, shading model, domain, usage flags, etc."),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::GetMaterialProperties),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material or material instance asset path"))
			.Build());

	// --- Wave 4: Instance & property improvements ---

	Registry.RegisterAction(TEXT("material"), TEXT("get_instance_parameters"),
		TEXT("Read all parameter overrides from a material instance, with override detection vs parent"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::GetInstanceParameters),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material instance asset path"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("set_instance_parameters"),
		TEXT("Set multiple parameter overrides on a material instance in one call (batch)"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::SetInstanceParameters),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material instance asset path"))
			.Required(TEXT("parameters"), TEXT("array"), TEXT("Array of {name, type (scalar/vector/texture/switch), value}"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("set_instance_parent"),
		TEXT("Reparent a material instance to a new parent material, reporting lost/kept parameters"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::SetInstanceParent),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material instance asset path"))
			.Required(TEXT("new_parent"), TEXT("string"), TEXT("Path to new parent material or material instance"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("clear_instance_parameter"),
		TEXT("Remove a single parameter override (or all overrides) from a material instance"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::ClearInstanceParameter),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material instance asset path"))
			.Optional(TEXT("parameter_name"), TEXT("string"), TEXT("Specific parameter to clear (omit for all)"))
			.Optional(TEXT("parameter_type"), TEXT("string"), TEXT("scalar, vector, texture, switch, or all (default: all)"), TEXT("all"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("save_material"),
		TEXT("Save a material or material instance asset to disk"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::SaveMaterial),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Optional(TEXT("only_if_dirty"), TEXT("bool"), TEXT("Only save if the asset has unsaved changes"), TEXT("true"))
			.Build());

	// --- Wave 5: Graph editing power ---

	Registry.RegisterAction(TEXT("material"), TEXT("update_custom_hlsl_node"),
		TEXT("Update fields on an existing Custom HLSL expression node (code, inputs, outputs, etc.)"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::UpdateCustomHlslNode),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Required(TEXT("expression_name"), TEXT("string"), TEXT("Name of the UMaterialExpressionCustom node"))
			.Optional(TEXT("code"), TEXT("string"), TEXT("New HLSL code"))
			.Optional(TEXT("description"), TEXT("string"), TEXT("New description/label"))
			.Optional(TEXT("output_type"), TEXT("string"), TEXT("Output type: Float1, Float2, Float3, Float4"))
			.Optional(TEXT("inputs"), TEXT("array"), TEXT("Array of {name, type?} input pin definitions (replaces all inputs)"))
			.Optional(TEXT("additional_outputs"), TEXT("array"), TEXT("Array of {name, type} output pin definitions (replaces all additional outputs)"))
			.Optional(TEXT("include_file_paths"), TEXT("array"), TEXT("Array of HLSL include file paths"))
			.Optional(TEXT("additional_defines"), TEXT("array"), TEXT("Array of {name, value} preprocessor defines"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("replace_expression"),
		TEXT("Replace an expression with a new one, preserving connections by pin name (index fallback with warnings)"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::ReplaceExpression),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Required(TEXT("expression_name"), TEXT("string"), TEXT("Name of the expression to replace"))
			.Required(TEXT("new_class"), TEXT("string"), TEXT("Class name for the new expression (e.g. VectorParameter, Multiply)"))
			.Optional(TEXT("new_properties"), TEXT("object"), TEXT("Properties to set on the new expression via reflection"))
			.Optional(TEXT("preserve_connections"), TEXT("bool"), TEXT("Attempt to reconnect wires by pin name, then index fallback"), TEXT("true"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("get_expression_pin_info"),
		TEXT("Get input and output pin info for a material expression class (without needing a material asset)"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::GetExpressionPinInfo),
		FParamSchemaBuilder()
			.Required(TEXT("class_name"), TEXT("string"), TEXT("Expression class name (e.g. MaterialExpressionMultiply or Multiply)"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("rename_expression"),
		TEXT("Set the user-visible description/label on an expression node in the graph editor"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::RenameExpression),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path"))
			.Required(TEXT("expression_name"), TEXT("string"), TEXT("Name of the expression node"))
			.Required(TEXT("new_desc"), TEXT("string"), TEXT("New description text (visible label in graph editor)"))
			.Build());

	Registry.RegisterAction(TEXT("material"), TEXT("list_material_instances"),
		TEXT("Find all material instances that reference a parent material (recursively walks the instance tree)"),
		FMonolithActionHandler::CreateStatic(&FMonolithMaterialActions::ListMaterialInstances),
		FParamSchemaBuilder()
			.Required(TEXT("parent_path"), TEXT("string"), TEXT("Path to the parent material or material instance"))
			.Optional(TEXT("recursive"), TEXT("bool"), TEXT("Recursively find instances of instances"), TEXT("true"))
			.Build());
}

// ============================================================================
// Helpers
// ============================================================================

UMaterial* FMonolithMaterialActions::LoadBaseMaterial(const FString& AssetPath)
{
	// Try UEditorAssetLibrary first (loads from disk)
	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (LoadedAsset)
	{
		return Cast<UMaterial>(LoadedAsset);
	}

	// Fallback: check in-memory objects (e.g. freshly created, not yet saved)
	FString FullObjectPath = AssetPath;
	if (!FullObjectPath.Contains(TEXT(".")))
	{
		// Convert "/Game/Path/Name" to "/Game/Path/Name.Name"
		int32 LastSlash;
		if (FullObjectPath.FindLastChar('/', LastSlash))
		{
			FString ObjName = FullObjectPath.Mid(LastSlash + 1);
			FullObjectPath = FString::Printf(TEXT("%s.%s"), *AssetPath, *ObjName);
		}
	}
	UObject* Found = FindFirstObject<UMaterial>(*FullObjectPath, EFindFirstObjectOptions::NativeFirst);
	return Cast<UMaterial>(Found);
}

TSharedPtr<FJsonObject> FMonolithMaterialActions::SerializeExpression(const UMaterialExpression* Expression)
{
	auto ExprJson = MakeShared<FJsonObject>();

	ExprJson->SetStringField(TEXT("name"), Expression->GetName());
	ExprJson->SetStringField(TEXT("class"), Expression->GetClass()->GetName());
	ExprJson->SetNumberField(TEXT("pos_x"), Expression->MaterialExpressionEditorX);
	ExprJson->SetNumberField(TEXT("pos_y"), Expression->MaterialExpressionEditorY);

	if (const auto* TexSampleParam = Cast<UMaterialExpressionTextureSampleParameter>(Expression))
	{
		ExprJson->SetStringField(TEXT("parameter_name"), TexSampleParam->ParameterName.ToString());
		if (TexSampleParam->Texture)
		{
			ExprJson->SetStringField(TEXT("texture"), TexSampleParam->Texture->GetPathName());
		}
	}
	else if (const auto* Param = Cast<UMaterialExpressionParameter>(Expression))
	{
		ExprJson->SetStringField(TEXT("parameter_name"), Param->ParameterName.ToString());
	}
	else if (const auto* TexBase = Cast<UMaterialExpressionTextureBase>(Expression))
	{
		if (TexBase->Texture)
		{
			ExprJson->SetStringField(TEXT("texture"), TexBase->Texture->GetPathName());
		}
	}

	if (const auto* Custom = Cast<UMaterialExpressionCustom>(Expression))
	{
		FString CodePreview = Custom->Code.Left(100);
		ExprJson->SetStringField(TEXT("code"), CodePreview);
	}

	if (const auto* Comment = Cast<UMaterialExpressionComment>(Expression))
	{
		ExprJson->SetStringField(TEXT("text"), Comment->Text);
	}

	if (const auto* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
	{
		if (FuncCall->MaterialFunction)
		{
			ExprJson->SetStringField(TEXT("function"), FuncCall->MaterialFunction->GetPathName());
		}
	}

	return ExprJson;
}

/** Map string property name to EMaterialProperty. */
static EMaterialProperty ParseMaterialProperty(const FString& PropName)
{
	static const TMap<FString, EMaterialProperty> Map = {
		{ TEXT("BaseColor"),            MP_BaseColor },
		{ TEXT("Metallic"),             MP_Metallic },
		{ TEXT("Specular"),             MP_Specular },
		{ TEXT("Roughness"),            MP_Roughness },
		{ TEXT("Anisotropy"),           MP_Anisotropy },
		{ TEXT("EmissiveColor"),        MP_EmissiveColor },
		{ TEXT("Opacity"),              MP_Opacity },
		{ TEXT("OpacityMask"),          MP_OpacityMask },
		{ TEXT("Normal"),               MP_Normal },
		{ TEXT("WorldPositionOffset"),  MP_WorldPositionOffset },
		{ TEXT("SubsurfaceColor"),      MP_SubsurfaceColor },
		{ TEXT("AmbientOcclusion"),     MP_AmbientOcclusion },
		{ TEXT("Refraction"),           MP_Refraction },
		{ TEXT("PixelDepthOffset"),     MP_PixelDepthOffset },
		{ TEXT("ShadingModel"),         MP_ShadingModel },
	};

	const EMaterialProperty* Found = Map.Find(PropName);
	return Found ? *Found : MP_MAX;
}

/** Map string to ECustomMaterialOutputType. */
static ECustomMaterialOutputType ParseCustomOutputType(const FString& TypeName)
{
	if (TypeName == TEXT("CMOT_Float1") || TypeName == TEXT("Float1")) return CMOT_Float1;
	if (TypeName == TEXT("CMOT_Float2") || TypeName == TEXT("Float2")) return CMOT_Float2;
	if (TypeName == TEXT("CMOT_Float3") || TypeName == TEXT("Float3")) return CMOT_Float3;
	if (TypeName == TEXT("CMOT_Float4") || TypeName == TEXT("Float4")) return CMOT_Float4;
	return CMOT_Float1;
}

/** Map ECustomMaterialOutputType to string. */
static FString CustomOutputTypeToString(ECustomMaterialOutputType Type)
{
	switch (Type)
	{
	case CMOT_Float1: return TEXT("Float1");
	case CMOT_Float2: return TEXT("Float2");
	case CMOT_Float3: return TEXT("Float3");
	case CMOT_Float4: return TEXT("Float4");
	default: return TEXT("Float1");
	}
}

/** Map EMaterialProperty to string name. */
static FString MaterialPropertyToString(EMaterialProperty Prop)
{
	switch (Prop)
	{
	case MP_BaseColor:            return TEXT("BaseColor");
	case MP_Metallic:             return TEXT("Metallic");
	case MP_Specular:             return TEXT("Specular");
	case MP_Roughness:            return TEXT("Roughness");
	case MP_Anisotropy:           return TEXT("Anisotropy");
	case MP_EmissiveColor:        return TEXT("EmissiveColor");
	case MP_Opacity:              return TEXT("Opacity");
	case MP_OpacityMask:          return TEXT("OpacityMask");
	case MP_Normal:               return TEXT("Normal");
	case MP_WorldPositionOffset:  return TEXT("WorldPositionOffset");
	case MP_SubsurfaceColor:      return TEXT("SubsurfaceColor");
	case MP_AmbientOcclusion:     return TEXT("AmbientOcclusion");
	case MP_Refraction:           return TEXT("Refraction");
	case MP_PixelDepthOffset:     return TEXT("PixelDepthOffset");
	case MP_ShadingModel:         return TEXT("ShadingModel");
	default:                      return TEXT("");
	}
}

/** All material properties for iteration */
static const EMaterialProperty AllMaterialProperties[] =
{
	MP_BaseColor, MP_Metallic, MP_Specular, MP_Roughness, MP_Anisotropy,
	MP_EmissiveColor, MP_Opacity, MP_OpacityMask, MP_Normal,
	MP_WorldPositionOffset, MP_SubsurfaceColor, MP_AmbientOcclusion,
	MP_Refraction, MP_PixelDepthOffset, MP_ShadingModel,
	MP_Tangent, MP_Displacement, MP_CustomData0, MP_CustomData1,
	MP_SurfaceThickness, MP_FrontMaterial, MP_MaterialAttributes,
};

/** Material output entries for connection graph */
struct FMaterialOutputEntry
{
	EMaterialProperty Property;
	const TCHAR* Name;
};

static const FMaterialOutputEntry MaterialOutputEntries[] =
{
	{ MP_BaseColor,              TEXT("BaseColor") },
	{ MP_Metallic,               TEXT("Metallic") },
	{ MP_Specular,               TEXT("Specular") },
	{ MP_Roughness,              TEXT("Roughness") },
	{ MP_Anisotropy,             TEXT("Anisotropy") },
	{ MP_EmissiveColor,          TEXT("EmissiveColor") },
	{ MP_Opacity,                TEXT("Opacity") },
	{ MP_OpacityMask,            TEXT("OpacityMask") },
	{ MP_Normal,                 TEXT("Normal") },
	{ MP_WorldPositionOffset,    TEXT("WorldPositionOffset") },
	{ MP_SubsurfaceColor,        TEXT("SubsurfaceColor") },
	{ MP_AmbientOcclusion,       TEXT("AmbientOcclusion") },
	{ MP_Refraction,             TEXT("Refraction") },
	{ MP_PixelDepthOffset,       TEXT("PixelDepthOffset") },
	{ MP_ShadingModel,           TEXT("ShadingModel") },
	{ MP_Tangent,                TEXT("Tangent") },
	{ MP_Displacement,           TEXT("Displacement") },
	{ MP_CustomData0,            TEXT("ClearCoat") },
	{ MP_CustomData1,            TEXT("ClearCoatRoughness") },
	{ MP_SurfaceThickness,       TEXT("SurfaceThickness") },
	{ MP_FrontMaterial,          TEXT("FrontMaterial") },
	{ MP_MaterialAttributes,     TEXT("MaterialAttributes") },
};

// ============================================================================
// Action: get_all_expressions
// Params: { "asset_path": "/Game/..." }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::GetAllExpressions(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	TArray<TSharedPtr<FJsonValue>> ExpressionsArray;
	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Mat->GetExpressions();
	for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
	{
		if (Expr)
		{
			ExpressionsArray.Add(MakeShared<FJsonValueObject>(SerializeExpression(Expr)));
		}
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetNumberField(TEXT("expression_count"), ExpressionsArray.Num());
	ResultJson->SetArrayField(TEXT("expressions"), ExpressionsArray);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: get_expression_details
// Params: { "asset_path": "...", "expression_name": "..." }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::GetExpressionDetails(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ExpressionName = Params->GetStringField(TEXT("expression_name"));

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	UMaterialExpression* FoundExpr = nullptr;
	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Mat->GetExpressions();
	for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
	{
		if (Expr && Expr->GetName() == ExpressionName)
		{
			FoundExpr = Expr;
			break;
		}
	}

	if (!FoundExpr)
	{
		TArray<FString> AvailableNames;
		for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
		{
			if (Expr) AvailableNames.Add(Expr->GetName());
		}
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Expression '%s' not found. Available: %s"),
			*ExpressionName, *FString::Join(AvailableNames, TEXT(", "))));
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetStringField(TEXT("expression_name"), ExpressionName);
	ResultJson->SetStringField(TEXT("class"), FoundExpr->GetClass()->GetName());

	// Serialize ALL UProperties via reflection
	auto PropsJson = MakeShared<FJsonObject>();
	for (TFieldIterator<FProperty> PropIt(FoundExpr->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;
		if (!Prop || Prop->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
		{
			continue;
		}
		FString ValueStr;
		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(FoundExpr);
		Prop->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, nullptr, PPF_None);
		if (!ValueStr.IsEmpty())
		{
			PropsJson->SetStringField(Prop->GetName(), ValueStr);
		}
	}
	ResultJson->SetObjectField(TEXT("properties"), PropsJson);

	// List input pins
	TArray<TSharedPtr<FJsonValue>> InputsArray;
	for (int32 i = 0; ; ++i)
	{
		FExpressionInput* Input = FoundExpr->GetInput(i);
		if (!Input)
		{
			break;
		}
		auto InputJson = MakeShared<FJsonObject>();
		InputJson->SetStringField(TEXT("name"), FoundExpr->GetInputName(i).ToString());
		InputJson->SetBoolField(TEXT("connected"), Input->Expression != nullptr);
		if (Input->Expression)
		{
			InputJson->SetStringField(TEXT("connected_to"), Input->Expression->GetName());
			InputJson->SetNumberField(TEXT("output_index"), Input->OutputIndex);
		}
		InputsArray.Add(MakeShared<FJsonValueObject>(InputJson));
	}
	ResultJson->SetArrayField(TEXT("inputs"), InputsArray);

	// List output pins
	TArray<TSharedPtr<FJsonValue>> OutputsArray;
	const TArray<FExpressionOutput>& Outputs = FoundExpr->Outputs;
	for (int32 i = 0; i < Outputs.Num(); ++i)
	{
		auto OutputJson = MakeShared<FJsonObject>();
		OutputJson->SetStringField(TEXT("name"), Outputs[i].OutputName.ToString());
		OutputJson->SetNumberField(TEXT("index"), i);
		OutputsArray.Add(MakeShared<FJsonValueObject>(OutputJson));
	}
	ResultJson->SetArrayField(TEXT("outputs"), OutputsArray);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: get_full_connection_graph
// Params: { "asset_path": "..." }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::GetFullConnectionGraph(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Mat->GetExpressions();

	for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
	{
		if (!Expr)
		{
			continue;
		}

		for (int32 i = 0; ; ++i)
		{
			FExpressionInput* Input = Expr->GetInput(i);
			if (!Input)
			{
				break;
			}
			if (!Input->Expression)
			{
				continue;
			}

			auto ConnJson = MakeShared<FJsonObject>();
			ConnJson->SetStringField(TEXT("from"), Input->Expression->GetName());
			ConnJson->SetNumberField(TEXT("from_output_index"), Input->OutputIndex);

			FString FromOutputName;
			const TArray<FExpressionOutput>& SourceOutputs = Input->Expression->Outputs;
			if (SourceOutputs.IsValidIndex(Input->OutputIndex))
			{
				FromOutputName = SourceOutputs[Input->OutputIndex].OutputName.ToString();
			}
			ConnJson->SetStringField(TEXT("from_output"), FromOutputName);
			ConnJson->SetStringField(TEXT("to"), Expr->GetName());
			ConnJson->SetStringField(TEXT("to_input"), Expr->GetInputName(i).ToString());

			ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnJson));
		}
	}

	// Material output connections
	TArray<TSharedPtr<FJsonValue>> MaterialOutputsArray;
	for (const FMaterialOutputEntry& Entry : MaterialOutputEntries)
	{
		FExpressionInput* Input = Mat->GetExpressionInputForProperty(Entry.Property);
		if (Input && Input->Expression)
		{
			auto OutJson = MakeShared<FJsonObject>();
			OutJson->SetStringField(TEXT("property"), Entry.Name);
			OutJson->SetStringField(TEXT("expression"), Input->Expression->GetName());
			OutJson->SetNumberField(TEXT("output_index"), Input->OutputIndex);
			MaterialOutputsArray.Add(MakeShared<FJsonValueObject>(OutJson));
		}
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetArrayField(TEXT("connections"), ConnectionsArray);
	ResultJson->SetArrayField(TEXT("material_outputs"), MaterialOutputsArray);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: disconnect_expression
// Params: { "asset_path": "...", "expression_name": "...", "input_name": "", "disconnect_outputs": false, "target_expression": "", "output_index": -1 }
// When disconnect_outputs=true: disconnects other expressions/material outputs that reference expression_name
//   target_expression: filter to only disconnect from this downstream expression or material property name (e.g. "BaseColor")
//   output_index: filter to only disconnect connections using this output index
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::DisconnectExpression(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ExpressionName = Params->GetStringField(TEXT("expression_name"));
	FString InputName = Params->HasField(TEXT("input_name")) ? Params->GetStringField(TEXT("input_name")) : TEXT("");
	bool bDisconnectOutputs = Params->HasField(TEXT("disconnect_outputs")) ? Params->GetBoolField(TEXT("disconnect_outputs")) : false;
	// Optional filter: only disconnect from a specific downstream expression (when disconnect_outputs=true)
	FString TargetDownstream = Params->HasField(TEXT("target_expression")) ? Params->GetStringField(TEXT("target_expression")) : TEXT("");
	// Optional filter: only disconnect a specific output index
	int32 TargetOutputIndex = Params->HasField(TEXT("output_index")) ? static_cast<int32>(Params->GetNumberField(TEXT("output_index"))) : -1;

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	UMaterialExpression* TargetExpr = nullptr;
	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Mat->GetExpressions();
	for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
	{
		if (Expr && Expr->GetName() == ExpressionName)
		{
			TargetExpr = Expr;
			break;
		}
	}

	if (!TargetExpr)
	{
		TArray<FString> AvailableNames;
		for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
		{
			if (Expr) AvailableNames.Add(Expr->GetName());
		}
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Expression '%s' not found. Available: %s"),
			*ExpressionName, *FString::Join(AvailableNames, TEXT(", "))));
	}

	GEditor->BeginTransaction(NSLOCTEXT("Monolith", "DisconnectExpr", "Disconnect Expression"));
	Mat->Modify();

	TArray<TSharedPtr<FJsonValue>> DisconnectedArray;
	int32 DisconnectCount = 0;

	if (!bDisconnectOutputs)
	{
		for (int32 i = 0; ; ++i)
		{
			FExpressionInput* Input = TargetExpr->GetInput(i);
			if (!Input)
			{
				break;
			}

			FString PinName = TargetExpr->GetInputName(i).ToString();
			if (InputName.IsEmpty() || PinName == InputName)
			{
				if (Input->Expression != nullptr)
				{
					auto DisconnJson = MakeShared<FJsonObject>();
					DisconnJson->SetStringField(TEXT("pin"), PinName);
					DisconnJson->SetStringField(TEXT("was_connected_to"), Input->Expression->GetName());
					DisconnectedArray.Add(MakeShared<FJsonValueObject>(DisconnJson));

					Input->Expression = nullptr;
					Input->OutputIndex = 0;
					DisconnectCount++;
				}
			}
		}
	}
	else
	{
		for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
		{
			if (!Expr)
			{
				continue;
			}
			for (int32 i = 0; ; ++i)
			{
				FExpressionInput* Input = Expr->GetInput(i);
				if (!Input)
				{
					break;
				}
				if (Input->Expression == TargetExpr)
				{
					// Filter by target downstream expression name if specified
					if (!TargetDownstream.IsEmpty() && Expr->GetName() != TargetDownstream) continue;
					// Filter by output index if specified
					if (TargetOutputIndex >= 0 && Input->OutputIndex != TargetOutputIndex) continue;

					auto DisconnJson = MakeShared<FJsonObject>();
					DisconnJson->SetStringField(TEXT("expression"), Expr->GetName());
					DisconnJson->SetStringField(TEXT("pin"), Expr->GetInputName(i).ToString());
					DisconnJson->SetNumberField(TEXT("output_index"), Input->OutputIndex);
					DisconnectedArray.Add(MakeShared<FJsonValueObject>(DisconnJson));

					Input->Expression = nullptr;
					Input->OutputIndex = 0;
					DisconnectCount++;
				}
			}
		}

		// Also check material output properties (expression -> material pin connections)
		for (const FMaterialOutputEntry& Entry : MaterialOutputEntries)
		{
			FExpressionInput* MatInput = Mat->GetExpressionInputForProperty(Entry.Property);
			if (!MatInput || MatInput->Expression != TargetExpr) continue;

			// Filter by target downstream: match against material property name (e.g. "BaseColor", "EmissiveColor")
			if (!TargetDownstream.IsEmpty() && TargetDownstream != Entry.Name) continue;
			// Filter by output index if specified
			if (TargetOutputIndex >= 0 && MatInput->OutputIndex != TargetOutputIndex) continue;

			auto DisconnJson = MakeShared<FJsonObject>();
			DisconnJson->SetStringField(TEXT("material_property"), Entry.Name);
			DisconnJson->SetNumberField(TEXT("output_index"), MatInput->OutputIndex);
			DisconnectedArray.Add(MakeShared<FJsonValueObject>(DisconnJson));

			MatInput->Expression = nullptr;
			MatInput->OutputIndex = 0;
			DisconnectCount++;
		}
	}

	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	GEditor->EndTransaction();

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetArrayField(TEXT("disconnected"), DisconnectedArray);
	ResultJson->SetNumberField(TEXT("count"), DisconnectCount);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: begin_transaction
// Params: { "transaction_name": "..." }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::BeginTransaction(const TSharedPtr<FJsonObject>& Params)
{
	FString TransactionName = Params->GetStringField(TEXT("transaction_name"));
	GEditor->BeginTransaction(FText::FromString(TransactionName));

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("transaction"), TransactionName);
	ResultJson->SetStringField(TEXT("status"), TEXT("begun"));
	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: end_transaction
// Params: {}
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::EndTransaction(const TSharedPtr<FJsonObject>& Params)
{
	GEditor->EndTransaction();

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("status"), TEXT("ended"));
	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: build_material_graph
// Params: { "asset_path": "...", "graph_spec": { nodes, custom_hlsl_nodes, connections, outputs }, "clear_existing": false }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::BuildMaterialGraph(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	bool bClearExisting = Params->HasField(TEXT("clear_existing")) ? Params->GetBoolField(TEXT("clear_existing")) : false;

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	// graph_spec can be passed as a nested object or as a JSON string
	TSharedPtr<FJsonObject> Spec;
	if (Params->HasTypedField<EJson::Object>(TEXT("graph_spec")))
	{
		Spec = Params->GetObjectField(TEXT("graph_spec"));
	}
	else if (Params->HasTypedField<EJson::String>(TEXT("graph_spec")))
	{
		FString GraphSpecJson = Params->GetStringField(TEXT("graph_spec"));
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(GraphSpecJson);
		if (!FJsonSerializer::Deserialize(Reader, Spec) || !Spec.IsValid())
		{
			return FMonolithActionResult::Error(TEXT("Failed to parse graph_spec JSON string"));
		}
	}
	else
	{
		return FMonolithActionResult::Error(TEXT("Missing 'graph_spec' parameter"));
	}

	TArray<TSharedPtr<FJsonValue>> ErrorsArray;
	int32 NodesCreated = 0;
	int32 ConnectionsMade = 0;

	GEditor->BeginTransaction(FText::FromString(TEXT("BuildMaterialGraph")));
	Mat->Modify();

	// Bug fix: save material-level properties before clear — DeleteAllMaterialExpressions
	// triggers PostEditChange which can reset BlendMode and other scalar properties
	const EBlendMode SavedBlendMode = Mat->BlendMode;
	const EMaterialShadingModel SavedShadingModel = Mat->GetShadingModels().GetFirstShadingModel();
	const bool bSavedTwoSided = Mat->TwoSided;
	const float SavedOpacityMaskClipValue = Mat->OpacityMaskClipValue;

	if (bClearExisting)
	{
		// Bug fix: DeleteAllMaterialExpressions uses a range-for over TConstArrayView
		// while Remove() mutates the backing TArray — iterator invalidation skips ~half.
		// Copy to a local array first.
		TArray<UMaterialExpression*> ToDelete;
		for (const TObjectPtr<UMaterialExpression>& Expr : Mat->GetExpressions())
		{
			if (Expr)
			{
				ToDelete.Add(Expr);
			}
		}
		for (UMaterialExpression* Expr : ToDelete)
		{
			UMaterialEditingLibrary::DeleteMaterialExpression(Mat, Expr);
		}
	}

	// Restore material-level properties that may have been reset by PostEditChange
	Mat->BlendMode = SavedBlendMode;
	Mat->SetShadingModel(SavedShadingModel);
	Mat->TwoSided = bSavedTwoSided;
	Mat->OpacityMaskClipValue = SavedOpacityMaskClipValue;

	TMap<FString, UMaterialExpression*> IdToExpr;

	// Bug fix: seed remap table with pre-existing expressions so connections
	// can reference nodes that survived a partial clear or were already present
	for (const TObjectPtr<UMaterialExpression>& Expr : Mat->GetExpressions())
	{
		if (Expr)
		{
			IdToExpr.Add(Expr->GetName(), Expr.Get());
		}
	}

	// Phase 1 — Standard nodes
	const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
	if (Spec->TryGetArrayField(TEXT("nodes"), NodesArray))
	{
		for (const TSharedPtr<FJsonValue>& NodeVal : *NodesArray)
		{
			const TSharedPtr<FJsonObject>* NodeObjPtr = nullptr;
			if (!NodeVal || !NodeVal->TryGetObject(NodeObjPtr) || !NodeObjPtr)
			{
				continue;
			}
			const TSharedPtr<FJsonObject>& NodeObj = *NodeObjPtr;

			FString Id = NodeObj->GetStringField(TEXT("id"));
			FString ShortClass = NodeObj->GetStringField(TEXT("class"));

			FString FullClassName = ShortClass;
			if (!ShortClass.StartsWith(TEXT("MaterialExpression")))
			{
				FullClassName = FString::Printf(TEXT("MaterialExpression%s"), *ShortClass);
			}

			// Try multiple lookup strategies for the expression class
			UClass* ExprClass = FindFirstObject<UClass>(*FullClassName, EFindFirstObjectOptions::NativeFirst);
			if (!ExprClass)
			{
				// Try with U prefix (UMaterialExpressionConstant)
				FString UClassName = FString::Printf(TEXT("U%s"), *FullClassName);
				ExprClass = FindFirstObject<UClass>(*UClassName, EFindFirstObjectOptions::NativeFirst);
			}
			if (!ExprClass)
			{
				auto ErrJson = MakeShared<FJsonObject>();
				ErrJson->SetStringField(TEXT("node_id"), Id);
				ErrJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Class '%s' not found"), *FullClassName));
				ErrorsArray.Add(MakeShared<FJsonValueObject>(ErrJson));
				continue;
			}

			int32 PosX = 0, PosY = 0;
			const TArray<TSharedPtr<FJsonValue>>* PosArray = nullptr;
			if (NodeObj->TryGetArrayField(TEXT("pos"), PosArray) && PosArray->Num() >= 2)
			{
				PosX = static_cast<int32>((*PosArray)[0]->AsNumber());
				PosY = static_cast<int32>((*PosArray)[1]->AsNumber());
			}

			UMaterialExpression* NewExpr = UMaterialEditingLibrary::CreateMaterialExpression(Mat, ExprClass, PosX, PosY);
			if (!NewExpr)
			{
				auto ErrJson = MakeShared<FJsonObject>();
				ErrJson->SetStringField(TEXT("node_id"), Id);
				ErrJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to create expression of class '%s'"), *FullClassName));
				ErrorsArray.Add(MakeShared<FJsonValueObject>(ErrJson));
				continue;
			}

			// Set properties
			const TSharedPtr<FJsonObject>* PropsObjPtr = nullptr;
			if (NodeObj->TryGetObjectField(TEXT("props"), PropsObjPtr) && PropsObjPtr)
			{
				const TSharedPtr<FJsonObject>& PropsObj = *PropsObjPtr;
				for (const auto& Pair : PropsObj->Values)
				{
					FProperty* Prop = NewExpr->GetClass()->FindPropertyByName(*Pair.Key);
					if (!Prop)
					{
						auto ErrJson = MakeShared<FJsonObject>();
						ErrJson->SetStringField(TEXT("node_id"), Id);
						ErrJson->SetStringField(TEXT("warning"), FString::Printf(TEXT("Property '%s' not found on '%s'"), *Pair.Key, *FullClassName));
						ErrorsArray.Add(MakeShared<FJsonValueObject>(ErrJson));
						continue;
					}

					FString ValueStr = Pair.Value->AsString();
					void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(NewExpr);

					if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
					{
						FloatProp->SetPropertyValue(ValuePtr, static_cast<float>(Pair.Value->AsNumber()));
					}
					else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
					{
						DoubleProp->SetPropertyValue(ValuePtr, Pair.Value->AsNumber());
					}
					else if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
					{
						IntProp->SetPropertyValue(ValuePtr, static_cast<int32>(Pair.Value->AsNumber()));
					}
					else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
					{
						BoolProp->SetPropertyValue(ValuePtr, Pair.Value->AsBool());
					}
					else
					{
						Prop->ImportText_Direct(*ValueStr, ValuePtr, NewExpr, PPF_None);
					}
				}
			}

			IdToExpr.Add(Id, NewExpr);
			NodesCreated++;
		}
	}

	// Phase 2 — Custom HLSL nodes
	const TArray<TSharedPtr<FJsonValue>>* CustomArray = nullptr;
	if (Spec->TryGetArrayField(TEXT("custom_hlsl_nodes"), CustomArray))
	{
		for (const TSharedPtr<FJsonValue>& CustomVal : *CustomArray)
		{
			const TSharedPtr<FJsonObject>* CustomObjPtr = nullptr;
			if (!CustomVal || !CustomVal->TryGetObject(CustomObjPtr) || !CustomObjPtr)
			{
				continue;
			}
			const TSharedPtr<FJsonObject>& CustomObj = *CustomObjPtr;

			FString Id = CustomObj->GetStringField(TEXT("id"));

			int32 PosX = 0, PosY = 0;
			const TArray<TSharedPtr<FJsonValue>>* PosArray = nullptr;
			if (CustomObj->TryGetArrayField(TEXT("pos"), PosArray) && PosArray->Num() >= 2)
			{
				PosX = static_cast<int32>((*PosArray)[0]->AsNumber());
				PosY = static_cast<int32>((*PosArray)[1]->AsNumber());
			}

			UMaterialExpression* BaseExpr = UMaterialEditingLibrary::CreateMaterialExpression(
				Mat, UMaterialExpressionCustom::StaticClass(), PosX, PosY);

			UMaterialExpressionCustom* CustomExpr = Cast<UMaterialExpressionCustom>(BaseExpr);
			if (!CustomExpr)
			{
				auto ErrJson = MakeShared<FJsonObject>();
				ErrJson->SetStringField(TEXT("node_id"), Id);
				ErrJson->SetStringField(TEXT("error"), TEXT("Failed to create Custom HLSL expression"));
				ErrorsArray.Add(MakeShared<FJsonValueObject>(ErrJson));
				continue;
			}

			CustomExpr->Code = CustomObj->GetStringField(TEXT("code"));
			// Bug fix: when graph_spec arrives as a pre-serialized JSON string, newlines
			// get double-encoded as literal \\n text. Unescape them.
			CustomExpr->Code.ReplaceInline(TEXT("\\n"), TEXT("\n"), ESearchCase::CaseSensitive);
			CustomExpr->Code.ReplaceInline(TEXT("\\t"), TEXT("\t"), ESearchCase::CaseSensitive);
			if (CustomObj->HasField(TEXT("description")))
			{
				CustomExpr->Description = CustomObj->GetStringField(TEXT("description"));
			}
			if (CustomObj->HasField(TEXT("output_type")))
			{
				CustomExpr->OutputType = ParseCustomOutputType(CustomObj->GetStringField(TEXT("output_type")));
			}

			const TArray<TSharedPtr<FJsonValue>>* InputsArray = nullptr;
			if (CustomObj->TryGetArrayField(TEXT("inputs"), InputsArray))
			{
				CustomExpr->Inputs.Empty();
				for (const TSharedPtr<FJsonValue>& InputVal : *InputsArray)
				{
					const TSharedPtr<FJsonObject>* InputObjPtr = nullptr;
					if (InputVal && InputVal->TryGetObject(InputObjPtr) && InputObjPtr)
					{
						FCustomInput NewInput;
						NewInput.InputName = *(*InputObjPtr)->GetStringField(TEXT("name"));
						CustomExpr->Inputs.Add(NewInput);
					}
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* AddOutputsArray = nullptr;
			if (CustomObj->TryGetArrayField(TEXT("additional_outputs"), AddOutputsArray))
			{
				CustomExpr->AdditionalOutputs.Empty();
				for (const TSharedPtr<FJsonValue>& OutVal : *AddOutputsArray)
				{
					const TSharedPtr<FJsonObject>* OutObjPtr = nullptr;
					if (OutVal && OutVal->TryGetObject(OutObjPtr) && OutObjPtr)
					{
						FCustomOutput NewOutput;
						NewOutput.OutputName = *(*OutObjPtr)->GetStringField(TEXT("name"));
						if ((*OutObjPtr)->HasField(TEXT("type")))
						{
							NewOutput.OutputType = ParseCustomOutputType((*OutObjPtr)->GetStringField(TEXT("type")));
						}
						CustomExpr->AdditionalOutputs.Add(NewOutput);
					}
				}
			}

			CustomExpr->RebuildOutputs();
			IdToExpr.Add(Id, CustomExpr);
			NodesCreated++;
		}
	}

	// Phase 3 — Wire connections between expressions
	const TArray<TSharedPtr<FJsonValue>>* ConnsArray = nullptr;
	if (Spec->TryGetArrayField(TEXT("connections"), ConnsArray))
	{
		for (const TSharedPtr<FJsonValue>& ConnVal : *ConnsArray)
		{
			const TSharedPtr<FJsonObject>* ConnObjPtr = nullptr;
			if (!ConnVal || !ConnVal->TryGetObject(ConnObjPtr) || !ConnObjPtr)
			{
				continue;
			}
			const TSharedPtr<FJsonObject>& ConnObj = *ConnObjPtr;

			FString FromId = ConnObj->GetStringField(TEXT("from"));
			FString ToId = ConnObj->GetStringField(TEXT("to"));
			FString FromPin = ConnObj->HasField(TEXT("from_pin")) ? ConnObj->GetStringField(TEXT("from_pin")) : TEXT("");
			FString ToPin = ConnObj->HasField(TEXT("to_pin")) ? ConnObj->GetStringField(TEXT("to_pin")) : TEXT("");

			UMaterialExpression** FromPtr = IdToExpr.Find(FromId);
			UMaterialExpression** ToPtr = IdToExpr.Find(ToId);

			if (!FromPtr || !*FromPtr)
			{
				auto ErrJson = MakeShared<FJsonObject>();
				ErrJson->SetStringField(TEXT("connection"), FString::Printf(TEXT("%s -> %s"), *FromId, *ToId));
				ErrJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Source node '%s' not found"), *FromId));
				ErrorsArray.Add(MakeShared<FJsonValueObject>(ErrJson));
				continue;
			}
			if (!ToPtr || !*ToPtr)
			{
				auto ErrJson = MakeShared<FJsonObject>();
				ErrJson->SetStringField(TEXT("connection"), FString::Printf(TEXT("%s -> %s"), *FromId, *ToId));
				ErrJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Target node '%s' not found"), *ToId));
				ErrorsArray.Add(MakeShared<FJsonValueObject>(ErrJson));
				continue;
			}

			bool bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(*FromPtr, FromPin, *ToPtr, ToPin);
			if (bConnected)
			{
				ConnectionsMade++;
			}
			else
			{
				auto ErrJson = MakeShared<FJsonObject>();
				ErrJson->SetStringField(TEXT("connection"), FString::Printf(TEXT("%s.%s -> %s.%s"), *FromId, *FromPin, *ToId, *ToPin));
				ErrJson->SetStringField(TEXT("error"), TEXT("ConnectMaterialExpressions returned false"));
				ErrorsArray.Add(MakeShared<FJsonValueObject>(ErrJson));
			}
		}
	}

	// Phase 4 — Wire material output properties
	const TArray<TSharedPtr<FJsonValue>>* OutputsArray = nullptr;
	if (Spec->TryGetArrayField(TEXT("outputs"), OutputsArray))
	{
		for (const TSharedPtr<FJsonValue>& OutVal : *OutputsArray)
		{
			const TSharedPtr<FJsonObject>* OutObjPtr = nullptr;
			if (!OutVal || !OutVal->TryGetObject(OutObjPtr) || !OutObjPtr)
			{
				continue;
			}
			const TSharedPtr<FJsonObject>& OutObj = *OutObjPtr;

			FString FromId = OutObj->GetStringField(TEXT("from"));
			FString FromPin = OutObj->HasField(TEXT("from_pin")) ? OutObj->GetStringField(TEXT("from_pin")) : TEXT("");
			FString ToProp = OutObj->GetStringField(TEXT("to_property"));

			UMaterialExpression** FromPtr = IdToExpr.Find(FromId);
			if (!FromPtr || !*FromPtr)
			{
				auto ErrJson = MakeShared<FJsonObject>();
				ErrJson->SetStringField(TEXT("output"), ToProp);
				ErrJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Source node '%s' not found"), *FromId));
				ErrorsArray.Add(MakeShared<FJsonValueObject>(ErrJson));
				continue;
			}

			EMaterialProperty MatProp = ParseMaterialProperty(ToProp);
			if (MatProp == MP_MAX)
			{
				auto ErrJson = MakeShared<FJsonObject>();
				ErrJson->SetStringField(TEXT("output"), ToProp);
				ErrJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown material property '%s'"), *ToProp));
				ErrorsArray.Add(MakeShared<FJsonValueObject>(ErrJson));
				continue;
			}

			// Blend mode validation warnings
			{
				EBlendMode BM = Mat->BlendMode;
				if (MatProp == MP_Opacity && (BM == BLEND_Opaque || BM == BLEND_Masked))
				{
					auto WJ = MakeShared<FJsonObject>();
					WJ->SetStringField(TEXT("output"), ToProp);
					WJ->SetStringField(TEXT("warning"), TEXT("Opacity has no effect on Opaque/Masked materials"));
					ErrorsArray.Add(MakeShared<FJsonValueObject>(WJ));
				}
				if (MatProp == MP_OpacityMask && BM != BLEND_Masked)
				{
					auto WJ = MakeShared<FJsonObject>();
					WJ->SetStringField(TEXT("output"), ToProp);
					WJ->SetStringField(TEXT("warning"), TEXT("OpacityMask only affects Masked blend mode"));
					ErrorsArray.Add(MakeShared<FJsonValueObject>(WJ));
				}
			}

			bool bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(*FromPtr, FromPin, MatProp);
			if (bConnected)
			{
				ConnectionsMade++;
			}
			else
			{
				auto ErrJson = MakeShared<FJsonObject>();
				ErrJson->SetStringField(TEXT("output"), ToProp);
				ErrJson->SetStringField(TEXT("error"), TEXT("ConnectMaterialProperty returned false"));
				ErrorsArray.Add(MakeShared<FJsonValueObject>(ErrJson));
			}
		}
	}

	// Auto-recompile so the material is immediately usable
	UMaterialEditingLibrary::RecompileMaterial(Mat);

	GEditor->EndTransaction();

	auto ResultJson = MakeShared<FJsonObject>();
	// Separate actual errors from warnings (blend mode warnings are not errors)
	int32 ErrorCount = 0, WarningCount = 0;
	for (const TSharedPtr<FJsonValue>& Entry : ErrorsArray)
	{
		if (Entry->AsObject()->HasField(TEXT("warning"))) WarningCount++;
		else ErrorCount++;
	}
	ResultJson->SetBoolField(TEXT("has_errors"), ErrorCount > 0);
	ResultJson->SetBoolField(TEXT("has_warnings"), WarningCount > 0);
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetNumberField(TEXT("nodes_created"), NodesCreated);
	ResultJson->SetNumberField(TEXT("connections_made"), ConnectionsMade);
	ResultJson->SetBoolField(TEXT("recompiled"), true);

	auto IdMapJson = MakeShared<FJsonObject>();
	for (const auto& Pair : IdToExpr)
	{
		if (Pair.Value)
		{
			IdMapJson->SetStringField(Pair.Key, Pair.Value->GetName());
		}
	}
	ResultJson->SetObjectField(TEXT("id_to_name"), IdMapJson);
	ResultJson->SetArrayField(TEXT("errors"), ErrorsArray);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: export_material_graph
// Params: { "asset_path": "...", "include_properties": true, "include_positions": true }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::ExportMaterialGraph(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	bool bIncludeProperties = true;
	bool bIncludePositions = true;
	if (Params.IsValid())
	{
		Params->TryGetBoolField(TEXT("include_properties"), bIncludeProperties);
		Params->TryGetBoolField(TEXT("include_positions"), bIncludePositions);
	}

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Mat->GetExpressions();

	TMap<UMaterialExpression*, FString> ExprToId;
	for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
	{
		if (Expr)
		{
			ExprToId.Add(Expr, Expr->GetName());
		}
	}

	TArray<TSharedPtr<FJsonValue>> NodesArray;
	TArray<TSharedPtr<FJsonValue>> CustomHlslArray;

	for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
	{
		if (!Expr)
		{
			continue;
		}

		const UMaterialExpressionCustom* CustomExpr = Cast<UMaterialExpressionCustom>(Expr);

		if (CustomExpr)
		{
			auto NodeJson = MakeShared<FJsonObject>();
			NodeJson->SetStringField(TEXT("id"), Expr->GetName());

			if (bIncludePositions)
			{
				TArray<TSharedPtr<FJsonValue>> PosArr;
				PosArr.Add(MakeShared<FJsonValueNumber>(Expr->MaterialExpressionEditorX));
				PosArr.Add(MakeShared<FJsonValueNumber>(Expr->MaterialExpressionEditorY));
				NodeJson->SetArrayField(TEXT("pos"), PosArr);
			}

			NodeJson->SetStringField(TEXT("code"), CustomExpr->Code);
			NodeJson->SetStringField(TEXT("description"), CustomExpr->Description);
			NodeJson->SetStringField(TEXT("output_type"), CustomOutputTypeToString(CustomExpr->OutputType));

			TArray<TSharedPtr<FJsonValue>> InputsArr;
			for (const FCustomInput& CustInput : CustomExpr->Inputs)
			{
				auto InputJson = MakeShared<FJsonObject>();
				InputJson->SetStringField(TEXT("name"), CustInput.InputName.ToString());
				InputsArr.Add(MakeShared<FJsonValueObject>(InputJson));
			}
			NodeJson->SetArrayField(TEXT("inputs"), InputsArr);

			TArray<TSharedPtr<FJsonValue>> AddOutputsArr;
			for (const FCustomOutput& AddOut : CustomExpr->AdditionalOutputs)
			{
				auto OutJson = MakeShared<FJsonObject>();
				OutJson->SetStringField(TEXT("name"), AddOut.OutputName.ToString());
				OutJson->SetStringField(TEXT("type"), CustomOutputTypeToString(AddOut.OutputType));
				AddOutputsArr.Add(MakeShared<FJsonValueObject>(OutJson));
			}
			NodeJson->SetArrayField(TEXT("additional_outputs"), AddOutputsArr);

			CustomHlslArray.Add(MakeShared<FJsonValueObject>(NodeJson));
		}
		else
		{
			auto NodeJson = MakeShared<FJsonObject>();
			NodeJson->SetStringField(TEXT("id"), Expr->GetName());

			FString ClassName = Expr->GetClass()->GetName();
			if (ClassName.StartsWith(TEXT("MaterialExpression")))
			{
				ClassName = ClassName.Mid(18);
			}
			NodeJson->SetStringField(TEXT("class"), ClassName);

			if (bIncludePositions)
			{
				TArray<TSharedPtr<FJsonValue>> PosArr;
				PosArr.Add(MakeShared<FJsonValueNumber>(Expr->MaterialExpressionEditorX));
				PosArr.Add(MakeShared<FJsonValueNumber>(Expr->MaterialExpressionEditorY));
				NodeJson->SetArrayField(TEXT("pos"), PosArr);
			}

			if (bIncludeProperties)
			{
				auto PropsJson = MakeShared<FJsonObject>();
				for (TFieldIterator<FProperty> PropIt(Expr->GetClass()); PropIt; ++PropIt)
				{
					FProperty* Prop = *PropIt;
					if (!Prop || Prop->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
					{
						continue;
					}
					if (Prop->GetOwnerClass() == UMaterialExpression::StaticClass())
					{
						continue;
					}
					FString ValueStr;
					const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Expr);
					Prop->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, nullptr, PPF_None);
					if (!ValueStr.IsEmpty())
					{
						PropsJson->SetStringField(Prop->GetName(), ValueStr);
					}
				}
				NodeJson->SetObjectField(TEXT("props"), PropsJson);
			}

			NodesArray.Add(MakeShared<FJsonValueObject>(NodeJson));
		}
	}

	// Build connections
	TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
	for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
	{
		if (!Expr)
		{
			continue;
		}
		for (int32 i = 0; ; ++i)
		{
			FExpressionInput* ExprInput = Expr->GetInput(i);
			if (!ExprInput)
			{
				break;
			}
			if (!ExprInput->Expression)
			{
				continue;
			}

			auto ConnJson = MakeShared<FJsonObject>();
			FString* FromId = ExprToId.Find(ExprInput->Expression);
			ConnJson->SetStringField(TEXT("from"), FromId ? *FromId : ExprInput->Expression->GetName());

			FString FromPin;
			const TArray<FExpressionOutput>& SourceOutputs = ExprInput->Expression->Outputs;
			if (SourceOutputs.IsValidIndex(ExprInput->OutputIndex))
			{
				FromPin = SourceOutputs[ExprInput->OutputIndex].OutputName.ToString();
			}
			ConnJson->SetStringField(TEXT("from_pin"), FromPin);
			ConnJson->SetStringField(TEXT("to"), Expr->GetName());
			ConnJson->SetStringField(TEXT("to_pin"), Expr->GetInputName(i).ToString());

			ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnJson));
		}
	}

	// Build outputs
	TArray<TSharedPtr<FJsonValue>> OutputsArray;
	for (EMaterialProperty MatProp : AllMaterialProperties)
	{
		FExpressionInput* PropInput = Mat->GetExpressionInputForProperty(MatProp);
		if (PropInput && PropInput->Expression)
		{
			auto OutJson = MakeShared<FJsonObject>();
			FString* FromId = ExprToId.Find(PropInput->Expression);
			OutJson->SetStringField(TEXT("from"), FromId ? *FromId : PropInput->Expression->GetName());

			FString FromPin;
			const TArray<FExpressionOutput>& SourceOutputs = PropInput->Expression->Outputs;
			if (SourceOutputs.IsValidIndex(PropInput->OutputIndex))
			{
				FromPin = SourceOutputs[PropInput->OutputIndex].OutputName.ToString();
			}
			OutJson->SetStringField(TEXT("from_pin"), FromPin);
			OutJson->SetStringField(TEXT("to_property"), MaterialPropertyToString(MatProp));
			OutputsArray.Add(MakeShared<FJsonValueObject>(OutJson));
		}
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetArrayField(TEXT("nodes"), NodesArray);
	ResultJson->SetArrayField(TEXT("custom_hlsl_nodes"), CustomHlslArray);
	ResultJson->SetArrayField(TEXT("connections"), ConnectionsArray);
	ResultJson->SetArrayField(TEXT("outputs"), OutputsArray);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: import_material_graph
// Params: { "asset_path": "...", "graph_json": "...", "mode": "overwrite"|"merge" }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::ImportMaterialGraph(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString GraphJson = Params->GetStringField(TEXT("graph_json"));
	FString Mode = Params->HasField(TEXT("mode")) ? Params->GetStringField(TEXT("mode")) : TEXT("overwrite");

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	if (Mode == TEXT("overwrite"))
	{
		// Build a params object for BuildMaterialGraph with clear_existing=true
		auto BuildParams = MakeShared<FJsonObject>();
		BuildParams->SetStringField(TEXT("asset_path"), AssetPath);
		BuildParams->SetStringField(TEXT("graph_spec"), GraphJson);
		BuildParams->SetBoolField(TEXT("clear_existing"), true);
		return BuildMaterialGraph(BuildParams);
	}
	else if (Mode == TEXT("merge"))
	{
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(GraphJson);
		TSharedPtr<FJsonObject> Spec;
		if (!FJsonSerializer::Deserialize(JsonReader, Spec) || !Spec.IsValid())
		{
			return FMonolithActionResult::Error(TEXT("Failed to parse graph_json for merge"));
		}

		auto OffsetNodePositions = [](const TArray<TSharedPtr<FJsonValue>>* ArrayPtr)
		{
			if (!ArrayPtr) return;
			for (const TSharedPtr<FJsonValue>& NodeVal : *ArrayPtr)
			{
				const TSharedPtr<FJsonObject>* NodeObjPtr = nullptr;
				if (NodeVal && NodeVal->TryGetObject(NodeObjPtr) && NodeObjPtr)
				{
					const TArray<TSharedPtr<FJsonValue>>* PosArray = nullptr;
					if ((*NodeObjPtr)->TryGetArrayField(TEXT("pos"), PosArray) && PosArray->Num() >= 2)
					{
						double OrigX = (*PosArray)[0]->AsNumber();
						double OrigY = (*PosArray)[1]->AsNumber();
						TArray<TSharedPtr<FJsonValue>> NewPos;
						NewPos.Add(MakeShared<FJsonValueNumber>(OrigX + 500.0));
						NewPos.Add(MakeShared<FJsonValueNumber>(OrigY));
						(*NodeObjPtr)->SetArrayField(TEXT("pos"), NewPos);
					}
				}
			}
		};

		const TArray<TSharedPtr<FJsonValue>>* NodesArr = nullptr;
		Spec->TryGetArrayField(TEXT("nodes"), NodesArr);
		OffsetNodePositions(NodesArr);

		const TArray<TSharedPtr<FJsonValue>>* CustomArr = nullptr;
		Spec->TryGetArrayField(TEXT("custom_hlsl_nodes"), CustomArr);
		OffsetNodePositions(CustomArr);

		auto BuildParams = MakeShared<FJsonObject>();
		BuildParams->SetStringField(TEXT("asset_path"), AssetPath);
		BuildParams->SetObjectField(TEXT("graph_spec"), Spec);
		BuildParams->SetBoolField(TEXT("clear_existing"), false);
		return BuildMaterialGraph(BuildParams);
	}
	else
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Unknown import mode '%s'. Use 'overwrite' or 'merge'."), *Mode));
	}
}

// ============================================================================
// Action: validate_material
// Params: { "asset_path": "...", "fix_issues": false }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::ValidateMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	bool bFixIssues = Params->HasField(TEXT("fix_issues")) ? Params->GetBoolField(TEXT("fix_issues")) : false;

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Mat->GetExpressions();
	TArray<TSharedPtr<FJsonValue>> IssuesArray;
	int32 FixedCount = 0;

	// BFS from material outputs to find reachable expressions
	TSet<UMaterialExpression*> ReachableSet;
	TArray<UMaterialExpression*> BfsQueue;

	for (EMaterialProperty Prop : AllMaterialProperties)
	{
		FExpressionInput* PropInput = Mat->GetExpressionInputForProperty(Prop);
		if (PropInput && PropInput->Expression)
		{
			if (!ReachableSet.Contains(PropInput->Expression))
			{
				ReachableSet.Add(PropInput->Expression);
				BfsQueue.Add(PropInput->Expression);
			}
		}
	}

	// Also seed from UMaterialExpressionCustomOutput subclasses — they are output terminals too
	for (UMaterialExpression* Expr : Expressions)
	{
		if (Expr && Expr->IsA<UMaterialExpressionCustomOutput>())
		{
			if (!ReachableSet.Contains(Expr))
			{
				ReachableSet.Add(Expr);
				BfsQueue.Add(Expr);
			}
		}
	}

	// Seed from UMaterialExpressionMaterialAttributeLayers — implicit output terminals for layer-blend materials
	for (UMaterialExpression* Expr : Expressions)
	{
		if (Expr && Expr->IsA<UMaterialExpressionMaterialAttributeLayers>())
		{
			if (!ReachableSet.Contains(Expr))
			{
				ReachableSet.Add(Expr);
				BfsQueue.Add(Expr);
			}
		}
	}

	while (BfsQueue.Num() > 0)
	{
		UMaterialExpression* Current = BfsQueue.Pop(EAllowShrinking::No);
		for (int32 i = 0; ; ++i)
		{
			FExpressionInput* NodeInput = Current->GetInput(i);
			if (!NodeInput)
			{
				break;
			}
			if (NodeInput->Expression && !ReachableSet.Contains(NodeInput->Expression))
			{
				ReachableSet.Add(NodeInput->Expression);
				BfsQueue.Add(NodeInput->Expression);
			}
		}
	}

	TMap<FString, TArray<UMaterialExpression*>> ParameterNames;
	TArray<UMaterialExpression*> IslandExprs;

	for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
	{
		if (!Expr)
		{
			continue;
		}

		bool bIsReachable = ReachableSet.Contains(Expr);

		// Check: Disconnected islands (skip comments)
		if (!bIsReachable && !Cast<UMaterialExpressionComment>(Expr))
		{
			auto IssueJson = MakeShared<FJsonObject>();
			IssueJson->SetStringField(TEXT("severity"), TEXT("warning"));
			IssueJson->SetStringField(TEXT("type"), TEXT("island"));
			IssueJson->SetStringField(TEXT("expression"), Expr->GetName());
			IssueJson->SetStringField(TEXT("class"), Expr->GetClass()->GetName());

			bool bFixed = false;
			if (bFixIssues)
			{
				IslandExprs.Add(Expr);
				bFixed = true;
				FixedCount++;
			}
			IssueJson->SetBoolField(TEXT("fixed"), bFixed);
			IssuesArray.Add(MakeShared<FJsonValueObject>(IssueJson));
		}

		// Check: Broken texture refs
		if (const auto* TexBase = Cast<UMaterialExpressionTextureBase>(Expr))
		{
			if (!TexBase->Texture)
			{
				auto IssueJson = MakeShared<FJsonObject>();
				IssueJson->SetStringField(TEXT("severity"), TEXT("error"));
				IssueJson->SetStringField(TEXT("type"), TEXT("broken_texture_ref"));
				IssueJson->SetStringField(TEXT("expression"), Expr->GetName());
				IssueJson->SetBoolField(TEXT("fixed"), false);
				IssuesArray.Add(MakeShared<FJsonValueObject>(IssueJson));
			}
		}

		// Check: Missing material functions
		if (const auto* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expr))
		{
			if (!FuncCall->MaterialFunction)
			{
				auto IssueJson = MakeShared<FJsonObject>();
				IssueJson->SetStringField(TEXT("severity"), TEXT("error"));
				IssueJson->SetStringField(TEXT("type"), TEXT("missing_material_function"));
				IssueJson->SetStringField(TEXT("expression"), Expr->GetName());
				IssueJson->SetBoolField(TEXT("fixed"), false);
				IssuesArray.Add(MakeShared<FJsonValueObject>(IssueJson));
			}
		}

		// Collect parameter names for duplicate detection
		if (const auto* Param = Cast<UMaterialExpressionParameter>(Expr))
		{
			ParameterNames.FindOrAdd(Param->ParameterName.ToString()).Add(Expr);
		}
		else if (const auto* TexParam = Cast<UMaterialExpressionTextureSampleParameter>(Expr))
		{
			ParameterNames.FindOrAdd(TexParam->ParameterName.ToString()).Add(Expr);
		}

		// Check: Unused parameters
		if (!bIsReachable)
		{
			bool bIsParam = Cast<UMaterialExpressionParameter>(Expr) != nullptr
				|| Cast<UMaterialExpressionTextureSampleParameter>(Expr) != nullptr;
			if (bIsParam)
			{
				auto IssueJson = MakeShared<FJsonObject>();
				IssueJson->SetStringField(TEXT("severity"), TEXT("warning"));
				IssueJson->SetStringField(TEXT("type"), TEXT("unused_parameter"));
				IssueJson->SetStringField(TEXT("expression"), Expr->GetName());
				IssueJson->SetBoolField(TEXT("fixed"), false);
				IssuesArray.Add(MakeShared<FJsonValueObject>(IssueJson));
			}
		}
	}

	// Check: Duplicate parameter names
	for (const auto& Pair : ParameterNames)
	{
		if (Pair.Value.Num() > 1)
		{
			auto IssueJson = MakeShared<FJsonObject>();
			IssueJson->SetStringField(TEXT("severity"), TEXT("warning"));
			IssueJson->SetStringField(TEXT("type"), TEXT("duplicate_parameter_name"));
			IssueJson->SetStringField(TEXT("parameter_name"), Pair.Key);
			IssueJson->SetNumberField(TEXT("count"), Pair.Value.Num());

			TArray<TSharedPtr<FJsonValue>> DupExprs;
			for (UMaterialExpression* DupExpr : Pair.Value)
			{
				DupExprs.Add(MakeShared<FJsonValueString>(DupExpr->GetName()));
			}
			IssueJson->SetArrayField(TEXT("expressions"), DupExprs);
			IssueJson->SetBoolField(TEXT("fixed"), false);
			IssuesArray.Add(MakeShared<FJsonValueObject>(IssueJson));
		}
	}

	// Check: Expression count warning
	if (Expressions.Num() > 200)
	{
		auto IssueJson = MakeShared<FJsonObject>();
		IssueJson->SetStringField(TEXT("severity"), TEXT("info"));
		IssueJson->SetStringField(TEXT("type"), TEXT("high_expression_count"));
		IssueJson->SetNumberField(TEXT("count"), Expressions.Num());
		IssueJson->SetBoolField(TEXT("fixed"), false);
		IssuesArray.Add(MakeShared<FJsonValueObject>(IssueJson));
	}

	// Check: Blend mode / shading model vs connected outputs
	{
		auto AddBlendWarning = [&](const FString& Type, const FString& Message)
		{
			auto IssueJson = MakeShared<FJsonObject>();
			IssueJson->SetStringField(TEXT("severity"), TEXT("warning"));
			IssueJson->SetStringField(TEXT("type"), Type);
			IssueJson->SetStringField(TEXT("message"), Message);
			IssueJson->SetBoolField(TEXT("fixed"), false);
			IssuesArray.Add(MakeShared<FJsonValueObject>(IssueJson));
		};

		EBlendMode BM = Mat->BlendMode;
		FExpressionInput* OpacityInput = Mat->GetExpressionInputForProperty(MP_Opacity);
		FExpressionInput* OpacityMaskInput = Mat->GetExpressionInputForProperty(MP_OpacityMask);
		FExpressionInput* RefractionInput = Mat->GetExpressionInputForProperty(MP_Refraction);
		FExpressionInput* EmissiveInput = Mat->GetExpressionInputForProperty(MP_EmissiveColor);
		FExpressionInput* SubsurfaceInput = Mat->GetExpressionInputForProperty(MP_SubsurfaceColor);

		bool bHasOpacity = OpacityInput && OpacityInput->Expression;
		bool bHasOpacityMask = OpacityMaskInput && OpacityMaskInput->Expression;
		bool bHasRefraction = RefractionInput && RefractionInput->Expression;
		bool bHasEmissive = EmissiveInput && EmissiveInput->Expression;
		bool bHasSubsurface = SubsurfaceInput && SubsurfaceInput->Expression;

		// Masked without OpacityMask
		if (BM == BLEND_Masked && !bHasOpacityMask)
		{
			AddBlendWarning(TEXT("masked_no_opacity_mask"),
				TEXT("Masked blend mode but no OpacityMask input connected — material will be fully opaque or fully transparent"));
		}

		// Translucent/Additive without Opacity
		if ((BM == BLEND_Translucent || BM == BLEND_Additive) && !bHasOpacity)
		{
			AddBlendWarning(TEXT("translucent_no_opacity"),
				TEXT("Translucent/Additive blend mode but no Opacity input connected — defaults to fully opaque"));
		}

		// Opaque with Opacity or Refraction connected (wasted work)
		if (BM == BLEND_Opaque)
		{
			if (bHasOpacity)
			{
				AddBlendWarning(TEXT("opaque_unused_opacity"),
					TEXT("Opaque blend mode with Opacity connected — Opacity has no effect, consider removing or changing blend mode"));
			}
			if (bHasRefraction)
			{
				AddBlendWarning(TEXT("opaque_unused_refraction"),
					TEXT("Opaque blend mode with Refraction connected — Refraction has no effect on Opaque materials"));
			}
		}

		// PostProcess domain without EmissiveColor
		if (Mat->MaterialDomain == MD_PostProcess && !bHasEmissive)
		{
			AddBlendWarning(TEXT("postprocess_no_emissive"),
				TEXT("PostProcess domain but no EmissiveColor connected — PostProcess materials output through EmissiveColor"));
		}

		// Subsurface outputs on non-subsurface shading model
		EMaterialShadingModel SM = Mat->GetShadingModels().GetFirstShadingModel();
		if (bHasSubsurface && SM != MSM_Subsurface && SM != MSM_SubsurfaceProfile
			&& SM != MSM_PreintegratedSkin && SM != MSM_TwoSidedFoliage)
		{
			AddBlendWarning(TEXT("subsurface_wrong_model"),
				TEXT("SubsurfaceColor connected but shading model is not Subsurface/SubsurfaceProfile/PreintegratedSkin/TwoSidedFoliage — SubsurfaceColor will be ignored"));
		}
	}

	// Apply fixes — delete island expressions
	if (bFixIssues && IslandExprs.Num() > 0)
	{
		GEditor->BeginTransaction(FText::FromString(TEXT("ValidateMaterial_FixIslands")));
		Mat->Modify();
		for (UMaterialExpression* IslandExpr : IslandExprs)
		{
			UMaterialEditingLibrary::DeleteMaterialExpression(Mat, IslandExpr);
		}
		GEditor->EndTransaction();
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetArrayField(TEXT("issues"), IssuesArray);
	ResultJson->SetNumberField(TEXT("issue_count"), IssuesArray.Num());
	ResultJson->SetNumberField(TEXT("fixed_count"), FixedCount);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: render_preview
// Params: { "asset_path": "...", "resolution": 256 }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::RenderPreview(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	int32 Resolution = Params->HasField(TEXT("resolution")) ? static_cast<int32>(Params->GetNumberField(TEXT("resolution"))) : 256;
	if (Resolution <= 0)
	{
		Resolution = 256;
	}

	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!LoadedAsset)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load asset at '%s'"), *AssetPath));
	}

	FObjectThumbnail Thumbnail;
	ThumbnailTools::RenderThumbnail(LoadedAsset, Resolution, Resolution,
		ThumbnailTools::EThumbnailTextureFlushMode::NeverFlush, nullptr, &Thumbnail);

	if (Thumbnail.GetImageWidth() == 0 || Thumbnail.GetImageHeight() == 0)
	{
		return FMonolithActionResult::Error(TEXT("Thumbnail rendering produced an empty image"));
	}

	TArray<uint8>& ThumbData = Thumbnail.AccessImageData();
	int32 Width = Thumbnail.GetImageWidth();
	int32 Height = Thumbnail.GetImageHeight();

	TArray64<uint8> PngData;
	FImageView ImageView((void*)ThumbData.GetData(), Width, Height, ERawImageFormat::BGRA8);
	FImageUtils::CompressImage(PngData, TEXT(".png"), ImageView);

	if (PngData.Num() == 0)
	{
		return FMonolithActionResult::Error(TEXT("Failed to compress thumbnail to PNG"));
	}

	FString MaterialName = FPaths::GetBaseFilename(AssetPath);
	FString PreviewDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Monolith"), TEXT("previews"));
	IFileManager::Get().MakeDirectory(*PreviewDir, true);
	FString FilePath = FPaths::Combine(PreviewDir, FString::Printf(TEXT("%s_%d.png"), *MaterialName, Resolution));

	if (!FFileHelper::SaveArrayToFile(PngData, *FilePath))
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to save PNG to '%s'"), *FilePath));
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetStringField(TEXT("file_path"), FilePath);
	ResultJson->SetNumberField(TEXT("width"), Width);
	ResultJson->SetNumberField(TEXT("height"), Height);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: get_thumbnail
// Params: { "asset_path": "...", "resolution": 256, "save_to_file": false }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::GetThumbnail(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	int32 Resolution = Params->HasField(TEXT("resolution")) ? static_cast<int32>(Params->GetNumberField(TEXT("resolution"))) : 256;
	if (Resolution <= 0)
	{
		Resolution = 256;
	}

	bool bSaveToFile = false;
	if (Params.IsValid())
	{
		Params->TryGetBoolField(TEXT("save_to_file"), bSaveToFile);
	}

	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!LoadedAsset)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load asset at '%s'"), *AssetPath));
	}

	FObjectThumbnail Thumbnail;
	ThumbnailTools::RenderThumbnail(LoadedAsset, Resolution, Resolution,
		ThumbnailTools::EThumbnailTextureFlushMode::NeverFlush, nullptr, &Thumbnail);

	if (Thumbnail.GetImageWidth() == 0 || Thumbnail.GetImageHeight() == 0)
	{
		return FMonolithActionResult::Error(TEXT("Thumbnail rendering produced an empty image"));
	}

	TArray<uint8>& ThumbData = Thumbnail.AccessImageData();
	int32 Width = Thumbnail.GetImageWidth();
	int32 Height = Thumbnail.GetImageHeight();

	TArray64<uint8> PngData;
	FImageView ImageView((void*)ThumbData.GetData(), Width, Height, ERawImageFormat::BGRA8);
	FImageUtils::CompressImage(PngData, TEXT(".png"), ImageView);

	if (PngData.Num() == 0)
	{
		return FMonolithActionResult::Error(TEXT("Failed to compress thumbnail to PNG"));
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetNumberField(TEXT("width"), Width);
	ResultJson->SetNumberField(TEXT("height"), Height);

	if (bSaveToFile)
	{
		FString AssetName = FPaths::GetBaseFilename(AssetPath);
		FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Monolith"), TEXT("previews"));
		IFileManager::Get().MakeDirectory(*SaveDir, true);
		FString FullPath = FPaths::Combine(SaveDir, FString::Printf(TEXT("%s_%d.png"), *AssetName, Resolution));

		if (!FFileHelper::SaveArrayToFile(PngData, *FullPath))
		{
			return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to save thumbnail to '%s'"), *FullPath));
		}

		ResultJson->SetStringField(TEXT("file_path"), FullPath);
	}
	else
	{
		FString Base64String = FBase64::Encode(PngData.GetData(), static_cast<uint32>(PngData.Num()));
		ResultJson->SetStringField(TEXT("format"), TEXT("png"));
		ResultJson->SetStringField(TEXT("encoding"), TEXT("base64"));
		ResultJson->SetStringField(TEXT("data"), Base64String);
	}

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: create_custom_hlsl_node
// Params: { "asset_path": "...", "code": "...", "description": "...", "output_type": "Float4",
//           "inputs": [...], "additional_outputs": [...], "pos_x": 0, "pos_y": 0 }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::CreateCustomHLSLNode(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString Code = Params->GetStringField(TEXT("code"));
	FString Description = Params->HasField(TEXT("description")) ? Params->GetStringField(TEXT("description")) : TEXT("");
	FString OutputType = Params->HasField(TEXT("output_type")) ? Params->GetStringField(TEXT("output_type")) : TEXT("");
	int32 PosX = Params->HasField(TEXT("pos_x")) ? static_cast<int32>(Params->GetNumberField(TEXT("pos_x"))) : 0;
	int32 PosY = Params->HasField(TEXT("pos_y")) ? static_cast<int32>(Params->GetNumberField(TEXT("pos_y"))) : 0;

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	GEditor->BeginTransaction(FText::FromString(TEXT("CreateCustomHLSLNode")));
	Mat->Modify();

	UMaterialExpression* BaseExpr = UMaterialEditingLibrary::CreateMaterialExpression(
		Mat, UMaterialExpressionCustom::StaticClass(), PosX, PosY);

	UMaterialExpressionCustom* CustomExpr = Cast<UMaterialExpressionCustom>(BaseExpr);
	if (!CustomExpr)
	{
		GEditor->EndTransaction();
		return FMonolithActionResult::Error(TEXT("Failed to create Custom HLSL expression"));
	}

	CustomExpr->Code = Code;
	CustomExpr->Description = Description;

	if (!OutputType.IsEmpty())
	{
		CustomExpr->OutputType = ParseCustomOutputType(OutputType);
	}

	// Set inputs from JSON array
	const TArray<TSharedPtr<FJsonValue>>* InputsArray = nullptr;
	if (Params->TryGetArrayField(TEXT("inputs"), InputsArray))
	{
		CustomExpr->Inputs.Empty();
		for (const TSharedPtr<FJsonValue>& InputVal : *InputsArray)
		{
			const TSharedPtr<FJsonObject>* InputObjPtr = nullptr;
			if (InputVal && InputVal->TryGetObject(InputObjPtr) && InputObjPtr)
			{
				FCustomInput NewInput;
				NewInput.InputName = *(*InputObjPtr)->GetStringField(TEXT("name"));
				CustomExpr->Inputs.Add(NewInput);
			}
		}
	}

	// Set additional outputs from JSON array
	const TArray<TSharedPtr<FJsonValue>>* AddOutputsArray = nullptr;
	if (Params->TryGetArrayField(TEXT("additional_outputs"), AddOutputsArray))
	{
		CustomExpr->AdditionalOutputs.Empty();
		for (const TSharedPtr<FJsonValue>& OutVal : *AddOutputsArray)
		{
			const TSharedPtr<FJsonObject>* OutObjPtr = nullptr;
			if (OutVal && OutVal->TryGetObject(OutObjPtr) && OutObjPtr)
			{
				FCustomOutput NewOutput;
				NewOutput.OutputName = *(*OutObjPtr)->GetStringField(TEXT("name"));
				if ((*OutObjPtr)->HasField(TEXT("type")))
				{
					NewOutput.OutputType = ParseCustomOutputType((*OutObjPtr)->GetStringField(TEXT("type")));
				}
				CustomExpr->AdditionalOutputs.Add(NewOutput);
			}
		}
	}

	CustomExpr->RebuildOutputs();
	GEditor->EndTransaction();

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetStringField(TEXT("expression_name"), CustomExpr->GetName());
	ResultJson->SetStringField(TEXT("description"), CustomExpr->Description);
	ResultJson->SetStringField(TEXT("output_type"), CustomOutputTypeToString(CustomExpr->OutputType));
	ResultJson->SetNumberField(TEXT("input_count"), CustomExpr->Inputs.Num());
	ResultJson->SetNumberField(TEXT("additional_output_count"), CustomExpr->AdditionalOutputs.Num());
	ResultJson->SetNumberField(TEXT("pos_x"), PosX);
	ResultJson->SetNumberField(TEXT("pos_y"), PosY);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: get_layer_info
// Params: { "asset_path": "..." }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::GetLayerInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!LoadedAsset)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load asset at '%s'"), *AssetPath));
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);

	UMaterialFunctionMaterialLayer* Layer = Cast<UMaterialFunctionMaterialLayer>(LoadedAsset);
	UMaterialFunctionMaterialLayerBlend* LayerBlend = Cast<UMaterialFunctionMaterialLayerBlend>(LoadedAsset);
	UMaterialFunction* MatFunc = Cast<UMaterialFunction>(LoadedAsset);

	if (Layer)
	{
		ResultJson->SetStringField(TEXT("type"), TEXT("MaterialLayer"));
		MatFunc = Layer;
	}
	else if (LayerBlend)
	{
		ResultJson->SetStringField(TEXT("type"), TEXT("MaterialLayerBlend"));
		MatFunc = LayerBlend;
	}
	else if (MatFunc)
	{
		ResultJson->SetStringField(TEXT("type"), TEXT("MaterialFunction"));
	}
	else
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Asset '%s' is not a MaterialFunction, MaterialLayer, or MaterialLayerBlend"), *AssetPath));
	}

	ResultJson->SetStringField(TEXT("description"), MatFunc->Description);

	TArray<TSharedPtr<FJsonValue>> FuncExpressionsArray;
	TArray<TSharedPtr<FJsonValue>> FuncInputsArray;
	TArray<TSharedPtr<FJsonValue>> FuncOutputsArray;

	TConstArrayView<TObjectPtr<UMaterialExpression>> FuncExprs = MatFunc->GetExpressions();

	for (const TObjectPtr<UMaterialExpression>& Expr : FuncExprs)
	{
		if (!Expr)
		{
			continue;
		}

		if (const auto* FuncInput = Cast<UMaterialExpressionFunctionInput>(Expr))
		{
			auto InputJson = MakeShared<FJsonObject>();
			InputJson->SetStringField(TEXT("name"), FuncInput->InputName.ToString());
			InputJson->SetStringField(TEXT("expression_name"), FuncInput->GetName());
			InputJson->SetNumberField(TEXT("sort_priority"), FuncInput->SortPriority);
			FuncInputsArray.Add(MakeShared<FJsonValueObject>(InputJson));
		}

		if (const auto* FuncOutput = Cast<UMaterialExpressionFunctionOutput>(Expr))
		{
			auto OutputJson = MakeShared<FJsonObject>();
			OutputJson->SetStringField(TEXT("name"), FuncOutput->OutputName.ToString());
			OutputJson->SetStringField(TEXT("expression_name"), FuncOutput->GetName());
			OutputJson->SetNumberField(TEXT("sort_priority"), FuncOutput->SortPriority);
			FuncOutputsArray.Add(MakeShared<FJsonValueObject>(OutputJson));
		}

		auto ExprJson = MakeShared<FJsonObject>();
		ExprJson->SetStringField(TEXT("name"), Expr->GetName());
		ExprJson->SetStringField(TEXT("class"), Expr->GetClass()->GetName());
		ExprJson->SetNumberField(TEXT("pos_x"), Expr->MaterialExpressionEditorX);
		ExprJson->SetNumberField(TEXT("pos_y"), Expr->MaterialExpressionEditorY);
		FuncExpressionsArray.Add(MakeShared<FJsonValueObject>(ExprJson));
	}

	ResultJson->SetArrayField(TEXT("inputs"), FuncInputsArray);
	ResultJson->SetArrayField(TEXT("outputs"), FuncOutputsArray);
	ResultJson->SetArrayField(TEXT("expressions"), FuncExpressionsArray);
	ResultJson->SetNumberField(TEXT("expression_count"), FuncExpressionsArray.Num());

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Wave 2 — Helpers
// ============================================================================

// Generic enum parser using StaticEnum — handles bare names ("Opaque"),
// prefixed ("BLEND_Opaque"), and fully qualified ("EBlendMode::Opaque") forms.
template<typename TEnum>
static bool ParseEnum(const FString& Str, TEnum& OutValue, FString& OutError)
{
	const UEnum* Enum = StaticEnum<TEnum>();

	// Pass 1: try the raw input (handles prefixed and fully qualified forms)
	int64 Value = Enum->GetValueByNameString(Str);

	// Pass 2: if raw failed, try prepending common enum prefixes for short names
	if (Value == INDEX_NONE)
	{
		// Extract the prefix from the first enum value (e.g., "BLEND_" from "BLEND_Opaque")
		FString FirstName = Enum->GetNameStringByIndex(0);
		int32 UnderscoreIdx;
		if (FirstName.FindChar('_', UnderscoreIdx))
		{
			FString Prefix = FirstName.Left(UnderscoreIdx + 1); // e.g., "BLEND_"
			Value = Enum->GetValueByNameString(Prefix + Str);
		}
	}

	if (Value == INDEX_NONE)
	{
		TArray<FString> ValidNames;
		for (int32 i = 0; i < Enum->NumEnums() - 1; i++)
		{
			FString Name = Enum->GetNameStringByIndex(i);
			// Filter out sentinel values (_MAX, _NUM, FromMaterialExpression)
			if (Name.EndsWith(TEXT("_MAX")) || Name.EndsWith(TEXT("_NUM")) || Name.Contains(TEXT("FromMaterial")))
			{
				continue;
			}
			ValidNames.Add(Name);
		}
		OutError = FString::Printf(TEXT("Invalid value '%s'. Valid values: %s"), *Str, *FString::Join(ValidNames, TEXT(", ")));
		return false;
	}
	OutValue = static_cast<TEnum>(Value);
	return true;
}

// ============================================================================
// Action: create_material
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::CreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	// Parse optional properties
	FString BlendModeStr = Params->HasField(TEXT("blend_mode")) ? Params->GetStringField(TEXT("blend_mode")) : TEXT("Opaque");
	FString ShadingModelStr = Params->HasField(TEXT("shading_model")) ? Params->GetStringField(TEXT("shading_model")) : TEXT("DefaultLit");
	FString DomainStr = Params->HasField(TEXT("material_domain")) ? Params->GetStringField(TEXT("material_domain")) : TEXT("Surface");
	bool bTwoSided = Params->HasField(TEXT("two_sided")) ? Params->GetBoolField(TEXT("two_sided")) : false;

	// Extract package path and asset name from the asset path
	FString PackagePath, AssetName;
	int32 LastSlash;
	if (AssetPath.FindLastChar('/', LastSlash))
	{
		PackagePath = AssetPath.Left(LastSlash);
		AssetName = AssetPath.Mid(LastSlash + 1);
	}
	else
	{
		return FMonolithActionResult::Error(TEXT("Invalid asset path — must contain at least one '/' (e.g. /Game/Materials/M_Name)"));
	}

	if (AssetName.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("Asset name is empty"));
	}

	// Check if asset already exists
	UObject* Existing = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (Existing)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Asset already exists at '%s'"), *AssetPath));
	}

	// Create package and material
	UPackage* Pkg = CreatePackage(*AssetPath);
	if (!Pkg)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to create package at '%s'"), *AssetPath));
	}

	UMaterial* NewMat = NewObject<UMaterial>(Pkg, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
	if (!NewMat)
	{
		return FMonolithActionResult::Error(TEXT("Failed to create UMaterial object"));
	}

	// Set material properties
	FString EnumError;
	EMaterialDomain Domain;
	if (!ParseEnum<EMaterialDomain>(DomainStr, Domain, EnumError))
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("material_domain: %s"), *EnumError));
	}
	EBlendMode BlendMode;
	if (!ParseEnum<EBlendMode>(BlendModeStr, BlendMode, EnumError))
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("blend_mode: %s"), *EnumError));
	}
	EMaterialShadingModel ShadingModel;
	if (!ParseEnum<EMaterialShadingModel>(ShadingModelStr, ShadingModel, EnumError))
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("shading_model: %s"), *EnumError));
	}
	NewMat->MaterialDomain = Domain;
	NewMat->BlendMode = BlendMode;
	NewMat->SetShadingModel(ShadingModel);
	NewMat->TwoSided = bTwoSided;

	// Register with asset registry and mark dirty
	FAssetRegistryModule::AssetCreated(NewMat);
	Pkg->MarkPackageDirty();

	// Trigger initial compile
	NewMat->PreEditChange(nullptr);
	NewMat->PostEditChange();

	// Save to disk so LoadAsset can find it later
	FString PackageFilename = FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Pkg, NewMat, *PackageFilename, SaveArgs);

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), NewMat->GetPathName());
	ResultJson->SetStringField(TEXT("asset_name"), AssetName);
	ResultJson->SetStringField(TEXT("blend_mode"), BlendModeStr);
	ResultJson->SetStringField(TEXT("shading_model"), ShadingModelStr);
	ResultJson->SetStringField(TEXT("material_domain"), DomainStr);
	ResultJson->SetBoolField(TEXT("two_sided"), bTwoSided);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: create_material_instance
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::CreateMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ParentPath = Params->GetStringField(TEXT("parent_material"));

	// Load parent material
	UObject* ParentObj = UEditorAssetLibrary::LoadAsset(ParentPath);
	UMaterialInterface* ParentMat = ParentObj ? Cast<UMaterialInterface>(ParentObj) : nullptr;
	if (!ParentMat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load parent material at '%s'"), *ParentPath));
	}

	// Check if asset already exists
	UObject* Existing = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (Existing)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Asset already exists at '%s'"), *AssetPath));
	}

	// Extract asset name
	FString AssetName;
	int32 LastSlash;
	if (!AssetPath.FindLastChar('/', LastSlash) || LastSlash == AssetPath.Len() - 1)
	{
		return FMonolithActionResult::Error(TEXT("Invalid asset path"));
	}
	AssetName = AssetPath.Mid(LastSlash + 1);

	// Create package and MIC
	UPackage* Pkg = CreatePackage(*AssetPath);
	if (!Pkg)
	{
		return FMonolithActionResult::Error(TEXT("Failed to create package"));
	}

	UMaterialInstanceConstant* MIC = NewObject<UMaterialInstanceConstant>(Pkg, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
	if (!MIC)
	{
		return FMonolithActionResult::Error(TEXT("Failed to create UMaterialInstanceConstant"));
	}

	MIC->SetParentEditorOnly(ParentMat);

	// Apply scalar parameter overrides
	int32 ScalarCount = 0;
	const TSharedPtr<FJsonObject>* ScalarParams = nullptr;
	if (Params->TryGetObjectField(TEXT("scalar_parameters"), ScalarParams))
	{
		for (const auto& Pair : (*ScalarParams)->Values)
		{
			float Value = static_cast<float>(Pair.Value->AsNumber());
			MIC->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(*Pair.Key), Value);
			ScalarCount++;
		}
	}

	// Apply vector parameter overrides
	int32 VectorCount = 0;
	const TSharedPtr<FJsonObject>* VectorParams = nullptr;
	if (Params->TryGetObjectField(TEXT("vector_parameters"), VectorParams))
	{
		for (const auto& Pair : (*VectorParams)->Values)
		{
			const TSharedPtr<FJsonObject>* ColorObj = nullptr;
			if (Pair.Value->TryGetObject(ColorObj))
			{
				FLinearColor Color;
				Color.R = (*ColorObj)->HasField(TEXT("R")) ? static_cast<float>((*ColorObj)->GetNumberField(TEXT("R"))) : 0.f;
				Color.G = (*ColorObj)->HasField(TEXT("G")) ? static_cast<float>((*ColorObj)->GetNumberField(TEXT("G"))) : 0.f;
				Color.B = (*ColorObj)->HasField(TEXT("B")) ? static_cast<float>((*ColorObj)->GetNumberField(TEXT("B"))) : 0.f;
				Color.A = (*ColorObj)->HasField(TEXT("A")) ? static_cast<float>((*ColorObj)->GetNumberField(TEXT("A"))) : 1.f;
				MIC->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(*Pair.Key), Color);
				VectorCount++;
			}
		}
	}

	// Apply texture parameter overrides
	int32 TextureCount = 0;
	const TSharedPtr<FJsonObject>* TextureParams = nullptr;
	if (Params->TryGetObjectField(TEXT("texture_parameters"), TextureParams))
	{
		for (const auto& Pair : (*TextureParams)->Values)
		{
			FString TexPath = Pair.Value->AsString();
			UTexture* Tex = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(TexPath));
			if (Tex)
			{
				MIC->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(*Pair.Key), Tex);
				TextureCount++;
			}
		}
	}

	// Apply static switch parameter overrides
	int32 SwitchCount = 0;
	const TSharedPtr<FJsonObject>* SwitchParams = nullptr;
	if (Params->TryGetObjectField(TEXT("static_switch_parameters"), SwitchParams))
	{
		for (const auto& Pair : (*SwitchParams)->Values)
		{
			bool bValue = Pair.Value->AsBool();
			MIC->SetStaticSwitchParameterValueEditorOnly(FMaterialParameterInfo(*Pair.Key), bValue);
			SwitchCount++;
		}
	}

	// If static switches were set, update the static permutation
	if (SwitchCount > 0)
	{
		UMaterialEditingLibrary::UpdateMaterialInstance(MIC);
	}

	FAssetRegistryModule::AssetCreated(MIC);
	Pkg->MarkPackageDirty();

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), MIC->GetPathName());
	ResultJson->SetStringField(TEXT("asset_name"), AssetName);
	ResultJson->SetStringField(TEXT("parent_material"), ParentMat->GetPathName());
	ResultJson->SetNumberField(TEXT("scalar_overrides"), ScalarCount);
	ResultJson->SetNumberField(TEXT("vector_overrides"), VectorCount);
	ResultJson->SetNumberField(TEXT("texture_overrides"), TextureCount);
	ResultJson->SetNumberField(TEXT("static_switch_overrides"), SwitchCount);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: set_material_property
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::SetMaterialProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	GEditor->BeginTransaction(FText::FromString(TEXT("SetMaterialProperty")));
	Mat->Modify();

	TArray<TSharedPtr<FJsonValue>> ChangedArray;
	int32 ChangeCount = 0;

	auto RecordChange = [&](const FString& PropName, const FString& Value)
	{
		auto ChangeJson = MakeShared<FJsonObject>();
		ChangeJson->SetStringField(TEXT("property"), PropName);
		ChangeJson->SetStringField(TEXT("value"), Value);
		ChangedArray.Add(MakeShared<FJsonValueObject>(ChangeJson));
		ChangeCount++;
	};

	if (Params->HasField(TEXT("blend_mode")))
	{
		FString Val = Params->GetStringField(TEXT("blend_mode"));
		FString EnumError;
		EBlendMode ParsedMode;
		if (!ParseEnum<EBlendMode>(Val, ParsedMode, EnumError))
		{
			GEditor->EndTransaction();
			return FMonolithActionResult::Error(FString::Printf(TEXT("blend_mode: %s"), *EnumError));
		}
		Mat->BlendMode = ParsedMode;
		RecordChange(TEXT("blend_mode"), Val);
	}
	if (Params->HasField(TEXT("shading_model")))
	{
		FString Val = Params->GetStringField(TEXT("shading_model"));
		FString EnumError;
		EMaterialShadingModel ParsedModel;
		if (!ParseEnum<EMaterialShadingModel>(Val, ParsedModel, EnumError))
		{
			GEditor->EndTransaction();
			return FMonolithActionResult::Error(FString::Printf(TEXT("shading_model: %s"), *EnumError));
		}
		Mat->SetShadingModel(ParsedModel);
		RecordChange(TEXT("shading_model"), Val);
	}
	if (Params->HasField(TEXT("material_domain")))
	{
		FString Val = Params->GetStringField(TEXT("material_domain"));
		FString EnumError;
		EMaterialDomain ParsedDomain;
		if (!ParseEnum<EMaterialDomain>(Val, ParsedDomain, EnumError))
		{
			GEditor->EndTransaction();
			return FMonolithActionResult::Error(FString::Printf(TEXT("material_domain: %s"), *EnumError));
		}
		Mat->MaterialDomain = ParsedDomain;
		RecordChange(TEXT("material_domain"), Val);
	}
	if (Params->HasField(TEXT("two_sided")))
	{
		bool Val = Params->GetBoolField(TEXT("two_sided"));
		Mat->TwoSided = Val;
		RecordChange(TEXT("two_sided"), Val ? TEXT("true") : TEXT("false"));
	}
	if (Params->HasField(TEXT("opacity_mask_clip_value")))
	{
		float Val = static_cast<float>(Params->GetNumberField(TEXT("opacity_mask_clip_value")));
		Mat->OpacityMaskClipValue = Val;
		RecordChange(TEXT("opacity_mask_clip_value"), FString::SanitizeFloat(Val));
	}
	if (Params->HasField(TEXT("dithered_lod_transition")))
	{
		bool Val = Params->GetBoolField(TEXT("dithered_lod_transition"));
		Mat->DitheredLODTransition = Val;
		RecordChange(TEXT("dithered_lod_transition"), Val ? TEXT("true") : TEXT("false"));
	}
	if (Params->HasField(TEXT("used_with_skeletal_mesh")))
	{
		bool Val = Params->GetBoolField(TEXT("used_with_skeletal_mesh"));
		if (Val)
		{
			bool bRecompile = false;
			UMaterialEditingLibrary::SetMaterialUsage(Mat, MATUSAGE_SkeletalMesh, bRecompile);
		}
		RecordChange(TEXT("used_with_skeletal_mesh"), Val ? TEXT("true") : TEXT("false"));
	}
	if (Params->HasField(TEXT("used_with_particle_sprites")))
	{
		bool Val = Params->GetBoolField(TEXT("used_with_particle_sprites"));
		if (Val)
		{
			bool bRecompile = false;
			UMaterialEditingLibrary::SetMaterialUsage(Mat, MATUSAGE_ParticleSprites, bRecompile);
		}
		RecordChange(TEXT("used_with_particle_sprites"), Val ? TEXT("true") : TEXT("false"));
	}
	if (Params->HasField(TEXT("used_with_niagara_sprites")))
	{
		bool Val = Params->GetBoolField(TEXT("used_with_niagara_sprites"));
		if (Val)
		{
			bool bRecompile = false;
			UMaterialEditingLibrary::SetMaterialUsage(Mat, MATUSAGE_NiagaraSprites, bRecompile);
		}
		RecordChange(TEXT("used_with_niagara_sprites"), Val ? TEXT("true") : TEXT("false"));
	}
	if (Params->HasField(TEXT("used_with_niagara_meshes")))
	{
		bool Val = Params->GetBoolField(TEXT("used_with_niagara_meshes"));
		if (Val)
		{
			bool bRecompile = false;
			UMaterialEditingLibrary::SetMaterialUsage(Mat, MATUSAGE_NiagaraMeshParticles, bRecompile);
		}
		RecordChange(TEXT("used_with_niagara_meshes"), Val ? TEXT("true") : TEXT("false"));
	}
	if (Params->HasField(TEXT("used_with_niagara_ribbons")))
	{
		bool Val = Params->GetBoolField(TEXT("used_with_niagara_ribbons"));
		if (Val)
		{
			bool bRecompile = false;
			UMaterialEditingLibrary::SetMaterialUsage(Mat, MATUSAGE_NiagaraRibbons, bRecompile);
		}
		RecordChange(TEXT("used_with_niagara_ribbons"), Val ? TEXT("true") : TEXT("false"));
	}
	if (Params->HasField(TEXT("used_with_morph_targets")))
	{
		bool Val = Params->GetBoolField(TEXT("used_with_morph_targets"));
		if (Val)
		{
			bool bRecompile = false;
			UMaterialEditingLibrary::SetMaterialUsage(Mat, MATUSAGE_MorphTargets, bRecompile);
		}
		RecordChange(TEXT("used_with_morph_targets"), Val ? TEXT("true") : TEXT("false"));
	}
	if (Params->HasField(TEXT("used_with_instanced_static_meshes")))
	{
		bool Val = Params->GetBoolField(TEXT("used_with_instanced_static_meshes"));
		if (Val)
		{
			bool bRecompile = false;
			UMaterialEditingLibrary::SetMaterialUsage(Mat, MATUSAGE_InstancedStaticMeshes, bRecompile);
		}
		RecordChange(TEXT("used_with_instanced_static_meshes"), Val ? TEXT("true") : TEXT("false"));
	}
	if (Params->HasField(TEXT("used_with_static_lighting")))
	{
		bool Val = Params->GetBoolField(TEXT("used_with_static_lighting"));
		if (Val)
		{
			bool bRecompile = false;
			UMaterialEditingLibrary::SetMaterialUsage(Mat, MATUSAGE_StaticLighting, bRecompile);
		}
		RecordChange(TEXT("used_with_static_lighting"), Val ? TEXT("true") : TEXT("false"));
	}
	if (Params->HasField(TEXT("fully_rough")))
	{
		bool Val = Params->GetBoolField(TEXT("fully_rough"));
		Mat->bFullyRough = Val;
		RecordChange(TEXT("fully_rough"), Val ? TEXT("true") : TEXT("false"));
	}
	if (Params->HasField(TEXT("cast_shadow_as_masked")))
	{
		bool Val = Params->GetBoolField(TEXT("cast_shadow_as_masked"));
		Mat->bCastDynamicShadowAsMasked = Val;
		RecordChange(TEXT("cast_shadow_as_masked"), Val ? TEXT("true") : TEXT("false"));
	}
	// Note: bOutputVelocityOnBasePass removed in UE 5.7 — velocity output is now automatic

	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	GEditor->EndTransaction();

	// Bug fix: flush property changes to disk so subsequent LoadAsset calls
	// (e.g. get_compilation_stats) don't read stale on-disk values after GC
	UEditorAssetLibrary::SaveAsset(AssetPath, false);

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetNumberField(TEXT("changes"), ChangeCount);
	ResultJson->SetArrayField(TEXT("changed"), ChangedArray);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: delete_expression
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::DeleteExpression(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ExprName = Params->GetStringField(TEXT("expression_name"));

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	// Find the expression
	UMaterialExpression* TargetExpr = nullptr;
	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Mat->GetExpressions();
	for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
	{
		if (Expr && Expr->GetName() == ExprName)
		{
			TargetExpr = Expr;
			break;
		}
	}

	if (!TargetExpr)
	{
		TArray<FString> AvailableNames;
		for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
		{
			if (Expr) AvailableNames.Add(Expr->GetName());
		}
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Expression '%s' not found. Available: %s"),
			*ExprName, *FString::Join(AvailableNames, TEXT(", "))));
	}

	GEditor->BeginTransaction(FText::FromString(TEXT("DeleteExpression")));
	Mat->Modify();

	FString ClassName = TargetExpr->GetClass()->GetName();
	UMaterialEditingLibrary::DeleteMaterialExpression(Mat, TargetExpr);

	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	GEditor->EndTransaction();

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetStringField(TEXT("deleted"), ExprName);
	ResultJson->SetStringField(TEXT("class"), ClassName);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: get_material_parameters
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::GetMaterialParameters(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	UMaterialInterface* MatInterface = LoadedAsset ? Cast<UMaterialInterface>(LoadedAsset) : nullptr;
	if (!MatInterface)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load material at '%s'"), *AssetPath));
	}

	TArray<TSharedPtr<FJsonValue>> ScalarArray, VectorArray, TextureArray, SwitchArray;

	// Scalar parameters
	TArray<FMaterialParameterInfo> ScalarInfos;
	TArray<FGuid> ScalarGuids;
	MatInterface->GetAllScalarParameterInfo(ScalarInfos, ScalarGuids);
	for (int32 i = 0; i < ScalarInfos.Num(); ++i)
	{
		float Value = 0.f;
		MatInterface->GetScalarParameterValue(ScalarInfos[i], Value);

		auto PJson = MakeShared<FJsonObject>();
		PJson->SetStringField(TEXT("name"), ScalarInfos[i].Name.ToString());
		PJson->SetNumberField(TEXT("value"), Value);
		PJson->SetStringField(TEXT("type"), TEXT("scalar"));
		ScalarArray.Add(MakeShared<FJsonValueObject>(PJson));
	}

	// Vector parameters
	TArray<FMaterialParameterInfo> VectorInfos;
	TArray<FGuid> VectorGuids;
	MatInterface->GetAllVectorParameterInfo(VectorInfos, VectorGuids);
	for (int32 i = 0; i < VectorInfos.Num(); ++i)
	{
		FLinearColor Value;
		MatInterface->GetVectorParameterValue(VectorInfos[i], Value);

		auto PJson = MakeShared<FJsonObject>();
		PJson->SetStringField(TEXT("name"), VectorInfos[i].Name.ToString());
		auto ColorJson = MakeShared<FJsonObject>();
		ColorJson->SetNumberField(TEXT("R"), Value.R);
		ColorJson->SetNumberField(TEXT("G"), Value.G);
		ColorJson->SetNumberField(TEXT("B"), Value.B);
		ColorJson->SetNumberField(TEXT("A"), Value.A);
		PJson->SetObjectField(TEXT("value"), ColorJson);
		PJson->SetStringField(TEXT("type"), TEXT("vector"));
		VectorArray.Add(MakeShared<FJsonValueObject>(PJson));
	}

	// Texture parameters
	TArray<FMaterialParameterInfo> TextureInfos;
	TArray<FGuid> TextureGuids;
	MatInterface->GetAllTextureParameterInfo(TextureInfos, TextureGuids);
	for (int32 i = 0; i < TextureInfos.Num(); ++i)
	{
		UTexture* Tex = nullptr;
		MatInterface->GetTextureParameterValue(TextureInfos[i], Tex);

		auto PJson = MakeShared<FJsonObject>();
		PJson->SetStringField(TEXT("name"), TextureInfos[i].Name.ToString());
		PJson->SetStringField(TEXT("value"), Tex ? Tex->GetPathName() : TEXT("None"));
		PJson->SetStringField(TEXT("type"), TEXT("texture"));
		TextureArray.Add(MakeShared<FJsonValueObject>(PJson));
	}

	// Static switch parameters
	TArray<FMaterialParameterInfo> SwitchInfos;
	TArray<FGuid> SwitchGuids;
	MatInterface->GetAllStaticSwitchParameterInfo(SwitchInfos, SwitchGuids);
	for (int32 i = 0; i < SwitchInfos.Num(); ++i)
	{
		bool Value = false;
		FGuid OutGuid;
		MatInterface->GetStaticSwitchParameterValue(SwitchInfos[i], Value, OutGuid);

		auto PJson = MakeShared<FJsonObject>();
		PJson->SetStringField(TEXT("name"), SwitchInfos[i].Name.ToString());
		PJson->SetBoolField(TEXT("value"), Value);
		PJson->SetStringField(TEXT("type"), TEXT("static_switch"));
		SwitchArray.Add(MakeShared<FJsonValueObject>(PJson));
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetArrayField(TEXT("scalar_parameters"), ScalarArray);
	ResultJson->SetArrayField(TEXT("vector_parameters"), VectorArray);
	ResultJson->SetArrayField(TEXT("texture_parameters"), TextureArray);
	ResultJson->SetArrayField(TEXT("static_switch_parameters"), SwitchArray);
	ResultJson->SetNumberField(TEXT("total_parameters"),
		ScalarArray.Num() + VectorArray.Num() + TextureArray.Num() + SwitchArray.Num());

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: set_instance_parameter
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::SetInstanceParameter(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ParamName = Params->GetStringField(TEXT("parameter_name"));

	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	UMaterialInstanceConstant* MIC = LoadedAsset ? Cast<UMaterialInstanceConstant>(LoadedAsset) : nullptr;
	if (!MIC)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load material instance at '%s'"), *AssetPath));
	}

	MIC->Modify();
	FString SetType;
	FString SetValue;

	FMaterialParameterInfo ParamInfo(*ParamName);

	if (Params->HasField(TEXT("scalar_value")))
	{
		float Val = static_cast<float>(Params->GetNumberField(TEXT("scalar_value")));
		MIC->SetScalarParameterValueEditorOnly(ParamInfo, Val);
		SetType = TEXT("scalar");
		SetValue = FString::SanitizeFloat(Val);
	}
	else if (Params->HasField(TEXT("vector_value")))
	{
		const TSharedPtr<FJsonObject>* ColorObj = nullptr;
		if (Params->TryGetObjectField(TEXT("vector_value"), ColorObj))
		{
			FLinearColor Color;
			Color.R = (*ColorObj)->HasField(TEXT("R")) ? static_cast<float>((*ColorObj)->GetNumberField(TEXT("R"))) : 0.f;
			Color.G = (*ColorObj)->HasField(TEXT("G")) ? static_cast<float>((*ColorObj)->GetNumberField(TEXT("G"))) : 0.f;
			Color.B = (*ColorObj)->HasField(TEXT("B")) ? static_cast<float>((*ColorObj)->GetNumberField(TEXT("B"))) : 0.f;
			Color.A = (*ColorObj)->HasField(TEXT("A")) ? static_cast<float>((*ColorObj)->GetNumberField(TEXT("A"))) : 1.f;
			MIC->SetVectorParameterValueEditorOnly(ParamInfo, Color);
			SetType = TEXT("vector");
			SetValue = FString::Printf(TEXT("(%.3f, %.3f, %.3f, %.3f)"), Color.R, Color.G, Color.B, Color.A);
		}
	}
	else if (Params->HasField(TEXT("texture_value")))
	{
		FString TexPath = Params->GetStringField(TEXT("texture_value"));
		UTexture* Tex = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(TexPath));
		if (!Tex)
		{
			return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load texture at '%s'"), *TexPath));
		}
		MIC->SetTextureParameterValueEditorOnly(ParamInfo, Tex);
		SetType = TEXT("texture");
		SetValue = TexPath;
	}
	else if (Params->HasField(TEXT("switch_value")))
	{
		bool Val = Params->GetBoolField(TEXT("switch_value"));
		MIC->SetStaticSwitchParameterValueEditorOnly(ParamInfo, Val);
		SetType = TEXT("static_switch");
		SetValue = Val ? TEXT("true") : TEXT("false");
	}
	else
	{
		return FMonolithActionResult::Error(TEXT("Must provide one of: scalar_value, vector_value, texture_value, switch_value"));
	}

	MIC->MarkPackageDirty();

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetStringField(TEXT("parameter_name"), ParamName);
	ResultJson->SetStringField(TEXT("type"), SetType);
	ResultJson->SetStringField(TEXT("value"), SetValue);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: recompile_material
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::RecompileMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	UMaterialInterface* MatInterface = LoadedAsset ? Cast<UMaterialInterface>(LoadedAsset) : nullptr;
	if (!MatInterface)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load material at '%s'"), *AssetPath));
	}

	// For base materials, trigger full recompile
	UMaterial* BaseMat = MatInterface->GetMaterial();
	if (BaseMat)
	{
		UMaterialEditingLibrary::RecompileMaterial(BaseMat);
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetStringField(TEXT("status"), TEXT("recompiled"));

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: duplicate_material
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::DuplicateMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString SourcePath = Params->GetStringField(TEXT("source_path"));
	FString DestPath = Params->GetStringField(TEXT("dest_path"));

	// Check source exists
	UObject* SourceObj = UEditorAssetLibrary::LoadAsset(SourcePath);
	if (!SourceObj)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Source material not found at '%s'"), *SourcePath));
	}

	// Check dest doesn't exist
	if (UEditorAssetLibrary::LoadAsset(DestPath))
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Asset already exists at '%s'"), *DestPath));
	}

	UObject* DuplicatedObj = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestPath);
	if (!DuplicatedObj)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to duplicate '%s' to '%s'"), *SourcePath, *DestPath));
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("source_path"), SourcePath);
	ResultJson->SetStringField(TEXT("dest_path"), DestPath);
	ResultJson->SetStringField(TEXT("asset_class"), SourceObj->GetClass()->GetName());

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: get_compilation_stats
// Params: { "asset_path": "..." }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::GetCompilationStats(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	UMaterialInterface* MatInterface = LoadedAsset ? Cast<UMaterialInterface>(LoadedAsset) : nullptr;
	if (!MatInterface)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load material at '%s'"), *AssetPath));
	}

	UMaterial* BaseMat = MatInterface->GetMaterial();
	if (!BaseMat)
	{
		return FMonolithActionResult::Error(TEXT("Could not resolve base material"));
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);

	// Get material resource for the current shader platform
	const EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel];
	FMaterialResource* MatResource = BaseMat->GetMaterialResource(ShaderPlatform);
	if (MatResource)
	{
		bool bIsCompiled = MatResource->IsGameThreadShaderMapComplete();
		ResultJson->SetBoolField(TEXT("is_compiled"), bIsCompiled);

#if WITH_EDITOR
		// Sampler count
		int32 SamplerCount = MatResource->GetSamplerUsage();
		ResultJson->SetNumberField(TEXT("num_samplers"), SamplerCount);

		// Estimated texture samples (VS + PS)
		uint32 VSSamples = 0, PSSamples = 0;
		MatResource->GetEstimatedNumTextureSamples(VSSamples, PSSamples);
		ResultJson->SetNumberField(TEXT("estimated_vs_texture_samples"), static_cast<double>(VSSamples));
		ResultJson->SetNumberField(TEXT("estimated_ps_texture_samples"), static_cast<double>(PSSamples));

		// User interpolator usage
		uint32 NumUsedUVScalars = 0, NumUsedCustomInterpolatorScalars = 0;
		MatResource->GetUserInterpolatorUsage(NumUsedUVScalars, NumUsedCustomInterpolatorScalars);
		ResultJson->SetNumberField(TEXT("used_uv_scalars"), static_cast<double>(NumUsedUVScalars));
		ResultJson->SetNumberField(TEXT("used_custom_interpolator_scalars"), static_cast<double>(NumUsedCustomInterpolatorScalars));
#endif

		// Compile errors
		const TArray<FString>& Errors = MatResource->GetCompileErrors();
		if (Errors.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ErrorsArray;
			for (const FString& Err : Errors)
			{
				ErrorsArray.Add(MakeShared<FJsonValueString>(Err));
			}
			ResultJson->SetArrayField(TEXT("compile_errors"), ErrorsArray);
		}
	}
	else
	{
		ResultJson->SetBoolField(TEXT("is_compiled"), false);
		ResultJson->SetStringField(TEXT("note"), TEXT("Material resource not available - try recompile_material first"));
	}

	// Instruction counts via UMaterialEditingLibrary::GetStatistics (uses FMaterialStatsUtils internally)
	FMaterialStatistics Stats = UMaterialEditingLibrary::GetStatistics(MatInterface);
	ResultJson->SetNumberField(TEXT("num_vertex_shader_instructions"), Stats.NumVertexShaderInstructions);
	ResultJson->SetNumberField(TEXT("num_pixel_shader_instructions"), Stats.NumPixelShaderInstructions);

	// Override is_compiled if we got valid instruction counts — shader map may not be loaded in memory
	// but stats are cached from a previous compilation. Without this, is_compiled=false is misleading.
	if (!ResultJson->GetBoolField(TEXT("is_compiled")) && (Stats.NumVertexShaderInstructions > 0 || Stats.NumPixelShaderInstructions > 0))
	{
		ResultJson->SetBoolField(TEXT("is_compiled"), true);
	}

	// Material properties (always available from the base material)
	ResultJson->SetStringField(TEXT("blend_mode"),
		BaseMat->BlendMode == BLEND_Opaque ? TEXT("Opaque") :
		BaseMat->BlendMode == BLEND_Masked ? TEXT("Masked") :
		BaseMat->BlendMode == BLEND_Translucent ? TEXT("Translucent") :
		BaseMat->BlendMode == BLEND_Additive ? TEXT("Additive") :
		BaseMat->BlendMode == BLEND_AlphaComposite ? TEXT("AlphaComposite") :
		BaseMat->BlendMode == BLEND_Modulate ? TEXT("Modulate") : TEXT("Other"));
	ResultJson->SetBoolField(TEXT("two_sided"), BaseMat->TwoSided);
	ResultJson->SetNumberField(TEXT("expression_count"), BaseMat->GetExpressions().Num());

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: set_expression_property
// Params: { "asset_path": "...", "expression_name": "...", "property_name": "...", "value": "..." }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::SetExpressionProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ExprName = Params->GetStringField(TEXT("expression_name"));
	FString PropName = Params->GetStringField(TEXT("property_name"));

	// Extract value as string regardless of JSON type — GetStringField returns ""
	// for JSON numbers, which then gets Atof'd to 0.0 (Bug #6).
	FString ValueStr;
	{
		const TSharedPtr<FJsonValue>* ValueField = Params->Values.Find(TEXT("value"));
		if (ValueField && ValueField->IsValid())
		{
			switch ((*ValueField)->Type)
			{
				case EJson::Number:
					ValueStr = FString::SanitizeFloat((*ValueField)->AsNumber());
					break;
				case EJson::Boolean:
					ValueStr = (*ValueField)->AsBool() ? TEXT("true") : TEXT("false");
					break;
				case EJson::String:
					ValueStr = (*ValueField)->AsString();
					break;
				default:
					break;
			}
		}
	}

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	// Find expression
	UMaterialExpression* TargetExpr = nullptr;
	for (const TObjectPtr<UMaterialExpression>& Expr : Mat->GetExpressions())
	{
		if (Expr && Expr->GetName() == ExprName)
		{
			TargetExpr = Expr;
			break;
		}
	}

	if (!TargetExpr)
	{
		TArray<FString> AvailableNames;
		for (const TObjectPtr<UMaterialExpression>& E : Mat->GetExpressions())
		{
			if (E) AvailableNames.Add(E->GetName());
		}
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Expression '%s' not found. Available: %s"),
			*ExprName, *FString::Join(AvailableNames, TEXT(", "))));
	}

	FProperty* Prop = TargetExpr->GetClass()->FindPropertyByName(*PropName);
	if (!Prop)
	{
		TArray<FString> AvailableProps;
		for (TFieldIterator<FProperty> PropIt(TargetExpr->GetClass()); PropIt; ++PropIt)
		{
			if (*PropIt && !(*PropIt)->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
			{
				AvailableProps.Add((*PropIt)->GetName());
			}
		}
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Property '%s' not found on expression '%s' (%s). Available: %s"),
			*PropName, *ExprName, *TargetExpr->GetClass()->GetName(),
			*FString::Join(AvailableProps, TEXT(", "))));
	}

	GEditor->BeginTransaction(FText::FromString(TEXT("SetExpressionProperty")));
	TargetExpr->Modify();

	// Handle numeric types directly, everything else via ImportText
	bool bSuccess = false;

	if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
	{
		float Val = FCString::Atof(*ValueStr);
		FloatProp->SetPropertyValue_InContainer(TargetExpr, Val);
		bSuccess = true;
	}
	else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
	{
		double Val = FCString::Atod(*ValueStr);
		DoubleProp->SetPropertyValue_InContainer(TargetExpr, Val);
		bSuccess = true;
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
	{
		int32 Val = FCString::Atoi(*ValueStr);
		IntProp->SetPropertyValue_InContainer(TargetExpr, Val);
		bSuccess = true;
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
	{
		bool Val = ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr == TEXT("1");
		BoolProp->SetPropertyValue_InContainer(TargetExpr, Val);
		bSuccess = true;
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
	{
		NameProp->SetPropertyValue_InContainer(TargetExpr, FName(*ValueStr));
		bSuccess = true;
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
	{
		StrProp->SetPropertyValue_InContainer(TargetExpr, ValueStr);
		bSuccess = true;
	}
	else
	{
		// Generic ImportText for structs, enums, object references, etc.
		void* PropAddr = Prop->ContainerPtrToValuePtr<void>(TargetExpr);
		bSuccess = Prop->ImportText_Direct(*ValueStr, PropAddr, TargetExpr, PPF_None) != nullptr;
	}

	if (bSuccess)
	{
		// Pass the actual property so PostEditChangePropertyInternal calls
		// MaterialGraph->RebuildGraph() and the editor display updates correctly.
		FPropertyChangedEvent ChangeEvent(Prop);
		Mat->PreEditChange(Prop);
		Mat->PostEditChangeProperty(ChangeEvent);
	}

	GEditor->EndTransaction();

	if (!bSuccess)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to set property '%s' to '%s' on expression '%s'"), *PropName, *ValueStr, *ExprName));
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetStringField(TEXT("expression_name"), ExprName);
	ResultJson->SetStringField(TEXT("property_name"), PropName);
	ResultJson->SetStringField(TEXT("value"), ValueStr);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: connect_expressions
// Params: { "asset_path", "from_expression", "from_output"?, "to_expression"?, "to_input"?, "to_property"? }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::ConnectExpressions(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString FromExprName = Params->GetStringField(TEXT("from_expression"));
	FString FromOutput = Params->HasField(TEXT("from_output")) ? Params->GetStringField(TEXT("from_output")) : TEXT("");
	FString ToExprName = Params->HasField(TEXT("to_expression")) ? Params->GetStringField(TEXT("to_expression")) : TEXT("");
	FString ToInput = Params->HasField(TEXT("to_input")) ? Params->GetStringField(TEXT("to_input")) : TEXT("");
	FString ToProperty = Params->HasField(TEXT("to_property")) ? Params->GetStringField(TEXT("to_property")) : TEXT("");

	if (ToExprName.IsEmpty() && ToProperty.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("Must provide either to_expression or to_property"));
	}

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	// Find source expression (and optionally target expression)
	UMaterialExpression* FromExpr = nullptr;
	UMaterialExpression* ToExpr = nullptr;
	for (const TObjectPtr<UMaterialExpression>& Expr : Mat->GetExpressions())
	{
		if (Expr)
		{
			if (Expr->GetName() == FromExprName) FromExpr = Expr;
			if (!ToExprName.IsEmpty() && Expr->GetName() == ToExprName) ToExpr = Expr;
		}
	}

	if (!FromExpr)
	{
		TArray<FString> AvailableNames;
		for (const TObjectPtr<UMaterialExpression>& Expr : Mat->GetExpressions())
		{
			if (Expr) AvailableNames.Add(Expr->GetName());
		}
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Source expression '%s' not found. Available: %s"),
			*FromExprName, *FString::Join(AvailableNames, TEXT(", "))));
	}

	GEditor->BeginTransaction(FText::FromString(TEXT("ConnectExpressions")));
	Mat->Modify();

	bool bConnected = false;
	FString ConnectionDesc;

	TArray<FString> Warnings;

	if (!ToProperty.IsEmpty())
	{
		// Blend mode validation: warn about irrelevant material output connections
		EMaterialProperty MatProp = ParseMaterialProperty(ToProperty);
		if (MatProp != MP_MAX)
		{
			EBlendMode BM = Mat->BlendMode;
			if (MatProp == MP_Opacity && (BM == BLEND_Opaque || BM == BLEND_Masked))
				Warnings.Add(TEXT("Opacity has no effect on Opaque/Masked materials. Set blend mode to Translucent/Additive first."));
			if (MatProp == MP_OpacityMask && BM != BLEND_Masked)
				Warnings.Add(TEXT("OpacityMask only affects Masked blend mode."));
			if (MatProp == MP_Refraction && BM == BLEND_Opaque)
				Warnings.Add(TEXT("Refraction has no effect on Opaque materials."));
		}

		// Connect to material output property
		bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(FromExpr, FromOutput, MatProp);
		ConnectionDesc = FString::Printf(TEXT("%s -> %s"), *FromExprName, *ToProperty);
	}
	else if (ToExpr)
	{
		// Connect expression to expression
		bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(FromExpr, FromOutput, ToExpr, ToInput);
		ConnectionDesc = FString::Printf(TEXT("%s -> %s"), *FromExprName, *ToExprName);
	}
	else
	{
		GEditor->EndTransaction();
		TArray<FString> AvailableNames;
		for (const TObjectPtr<UMaterialExpression>& Expr : Mat->GetExpressions())
		{
			if (Expr) AvailableNames.Add(Expr->GetName());
		}
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Target expression '%s' not found. Available: %s"),
			*ToExprName, *FString::Join(AvailableNames, TEXT(", "))));
	}

	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	GEditor->EndTransaction();

	if (!bConnected)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Connection failed: %s"), *ConnectionDesc));
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetStringField(TEXT("connection"), ConnectionDesc);
	ResultJson->SetBoolField(TEXT("connected"), true);
	if (Warnings.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> WarnArr;
		for (const FString& W : Warnings) WarnArr.Add(MakeShared<FJsonValueString>(W));
		ResultJson->SetArrayField(TEXT("warnings"), WarnArr);
	}

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Wave 3 — Free Wins
// ============================================================================

// ============================================================================
// Action: auto_layout
// Params: { "asset_path": "..." }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::AutoLayout(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!LoadedAsset)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load asset at '%s'"), *AssetPath));
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);

	// Try as UMaterialFunction first (covers MaterialLayer, MaterialLayerBlend too)
	UMaterialFunction* MatFunc = Cast<UMaterialFunction>(LoadedAsset);
	if (MatFunc)
	{
		int32 ExprCount = MatFunc->GetExpressions().Num();
		UMaterialEditingLibrary::LayoutMaterialFunctionExpressions(MatFunc);
		ResultJson->SetStringField(TEXT("type"), TEXT("MaterialFunction"));
		ResultJson->SetNumberField(TEXT("expression_count"), ExprCount);
		return FMonolithActionResult::Success(ResultJson);
	}

	// Try as UMaterial
	UMaterial* Mat = Cast<UMaterial>(LoadedAsset);
	if (Mat)
	{
		int32 ExprCount = Mat->GetExpressions().Num();
		UMaterialEditingLibrary::LayoutMaterialExpressions(Mat);
		ResultJson->SetStringField(TEXT("type"), TEXT("Material"));
		ResultJson->SetNumberField(TEXT("expression_count"), ExprCount);
		return FMonolithActionResult::Success(ResultJson);
	}

	return FMonolithActionResult::Error(FString::Printf(TEXT("Asset '%s' is not a Material or MaterialFunction"), *AssetPath));
}

// ============================================================================
// Action: duplicate_expression
// Params: { "asset_path", "expression_name", "offset_x"?, "offset_y"? }
// Note: Output connections are NOT duplicated (input connections are preserved) by UMaterialEditingLibrary::DuplicateMaterialExpression
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::DuplicateExpression(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ExprName = Params->GetStringField(TEXT("expression_name"));
	int32 OffsetX = Params->HasField(TEXT("offset_x")) ? static_cast<int32>(Params->GetNumberField(TEXT("offset_x"))) : 50;
	int32 OffsetY = Params->HasField(TEXT("offset_y")) ? static_cast<int32>(Params->GetNumberField(TEXT("offset_y"))) : 50;

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	// Find source expression
	UMaterialExpression* SourceExpr = nullptr;
	for (const TObjectPtr<UMaterialExpression>& Expr : Mat->GetExpressions())
	{
		if (Expr && Expr->GetName() == ExprName)
		{
			SourceExpr = Expr;
			break;
		}
	}

	if (!SourceExpr)
	{
		TArray<FString> AvailableNames;
		for (const TObjectPtr<UMaterialExpression>& Expr : Mat->GetExpressions())
		{
			if (Expr) AvailableNames.Add(Expr->GetName());
		}
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Expression '%s' not found. Available: %s"),
			*ExprName, *FString::Join(AvailableNames, TEXT(", "))));
	}

	GEditor->BeginTransaction(FText::FromString(TEXT("DuplicateExpression")));
	Mat->Modify();

	UMaterialExpression* NewExpr = UMaterialEditingLibrary::DuplicateMaterialExpression(Mat, nullptr, SourceExpr);
	if (!NewExpr)
	{
		GEditor->EndTransaction();
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to duplicate expression '%s'"), *ExprName));
	}

	// Apply position offset
	NewExpr->MaterialExpressionEditorX = SourceExpr->MaterialExpressionEditorX + OffsetX;
	NewExpr->MaterialExpressionEditorY = SourceExpr->MaterialExpressionEditorY + OffsetY;

	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	GEditor->EndTransaction();

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetStringField(TEXT("source_name"), ExprName);
	ResultJson->SetStringField(TEXT("new_name"), NewExpr->GetName());
	ResultJson->SetStringField(TEXT("class"), NewExpr->GetClass()->GetName());
	ResultJson->SetNumberField(TEXT("pos_x"), NewExpr->MaterialExpressionEditorX);
	ResultJson->SetNumberField(TEXT("pos_y"), NewExpr->MaterialExpressionEditorY);
	ResultJson->SetStringField(TEXT("note"), TEXT("Output connections are NOT duplicated (input connections are preserved) — only the expression node itself is copied"));

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: list_expression_classes
// Params: { "filter"?, "category"? }
// Cached in a static TMap — first call creates temp instances (~1-2s), subsequent calls instant.
// ============================================================================

namespace
{
	struct FExpressionClassInfo
	{
		FString ClassName;
		FString DisplayName;
		FString MenuCategories;
		bool bIsParameter;
		int32 InputCount;
		int32 OutputCount;
	};

	static TArray<FExpressionClassInfo> GCachedExpressionClasses;
	static bool GExpressionClassesCached = false;

	void EnsureExpressionClassCache()
	{
		if (GExpressionClassesCached)
		{
			return;
		}

		TArray<UClass*> DerivedClasses;
		GetDerivedClasses(UMaterialExpression::StaticClass(), DerivedClasses, true);

		for (UClass* Class : DerivedClasses)
		{
			if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_Hidden))
			{
				continue;
			}

			// Create temporary instance to read pin info
			UMaterialExpression* TempExpr = NewObject<UMaterialExpression>(GetTransientPackage(), Class, NAME_None, RF_Transient);
			if (!TempExpr)
			{
				continue;
			}

			FExpressionClassInfo Info;
			Info.ClassName = Class->GetName();
			Info.DisplayName = Class->GetDisplayNameText().ToString();
			Info.bIsParameter = Class->IsChildOf(UMaterialExpressionParameter::StaticClass())
				|| Class->IsChildOf(UMaterialExpressionTextureSampleParameter::StaticClass());

			// Read keywords from the temp instance
			Info.MenuCategories = TempExpr->GetKeywords().ToString();

			// Count inputs
			Info.InputCount = 0;
			for (int32 i = 0; ; ++i)
			{
				if (!TempExpr->GetInput(i))
				{
					break;
				}
				Info.InputCount++;
			}

			// Count outputs
			Info.OutputCount = TempExpr->Outputs.Num();

			GCachedExpressionClasses.Add(MoveTemp(Info));

			TempExpr->MarkAsGarbage();
		}

		// Sort alphabetically
		GCachedExpressionClasses.Sort([](const FExpressionClassInfo& A, const FExpressionClassInfo& B)
		{
			return A.ClassName < B.ClassName;
		});

		GExpressionClassesCached = true;
	}
}

FMonolithActionResult FMonolithMaterialActions::ListExpressionClasses(const TSharedPtr<FJsonObject>& Params)
{
	FString Filter = Params->HasField(TEXT("filter")) ? Params->GetStringField(TEXT("filter")) : TEXT("");
	FString Category = Params->HasField(TEXT("category")) ? Params->GetStringField(TEXT("category")) : TEXT("");

	EnsureExpressionClassCache();

	TArray<TSharedPtr<FJsonValue>> ClassesArray;
	for (const FExpressionClassInfo& Info : GCachedExpressionClasses)
	{
		// Apply filter
		if (!Filter.IsEmpty() && !Info.ClassName.Contains(Filter, ESearchCase::IgnoreCase)
			&& !Info.DisplayName.Contains(Filter, ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (!Category.IsEmpty() && !Info.MenuCategories.Contains(Category, ESearchCase::IgnoreCase))
		{
			continue;
		}

		auto ClassJson = MakeShared<FJsonObject>();
		ClassJson->SetStringField(TEXT("class_name"), Info.ClassName);
		ClassJson->SetStringField(TEXT("display_name"), Info.DisplayName);
		ClassJson->SetBoolField(TEXT("is_parameter"), Info.bIsParameter);
		ClassJson->SetNumberField(TEXT("input_count"), Info.InputCount);
		ClassJson->SetNumberField(TEXT("output_count"), Info.OutputCount);
		if (!Info.MenuCategories.IsEmpty())
		{
			ClassJson->SetStringField(TEXT("keywords"), Info.MenuCategories);
		}
		ClassesArray.Add(MakeShared<FJsonValueObject>(ClassJson));
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetNumberField(TEXT("total_classes"), GCachedExpressionClasses.Num());
	ResultJson->SetNumberField(TEXT("filtered_count"), ClassesArray.Num());
	ResultJson->SetArrayField(TEXT("classes"), ClassesArray);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: get_expression_connections
// Params: { "asset_path", "expression_name" }
// Returns inputs (what feeds into this node) and output consumers (what this node feeds)
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::GetExpressionConnections(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ExprName = Params->GetStringField(TEXT("expression_name"));

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	// Find target expression
	UMaterialExpression* TargetExpr = nullptr;
	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Mat->GetExpressions();
	for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
	{
		if (Expr && Expr->GetName() == ExprName)
		{
			TargetExpr = Expr;
			break;
		}
	}

	if (!TargetExpr)
	{
		TArray<FString> AvailableNames;
		for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
		{
			if (Expr) AvailableNames.Add(Expr->GetName());
		}
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Expression '%s' not found. Available: %s"),
			*ExprName, *FString::Join(AvailableNames, TEXT(", "))));
	}

	// --- Inputs: what feeds into this expression ---
	TArray<TSharedPtr<FJsonValue>> InputsArray;
	for (int32 i = 0; ; ++i)
	{
		FExpressionInput* Input = TargetExpr->GetInput(i);
		if (!Input)
		{
			break;
		}
		auto InputJson = MakeShared<FJsonObject>();
		InputJson->SetNumberField(TEXT("index"), i);
		InputJson->SetStringField(TEXT("name"), TargetExpr->GetInputName(i).ToString());
		InputJson->SetBoolField(TEXT("connected"), Input->Expression != nullptr);
		if (Input->Expression)
		{
			InputJson->SetStringField(TEXT("connected_to"), Input->Expression->GetName());
			InputJson->SetNumberField(TEXT("output_index"), Input->OutputIndex);
			// Include output name if available
			const TArray<FExpressionOutput>& SourceOutputs = Input->Expression->Outputs;
			if (SourceOutputs.IsValidIndex(Input->OutputIndex))
			{
				InputJson->SetStringField(TEXT("output_name"), SourceOutputs[Input->OutputIndex].OutputName.ToString());
			}
		}
		InputsArray.Add(MakeShared<FJsonValueObject>(InputJson));
	}

	// --- Outputs: what consumes this expression ---
	// Build per-output-index consumer lists
	TMap<int32, TArray<TSharedPtr<FJsonValue>>> OutputConsumers;
	// Initialize with all known output indices
	for (int32 i = 0; i < TargetExpr->Outputs.Num(); ++i)
	{
		OutputConsumers.FindOrAdd(i);
	}

	// Scan all expressions for inputs referencing TargetExpr
	for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
	{
		if (!Expr)
		{
			continue;
		}
		for (int32 i = 0; ; ++i)
		{
			FExpressionInput* Input = Expr->GetInput(i);
			if (!Input)
			{
				break;
			}
			if (Input->Expression == TargetExpr)
			{
				auto ConsumerJson = MakeShared<FJsonObject>();
				ConsumerJson->SetStringField(TEXT("expression"), Expr->GetName());
				ConsumerJson->SetStringField(TEXT("input_name"), Expr->GetInputName(i).ToString());
				ConsumerJson->SetNumberField(TEXT("input_index"), i);
				OutputConsumers.FindOrAdd(Input->OutputIndex).Add(MakeShared<FJsonValueObject>(ConsumerJson));
			}
		}
	}

	// Also scan material output slots
	for (const FMaterialOutputEntry& Entry : MaterialOutputEntries)
	{
		FExpressionInput* Input = Mat->GetExpressionInputForProperty(Entry.Property);
		if (Input && Input->Expression == TargetExpr)
		{
			auto ConsumerJson = MakeShared<FJsonObject>();
			ConsumerJson->SetStringField(TEXT("material_output"), Entry.Name);
			OutputConsumers.FindOrAdd(Input->OutputIndex).Add(MakeShared<FJsonValueObject>(ConsumerJson));
		}
	}

	// Build outputs array
	TArray<TSharedPtr<FJsonValue>> OutputsArray;
	for (int32 i = 0; i < TargetExpr->Outputs.Num(); ++i)
	{
		auto OutputJson = MakeShared<FJsonObject>();
		OutputJson->SetNumberField(TEXT("index"), i);
		OutputJson->SetStringField(TEXT("name"), TargetExpr->Outputs[i].OutputName.ToString());
		TArray<TSharedPtr<FJsonValue>>* Consumers = OutputConsumers.Find(i);
		OutputJson->SetArrayField(TEXT("consumers"), Consumers ? *Consumers : TArray<TSharedPtr<FJsonValue>>());
		OutputJson->SetNumberField(TEXT("consumer_count"), Consumers ? Consumers->Num() : 0);
		OutputsArray.Add(MakeShared<FJsonValueObject>(OutputJson));
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetStringField(TEXT("expression_name"), ExprName);
	ResultJson->SetStringField(TEXT("class"), TargetExpr->GetClass()->GetName());
	ResultJson->SetArrayField(TEXT("inputs"), InputsArray);
	ResultJson->SetArrayField(TEXT("outputs"), OutputsArray);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: move_expression
// Params: { "asset_path", "expression_name"?, "pos_x"?, "pos_y"?, "relative"?, "expressions"? }
// Single move: expression_name + pos_x + pos_y
// Batch move: expressions = [{name, x, y}]
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::MoveExpression(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	// Build a list of move operations
	struct FMoveOp
	{
		FString Name;
		int32 X;
		int32 Y;
	};
	TArray<FMoveOp> MoveOps;
	bool bRelative = Params->HasField(TEXT("relative")) ? Params->GetBoolField(TEXT("relative")) : false;

	const TArray<TSharedPtr<FJsonValue>>* ExpressionsArr = nullptr;
	// Handle Claude Code JSON string serialization quirk — array may arrive as string
	TSharedPtr<FJsonValue> ExpressionsField = Params->TryGetField(TEXT("expressions"));
	TArray<TSharedPtr<FJsonValue>> ParsedArray;
	if (ExpressionsField.IsValid())
	{
		if (ExpressionsField->Type == EJson::Array)
		{
			ExpressionsArr = &ExpressionsField->AsArray();
		}
		else if (ExpressionsField->Type == EJson::String)
		{
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ExpressionsField->AsString());
			TSharedPtr<FJsonValue> Parsed;
			if (FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid() && Parsed->Type == EJson::Array)
			{
				ParsedArray = Parsed->AsArray();
				ExpressionsArr = &ParsedArray;
			}
		}
	}
	if (ExpressionsArr && ExpressionsArr->Num() > 0)
	{
		for (const TSharedPtr<FJsonValue>& Val : *ExpressionsArr)
		{
			const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
			if (Val && Val->TryGetObject(ObjPtr) && ObjPtr)
			{
				FMoveOp Op;
				Op.Name = (*ObjPtr)->GetStringField(TEXT("name"));
				// Accept both "x"/"y" and "pos_x"/"pos_y"
				Op.X = static_cast<int32>((*ObjPtr)->HasField(TEXT("x")) ? (*ObjPtr)->GetNumberField(TEXT("x")) : (*ObjPtr)->GetNumberField(TEXT("pos_x")));
				Op.Y = static_cast<int32>((*ObjPtr)->HasField(TEXT("y")) ? (*ObjPtr)->GetNumberField(TEXT("y")) : (*ObjPtr)->GetNumberField(TEXT("pos_y")));
				MoveOps.Add(MoveTemp(Op));
			}
		}
	}
	else if (Params->HasField(TEXT("expression_name")))
	{
		FMoveOp Op;
		Op.Name = Params->GetStringField(TEXT("expression_name"));
		Op.X = Params->HasField(TEXT("pos_x")) ? static_cast<int32>(Params->GetNumberField(TEXT("pos_x"))) : 0;
		Op.Y = Params->HasField(TEXT("pos_y")) ? static_cast<int32>(Params->GetNumberField(TEXT("pos_y"))) : 0;
		MoveOps.Add(MoveTemp(Op));
	}
	else
	{
		return FMonolithActionResult::Error(TEXT("Must provide either 'expression_name' or 'expressions' array"));
	}

	if (MoveOps.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("No move operations specified"));
	}

	// Build name -> expression lookup
	TMap<FString, UMaterialExpression*> NameToExpr;
	for (const TObjectPtr<UMaterialExpression>& Expr : Mat->GetExpressions())
	{
		if (Expr)
		{
			NameToExpr.Add(Expr->GetName(), Expr);
		}
	}

	GEditor->BeginTransaction(FText::FromString(TEXT("MoveExpression")));
	Mat->Modify();

	TArray<TSharedPtr<FJsonValue>> MovedArray;
	TArray<FString> NotFound;

	for (const FMoveOp& Op : MoveOps)
	{
		UMaterialExpression** FoundPtr = NameToExpr.Find(Op.Name);
		if (!FoundPtr || !*FoundPtr)
		{
			NotFound.Add(Op.Name);
			continue;
		}

		UMaterialExpression* Expr = *FoundPtr;
		if (bRelative)
		{
			Expr->MaterialExpressionEditorX += Op.X;
			Expr->MaterialExpressionEditorY += Op.Y;
		}
		else
		{
			Expr->MaterialExpressionEditorX = Op.X;
			Expr->MaterialExpressionEditorY = Op.Y;
		}

		auto MovedJson = MakeShared<FJsonObject>();
		MovedJson->SetStringField(TEXT("name"), Op.Name);
		MovedJson->SetNumberField(TEXT("x"), Expr->MaterialExpressionEditorX);
		MovedJson->SetNumberField(TEXT("y"), Expr->MaterialExpressionEditorY);
		MovedArray.Add(MakeShared<FJsonValueObject>(MovedJson));
	}

	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	GEditor->EndTransaction();

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetNumberField(TEXT("moved_count"), MovedArray.Num());
	ResultJson->SetArrayField(TEXT("moved"), MovedArray);
	if (NotFound.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> NotFoundArr;
		for (const FString& Name : NotFound) NotFoundArr.Add(MakeShared<FJsonValueString>(Name));
		ResultJson->SetArrayField(TEXT("not_found"), NotFoundArr);
	}

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: get_material_properties
// Params: { "asset_path": "..." }
// Works on both UMaterial and UMaterialInstance (loaded as UMaterialInterface)
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::GetMaterialProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	UMaterialInterface* MatInterface = LoadedAsset ? Cast<UMaterialInterface>(LoadedAsset) : nullptr;
	if (!MatInterface)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load material at '%s'"), *AssetPath));
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetStringField(TEXT("asset_class"), MatInterface->GetClass()->GetName());

	// Get blend mode
	EBlendMode BlendMode = MatInterface->GetBlendMode();
	const UEnum* BlendEnum = StaticEnum<EBlendMode>();
	ResultJson->SetStringField(TEXT("blend_mode"), BlendEnum->GetNameStringByIndex(static_cast<int32>(BlendMode)));

	// Get shading model(s)
	FMaterialShadingModelField ShadingModels = MatInterface->GetShadingModels();
	EMaterialShadingModel FirstModel = ShadingModels.GetFirstShadingModel();
	const UEnum* ShadingEnum = StaticEnum<EMaterialShadingModel>();
	ResultJson->SetStringField(TEXT("shading_model"), ShadingEnum->GetNameStringByIndex(static_cast<int32>(FirstModel)));

	// If multiple shading models, list them all
	if (ShadingModels.CountShadingModels() > 1)
	{
		TArray<TSharedPtr<FJsonValue>> ModelsArr;
		for (int32 i = 0; i < static_cast<int32>(MSM_NUM); ++i)
		{
			if (ShadingModels.HasShadingModel(static_cast<EMaterialShadingModel>(i)))
			{
				ModelsArr.Add(MakeShared<FJsonValueString>(ShadingEnum->GetNameStringByIndex(i)));
			}
		}
		ResultJson->SetArrayField(TEXT("shading_models"), ModelsArr);
	}

	// Material domain — only available on base UMaterial
	UMaterial* BaseMat = MatInterface->GetMaterial();
	if (BaseMat)
	{
		const UEnum* DomainEnum = StaticEnum<EMaterialDomain>();
		ResultJson->SetStringField(TEXT("material_domain"), DomainEnum->GetNameStringByIndex(static_cast<int32>(BaseMat->MaterialDomain)));
		ResultJson->SetBoolField(TEXT("two_sided"), BaseMat->TwoSided != 0);
		ResultJson->SetNumberField(TEXT("opacity_mask_clip_value"), BaseMat->OpacityMaskClipValue);
		ResultJson->SetBoolField(TEXT("dithered_lod_transition"), BaseMat->DitheredLODTransition != 0);
		ResultJson->SetNumberField(TEXT("expression_count"), BaseMat->GetExpressions().Num());
	}

	// Is it translucent?
	ResultJson->SetBoolField(TEXT("is_translucent"), IsTranslucentBlendMode(BlendMode));
	ResultJson->SetBoolField(TEXT("is_masked"), BlendMode == BLEND_Masked);

	// Usage flags
	auto UsageJson = MakeShared<FJsonObject>();
	if (BaseMat)
	{
		UsageJson->SetBoolField(TEXT("skeletal_mesh"), BaseMat->GetUsageByFlag(MATUSAGE_SkeletalMesh));
		UsageJson->SetBoolField(TEXT("particle_sprites"), BaseMat->GetUsageByFlag(MATUSAGE_ParticleSprites));
		UsageJson->SetBoolField(TEXT("beam_trails"), BaseMat->GetUsageByFlag(MATUSAGE_BeamTrails));
		UsageJson->SetBoolField(TEXT("mesh_particles"), BaseMat->GetUsageByFlag(MATUSAGE_MeshParticles));
		UsageJson->SetBoolField(TEXT("static_lighting"), BaseMat->GetUsageByFlag(MATUSAGE_StaticLighting));
		UsageJson->SetBoolField(TEXT("morph_targets"), BaseMat->GetUsageByFlag(MATUSAGE_MorphTargets));
		UsageJson->SetBoolField(TEXT("spline_mesh"), BaseMat->GetUsageByFlag(MATUSAGE_SplineMesh));
		UsageJson->SetBoolField(TEXT("instanced_static_meshes"), BaseMat->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));
		UsageJson->SetBoolField(TEXT("niagara_sprites"), BaseMat->GetUsageByFlag(MATUSAGE_NiagaraSprites));
		UsageJson->SetBoolField(TEXT("niagara_meshes"), BaseMat->GetUsageByFlag(MATUSAGE_NiagaraMeshParticles));
		UsageJson->SetBoolField(TEXT("niagara_ribbons"), BaseMat->GetUsageByFlag(MATUSAGE_NiagaraRibbons));
		UsageJson->SetBoolField(TEXT("virtual_heightfield_mesh"), BaseMat->GetUsageByFlag(MATUSAGE_VirtualHeightfieldMesh));
	}
	ResultJson->SetObjectField(TEXT("usage_flags"), UsageJson);

	// Check if this is a material instance — report parent
	UMaterialInstance* MatInst = Cast<UMaterialInstance>(MatInterface);
	if (MatInst && MatInst->Parent)
	{
		ResultJson->SetStringField(TEXT("parent_material"), MatInst->Parent->GetPathName());
		ResultJson->SetBoolField(TEXT("is_instance"), true);
	}
	else
	{
		ResultJson->SetBoolField(TEXT("is_instance"), false);
	}

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: get_instance_parameters
// Reads all parameter overrides from a MIC with override detection vs parent
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::GetInstanceParameters(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	UMaterialInstanceConstant* MIC = LoadedAsset ? Cast<UMaterialInstanceConstant>(LoadedAsset) : nullptr;
	if (!MIC)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load material instance at '%s'"), *AssetPath));
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);

	// Parent info
	if (MIC->Parent)
	{
		ResultJson->SetStringField(TEXT("parent"), MIC->Parent->GetPathName());
	}

	// Scalar parameters
	TArray<TSharedPtr<FJsonValue>> ScalarArr;
	for (const auto& Param : MIC->ScalarParameterValues)
	{
		auto ParamJson = MakeShared<FJsonObject>();
		ParamJson->SetStringField(TEXT("name"), Param.ParameterInfo.Name.ToString());
		ParamJson->SetNumberField(TEXT("value"), Param.ParameterValue);
		ParamJson->SetBoolField(TEXT("is_overridden"), true); // present in array = overridden
		ScalarArr.Add(MakeShared<FJsonValueObject>(ParamJson));
	}
	ResultJson->SetArrayField(TEXT("scalar"), ScalarArr);

	// Vector parameters
	TArray<TSharedPtr<FJsonValue>> VectorArr;
	for (const auto& Param : MIC->VectorParameterValues)
	{
		auto ParamJson = MakeShared<FJsonObject>();
		ParamJson->SetStringField(TEXT("name"), Param.ParameterInfo.Name.ToString());
		auto ColorJson = MakeShared<FJsonObject>();
		ColorJson->SetNumberField(TEXT("R"), Param.ParameterValue.R);
		ColorJson->SetNumberField(TEXT("G"), Param.ParameterValue.G);
		ColorJson->SetNumberField(TEXT("B"), Param.ParameterValue.B);
		ColorJson->SetNumberField(TEXT("A"), Param.ParameterValue.A);
		ParamJson->SetObjectField(TEXT("value"), ColorJson);
		ParamJson->SetBoolField(TEXT("is_overridden"), true);
		VectorArr.Add(MakeShared<FJsonValueObject>(ParamJson));
	}
	ResultJson->SetArrayField(TEXT("vector"), VectorArr);

	// Texture parameters
	TArray<TSharedPtr<FJsonValue>> TextureArr;
	for (const auto& Param : MIC->TextureParameterValues)
	{
		auto ParamJson = MakeShared<FJsonObject>();
		ParamJson->SetStringField(TEXT("name"), Param.ParameterInfo.Name.ToString());
		ParamJson->SetStringField(TEXT("value"), Param.ParameterValue ? Param.ParameterValue->GetPathName() : TEXT("None"));
		ParamJson->SetBoolField(TEXT("is_overridden"), true);
		TextureArr.Add(MakeShared<FJsonValueObject>(ParamJson));
	}
	ResultJson->SetArrayField(TEXT("texture"), TextureArr);

	// Static switch parameters — use GetAllStaticSwitchParameterInfo (proven API)
	TArray<TSharedPtr<FJsonValue>> SwitchArr;
	{
		TArray<FMaterialParameterInfo> SwitchInfos;
		TArray<FGuid> SwitchGuids;
		MIC->GetAllStaticSwitchParameterInfo(SwitchInfos, SwitchGuids);

		// Also get parent values to detect overrides
		for (int32 i = 0; i < SwitchInfos.Num(); ++i)
		{
			bool Value = false;
			FGuid OutGuid;
			MIC->GetStaticSwitchParameterValue(SwitchInfos[i], Value, OutGuid);

			// Check if overridden by comparing to parent
			bool bIsOverridden = false;
			if (MIC->Parent)
			{
				bool ParentValue = false;
				FGuid ParentGuid;
				if (MIC->Parent->GetStaticSwitchParameterValue(SwitchInfos[i], ParentValue, ParentGuid))
				{
					bIsOverridden = (Value != ParentValue);
				}
			}

			auto ParamJson = MakeShared<FJsonObject>();
			ParamJson->SetStringField(TEXT("name"), SwitchInfos[i].Name.ToString());
			ParamJson->SetBoolField(TEXT("value"), Value);
			ParamJson->SetBoolField(TEXT("is_overridden"), bIsOverridden);
			SwitchArr.Add(MakeShared<FJsonValueObject>(ParamJson));
		}
	}
	ResultJson->SetArrayField(TEXT("static_switch"), SwitchArr);

	ResultJson->SetNumberField(TEXT("total_overrides"),
		ScalarArr.Num() + VectorArr.Num() + TextureArr.Num() + SwitchArr.Num());

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: set_instance_parameters (batch)
// Sets multiple parameter overrides in one call with a single update
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::SetInstanceParameters(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	UMaterialInstanceConstant* MIC = LoadedAsset ? Cast<UMaterialInstanceConstant>(LoadedAsset) : nullptr;
	if (!MIC)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load material instance at '%s'"), *AssetPath));
	}

	const TArray<TSharedPtr<FJsonValue>>* ParamArray = nullptr;
	if (!Params->TryGetArrayField(TEXT("parameters"), ParamArray) || !ParamArray)
	{
		return FMonolithActionResult::Error(TEXT("Missing required 'parameters' array"));
	}

	GEditor->BeginTransaction(FText::FromString(TEXT("SetInstanceParameters")));
	MIC->Modify();

	int32 SetCount = 0;
	bool bHasStaticSwitches = false;
	TArray<TSharedPtr<FJsonValue>> ErrorsArr;

	for (const auto& ParamVal : *ParamArray)
	{
		const TSharedPtr<FJsonObject>* ParamObj = nullptr;
		if (!ParamVal->TryGetObject(ParamObj) || !ParamObj)
		{
			auto ErrJson = MakeShared<FJsonObject>();
			ErrJson->SetStringField(TEXT("error"), TEXT("Array element is not a JSON object"));
			ErrorsArr.Add(MakeShared<FJsonValueObject>(ErrJson));
			continue;
		}

		FString Name = (*ParamObj)->GetStringField(TEXT("name"));
		FString Type = (*ParamObj)->GetStringField(TEXT("type"));
		FMaterialParameterInfo ParamInfo(*Name);

		if (Name.IsEmpty() || Type.IsEmpty())
		{
			auto ErrJson = MakeShared<FJsonObject>();
			ErrJson->SetStringField(TEXT("error"), TEXT("Parameter missing 'name' or 'type'"));
			ErrorsArr.Add(MakeShared<FJsonValueObject>(ErrJson));
			continue;
		}

		if (Type.Equals(TEXT("scalar"), ESearchCase::IgnoreCase))
		{
			double NumVal = 0.0;
			if ((*ParamObj)->TryGetNumberField(TEXT("value"), NumVal))
			{
				MIC->SetScalarParameterValueEditorOnly(ParamInfo, static_cast<float>(NumVal));
				SetCount++;
			}
			else
			{
				auto ErrJson = MakeShared<FJsonObject>();
				ErrJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Scalar param '%s': missing or invalid 'value'"), *Name));
				ErrorsArr.Add(MakeShared<FJsonValueObject>(ErrJson));
			}
		}
		else if (Type.Equals(TEXT("vector"), ESearchCase::IgnoreCase))
		{
			const TSharedPtr<FJsonObject>* ColorObj = nullptr;
			if ((*ParamObj)->TryGetObjectField(TEXT("value"), ColorObj))
			{
				FLinearColor Color;
				Color.R = (*ColorObj)->HasField(TEXT("R")) ? static_cast<float>((*ColorObj)->GetNumberField(TEXT("R"))) : 0.f;
				Color.G = (*ColorObj)->HasField(TEXT("G")) ? static_cast<float>((*ColorObj)->GetNumberField(TEXT("G"))) : 0.f;
				Color.B = (*ColorObj)->HasField(TEXT("B")) ? static_cast<float>((*ColorObj)->GetNumberField(TEXT("B"))) : 0.f;
				Color.A = (*ColorObj)->HasField(TEXT("A")) ? static_cast<float>((*ColorObj)->GetNumberField(TEXT("A"))) : 1.f;
				MIC->SetVectorParameterValueEditorOnly(ParamInfo, Color);
				SetCount++;
			}
			else
			{
				auto ErrJson = MakeShared<FJsonObject>();
				ErrJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Vector param '%s': missing or invalid 'value' object"), *Name));
				ErrorsArr.Add(MakeShared<FJsonValueObject>(ErrJson));
			}
		}
		else if (Type.Equals(TEXT("texture"), ESearchCase::IgnoreCase))
		{
			FString TexPath = (*ParamObj)->GetStringField(TEXT("value"));
			UTexture* Tex = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(TexPath));
			if (Tex)
			{
				MIC->SetTextureParameterValueEditorOnly(ParamInfo, Tex);
				SetCount++;
			}
			else
			{
				auto ErrJson = MakeShared<FJsonObject>();
				ErrJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Texture param '%s': failed to load texture at '%s'"), *Name, *TexPath));
				ErrorsArr.Add(MakeShared<FJsonValueObject>(ErrJson));
			}
		}
		else if (Type.Equals(TEXT("switch"), ESearchCase::IgnoreCase))
		{
			bool bValue = false;
			if ((*ParamObj)->TryGetBoolField(TEXT("value"), bValue))
			{
				MIC->SetStaticSwitchParameterValueEditorOnly(ParamInfo, bValue);
				bHasStaticSwitches = true;
				SetCount++;
			}
			else
			{
				auto ErrJson = MakeShared<FJsonObject>();
				ErrJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Switch param '%s': missing or invalid 'value' bool"), *Name));
				ErrorsArr.Add(MakeShared<FJsonValueObject>(ErrJson));
			}
		}
		else
		{
			auto ErrJson = MakeShared<FJsonObject>();
			ErrJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown type '%s' for param '%s'. Valid: scalar, vector, texture, switch"), *Type, *Name));
			ErrorsArr.Add(MakeShared<FJsonValueObject>(ErrJson));
		}
	}

	// Single update call at the end — handles static permutation recompile if needed
	if (SetCount > 0)
	{
		UMaterialEditingLibrary::UpdateMaterialInstance(MIC);
	}

	MIC->MarkPackageDirty();
	GEditor->EndTransaction();

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetNumberField(TEXT("set_count"), SetCount);
	if (ErrorsArr.Num() > 0)
	{
		ResultJson->SetArrayField(TEXT("errors"), ErrorsArr);
	}

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: set_instance_parent
// Reparent a material instance, reporting lost/kept parameters
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::SetInstanceParent(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString NewParentPath = Params->GetStringField(TEXT("new_parent"));

	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	UMaterialInstanceConstant* MIC = LoadedAsset ? Cast<UMaterialInstanceConstant>(LoadedAsset) : nullptr;
	if (!MIC)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load material instance at '%s'"), *AssetPath));
	}

	UObject* NewParentObj = UEditorAssetLibrary::LoadAsset(NewParentPath);
	UMaterialInterface* NewParent = NewParentObj ? Cast<UMaterialInterface>(NewParentObj) : nullptr;
	if (!NewParent)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load new parent material at '%s'"), *NewParentPath));
	}

	// Snapshot current override names before reparenting
	TSet<FString> OldParamNames;
	for (const auto& P : MIC->ScalarParameterValues) OldParamNames.Add(P.ParameterInfo.Name.ToString());
	for (const auto& P : MIC->VectorParameterValues) OldParamNames.Add(P.ParameterInfo.Name.ToString());
	for (const auto& P : MIC->TextureParameterValues) OldParamNames.Add(P.ParameterInfo.Name.ToString());
	// Also snapshot static switch names
	{
		TArray<FMaterialParameterInfo> SwitchInfos;
		TArray<FGuid> SwitchGuids;
		MIC->GetAllStaticSwitchParameterInfo(SwitchInfos, SwitchGuids);
		for (const auto& Info : SwitchInfos) OldParamNames.Add(Info.Name.ToString());
	}

	FString OldParentPath = MIC->Parent ? MIC->Parent->GetPathName() : TEXT("None");

	GEditor->BeginTransaction(FText::FromString(TEXT("SetInstanceParent")));
	MIC->Modify();
	MIC->SetParentEditorOnly(NewParent, true);
	UMaterialEditingLibrary::UpdateMaterialInstance(MIC);
	MIC->MarkPackageDirty();
	GEditor->EndTransaction();

	// After reparenting, check which parameters from the new parent exist
	// to determine what was kept vs lost
	TArray<TSharedPtr<FJsonValue>> KeptArr, LostArr;

	// Gather all parameter names the new parent exposes
	TSet<FString> NewParentParamNames;
	{
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Guids;
		NewParent->GetAllScalarParameterInfo(Infos, Guids);
		for (const auto& Info : Infos) NewParentParamNames.Add(Info.Name.ToString());
		Infos.Reset(); Guids.Reset();
		NewParent->GetAllVectorParameterInfo(Infos, Guids);
		for (const auto& Info : Infos) NewParentParamNames.Add(Info.Name.ToString());
		Infos.Reset(); Guids.Reset();
		NewParent->GetAllTextureParameterInfo(Infos, Guids);
		for (const auto& Info : Infos) NewParentParamNames.Add(Info.Name.ToString());
		Infos.Reset(); Guids.Reset();
		NewParent->GetAllStaticSwitchParameterInfo(Infos, Guids);
		for (const auto& Info : Infos) NewParentParamNames.Add(Info.Name.ToString());
	}

	for (const FString& Name : OldParamNames)
	{
		if (NewParentParamNames.Contains(Name))
		{
			KeptArr.Add(MakeShared<FJsonValueString>(Name));
		}
		else
		{
			LostArr.Add(MakeShared<FJsonValueString>(Name));
		}
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetStringField(TEXT("old_parent"), OldParentPath);
	ResultJson->SetStringField(TEXT("new_parent"), NewParent->GetPathName());
	ResultJson->SetArrayField(TEXT("kept_parameters"), KeptArr);
	ResultJson->SetArrayField(TEXT("lost_parameters"), LostArr);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: clear_instance_parameter
// Remove a single override (or all overrides) from a material instance
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::ClearInstanceParameter(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	UMaterialInstanceConstant* MIC = LoadedAsset ? Cast<UMaterialInstanceConstant>(LoadedAsset) : nullptr;
	if (!MIC)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load material instance at '%s'"), *AssetPath));
	}

	FString ParamName = Params->HasField(TEXT("parameter_name")) ? Params->GetStringField(TEXT("parameter_name")) : TEXT("");
	FString ParamType = Params->HasField(TEXT("parameter_type")) ? Params->GetStringField(TEXT("parameter_type")) : TEXT("all");

	GEditor->BeginTransaction(FText::FromString(TEXT("ClearInstanceParameter")));
	MIC->Modify();

	int32 ClearedCount = 0;
	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);

	if (ParamName.IsEmpty())
	{
		// Clear ALL overrides
		int32 TotalBefore = MIC->ScalarParameterValues.Num() + MIC->VectorParameterValues.Num()
			+ MIC->TextureParameterValues.Num();
		UMaterialEditingLibrary::ClearAllMaterialInstanceParameters(MIC);
		ClearedCount = TotalBefore;
		ResultJson->SetStringField(TEXT("cleared"), TEXT("all"));
	}
	else
	{
		// Clear a specific parameter by name and type
		bool bClearScalar = ParamType.Equals(TEXT("scalar"), ESearchCase::IgnoreCase) || ParamType.Equals(TEXT("all"), ESearchCase::IgnoreCase);
		bool bClearVector = ParamType.Equals(TEXT("vector"), ESearchCase::IgnoreCase) || ParamType.Equals(TEXT("all"), ESearchCase::IgnoreCase);
		bool bClearTexture = ParamType.Equals(TEXT("texture"), ESearchCase::IgnoreCase) || ParamType.Equals(TEXT("all"), ESearchCase::IgnoreCase);
		bool bClearSwitch = ParamType.Equals(TEXT("switch"), ESearchCase::IgnoreCase) || ParamType.Equals(TEXT("all"), ESearchCase::IgnoreCase);

		if (bClearScalar)
		{
			ClearedCount += MIC->ScalarParameterValues.RemoveAll([&](const FScalarParameterValue& P)
			{
				return P.ParameterInfo.Name.ToString() == ParamName;
			});
		}
		if (bClearVector)
		{
			ClearedCount += MIC->VectorParameterValues.RemoveAll([&](const FVectorParameterValue& P)
			{
				return P.ParameterInfo.Name.ToString() == ParamName;
			});
		}
		if (bClearTexture)
		{
			ClearedCount += MIC->TextureParameterValues.RemoveAll([&](const FTextureParameterValue& P)
			{
				return P.ParameterInfo.Name.ToString() == ParamName;
			});
		}
		if (bClearSwitch)
		{
			// Static switches: get params, unset the override flag, then update permutation
			FStaticParameterSet StaticParams;
			MIC->GetStaticParameterValues(StaticParams);
			bool bModifiedStatic = false;
			for (auto& SP : StaticParams.StaticSwitchParameters)
			{
				if (SP.ParameterInfo.Name.ToString() == ParamName && SP.bOverride)
				{
					SP.bOverride = false;
					bModifiedStatic = true;
					ClearedCount++;
				}
			}
			if (bModifiedStatic)
			{
				MIC->UpdateStaticPermutation(StaticParams);
			}
		}

		ResultJson->SetStringField(TEXT("cleared"), ParamName);
		ResultJson->SetStringField(TEXT("type_filter"), ParamType);
	}

	MIC->MarkPackageDirty();
	GEditor->EndTransaction();

	ResultJson->SetNumberField(TEXT("cleared_count"), ClearedCount);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: save_material
// Save a material asset to disk
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::SaveMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	bool bOnlyIfDirty = Params->HasField(TEXT("only_if_dirty")) ? Params->GetBoolField(TEXT("only_if_dirty")) : true;

	// Verify asset exists
	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!LoadedAsset)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load asset at '%s'"), *AssetPath));
	}

	bool bWasDirty = LoadedAsset->GetPackage()->IsDirty();
	bool bSaved = UEditorAssetLibrary::SaveAsset(AssetPath, bOnlyIfDirty);

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetBoolField(TEXT("saved"), bSaved);
	ResultJson->SetBoolField(TEXT("was_dirty"), bWasDirty);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Wave 5 — Graph Editing Power
// ============================================================================

// ============================================================================
// Action: update_custom_hlsl_node
// Params: { "asset_path", "expression_name", "code"?, "description"?,
//           "output_type"?, "inputs"?, "additional_outputs"?,
//           "include_file_paths"?, "additional_defines"? }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::UpdateCustomHlslNode(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ExprName = Params->GetStringField(TEXT("expression_name"));

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	// Find the expression
	UMaterialExpressionCustom* CustomExpr = nullptr;
	for (const TObjectPtr<UMaterialExpression>& Expr : Mat->GetExpressions())
	{
		if (Expr && Expr->GetName() == ExprName)
		{
			CustomExpr = Cast<UMaterialExpressionCustom>(Expr);
			if (!CustomExpr)
			{
				return FMonolithActionResult::Error(FString::Printf(
					TEXT("Expression '%s' is a %s, not a UMaterialExpressionCustom"),
					*ExprName, *Expr->GetClass()->GetName()));
			}
			break;
		}
	}

	if (!CustomExpr)
	{
		// List available expressions for better error message
		TArray<FString> AvailableNames;
		for (const TObjectPtr<UMaterialExpression>& Expr : Mat->GetExpressions())
		{
			if (Expr) AvailableNames.Add(Expr->GetName());
		}
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Expression '%s' not found. Available: %s"),
			*ExprName, *FString::Join(AvailableNames, TEXT(", "))));
	}

	GEditor->BeginTransaction(FText::FromString(TEXT("UpdateCustomHlslNode")));
	Mat->Modify();
	CustomExpr->Modify();

	TArray<FString> UpdatedFields;

	if (Params->HasField(TEXT("code")))
	{
		CustomExpr->Code = Params->GetStringField(TEXT("code"));
		UpdatedFields.Add(TEXT("code"));
	}

	if (Params->HasField(TEXT("description")))
	{
		CustomExpr->Description = Params->GetStringField(TEXT("description"));
		UpdatedFields.Add(TEXT("description"));
	}

	if (Params->HasField(TEXT("output_type")))
	{
		CustomExpr->OutputType = ParseCustomOutputType(Params->GetStringField(TEXT("output_type")));
		UpdatedFields.Add(TEXT("output_type"));
	}

	const TArray<TSharedPtr<FJsonValue>>* InputsArray = nullptr;
	if (Params->TryGetArrayField(TEXT("inputs"), InputsArray))
	{
		CustomExpr->Inputs.Empty();
		for (const TSharedPtr<FJsonValue>& InputVal : *InputsArray)
		{
			const TSharedPtr<FJsonObject>* InputObjPtr = nullptr;
			if (InputVal && InputVal->TryGetObject(InputObjPtr) && InputObjPtr)
			{
				FCustomInput NewInput;
				NewInput.InputName = *(*InputObjPtr)->GetStringField(TEXT("name"));
				CustomExpr->Inputs.Add(NewInput);
			}
		}
		UpdatedFields.Add(TEXT("inputs"));
	}

	const TArray<TSharedPtr<FJsonValue>>* AddOutputsArray = nullptr;
	if (Params->TryGetArrayField(TEXT("additional_outputs"), AddOutputsArray))
	{
		CustomExpr->AdditionalOutputs.Empty();
		for (const TSharedPtr<FJsonValue>& OutVal : *AddOutputsArray)
		{
			const TSharedPtr<FJsonObject>* OutObjPtr = nullptr;
			if (OutVal && OutVal->TryGetObject(OutObjPtr) && OutObjPtr)
			{
				FCustomOutput NewOutput;
				NewOutput.OutputName = *(*OutObjPtr)->GetStringField(TEXT("name"));
				if ((*OutObjPtr)->HasField(TEXT("type")))
				{
					NewOutput.OutputType = ParseCustomOutputType((*OutObjPtr)->GetStringField(TEXT("type")));
				}
				CustomExpr->AdditionalOutputs.Add(NewOutput);
			}
		}
		UpdatedFields.Add(TEXT("additional_outputs"));
	}

	const TArray<TSharedPtr<FJsonValue>>* IncludePathsArray = nullptr;
	if (Params->TryGetArrayField(TEXT("include_file_paths"), IncludePathsArray))
	{
		CustomExpr->IncludeFilePaths.Empty();
		for (const TSharedPtr<FJsonValue>& PathVal : *IncludePathsArray)
		{
			FString Path;
			if (PathVal && PathVal->TryGetString(Path))
			{
				CustomExpr->IncludeFilePaths.Add(Path);
			}
		}
		UpdatedFields.Add(TEXT("include_file_paths"));
	}

	const TArray<TSharedPtr<FJsonValue>>* DefinesArray = nullptr;
	if (Params->TryGetArrayField(TEXT("additional_defines"), DefinesArray))
	{
		CustomExpr->AdditionalDefines.Empty();
		for (const TSharedPtr<FJsonValue>& DefVal : *DefinesArray)
		{
			const TSharedPtr<FJsonObject>* DefObjPtr = nullptr;
			if (DefVal && DefVal->TryGetObject(DefObjPtr) && DefObjPtr)
			{
				FCustomDefine NewDefine;
				NewDefine.DefineName = (*DefObjPtr)->GetStringField(TEXT("name"));
				NewDefine.DefineValue = (*DefObjPtr)->HasField(TEXT("value"))
					? (*DefObjPtr)->GetStringField(TEXT("value")) : TEXT("");
				CustomExpr->AdditionalDefines.Add(NewDefine);
			}
		}
		UpdatedFields.Add(TEXT("additional_defines"));
	}

	// Rebuild outputs after any structural change
	CustomExpr->RebuildOutputs();

	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	GEditor->EndTransaction();

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetStringField(TEXT("expression_name"), CustomExpr->GetName());

	TArray<TSharedPtr<FJsonValue>> UpdatedArr;
	for (const FString& F : UpdatedFields) UpdatedArr.Add(MakeShared<FJsonValueString>(F));
	ResultJson->SetArrayField(TEXT("updated_fields"), UpdatedArr);
	ResultJson->SetNumberField(TEXT("input_count"), CustomExpr->Inputs.Num());
	ResultJson->SetNumberField(TEXT("output_count"), 1 + CustomExpr->AdditionalOutputs.Num());

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: replace_expression
// Params: { "asset_path", "expression_name", "new_class",
//           "new_properties"?, "preserve_connections"? }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::ReplaceExpression(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ExprName = Params->GetStringField(TEXT("expression_name"));
	FString NewClassName = Params->GetStringField(TEXT("new_class"));
	bool bPreserveConnections = Params->HasField(TEXT("preserve_connections"))
		? Params->GetBoolField(TEXT("preserve_connections")) : true;

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	// Find old expression
	UMaterialExpression* OldExpr = nullptr;
	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Mat->GetExpressions();
	for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
	{
		if (Expr && Expr->GetName() == ExprName)
		{
			OldExpr = Expr;
			break;
		}
	}

	if (!OldExpr)
	{
		TArray<FString> AvailableNames;
		for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
		{
			if (Expr) AvailableNames.Add(Expr->GetName());
		}
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Expression '%s' not found. Available: %s"),
			*ExprName, *FString::Join(AvailableNames, TEXT(", "))));
	}

	// Resolve new class
	FString FullClassName = NewClassName;
	if (!FullClassName.StartsWith(TEXT("MaterialExpression")))
	{
		FullClassName = TEXT("MaterialExpression") + FullClassName;
	}
	UClass* NewClass = FindFirstObject<UClass>(*FullClassName, EFindFirstObjectOptions::NativeFirst);
	if (!NewClass)
	{
		// Try with U prefix
		NewClass = FindFirstObject<UClass>(*(TEXT("U") + FullClassName), EFindFirstObjectOptions::NativeFirst);
	}
	if (!NewClass || !NewClass->IsChildOf(UMaterialExpression::StaticClass()))
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Invalid expression class '%s'. Use list_expression_classes to see valid options."), *NewClassName));
	}

	// Record old expression state before deleting
	FString OldClassName = OldExpr->GetClass()->GetName();
	int32 OldPosX = OldExpr->MaterialExpressionEditorX;
	int32 OldPosY = OldExpr->MaterialExpressionEditorY;

	// --- Record INPUT connections on the old expression (what feeds into it) ---
	struct FRecordedInput
	{
		FString InputName;
		int32 InputIndex;
		UMaterialExpression* SourceExpr;
		int32 SourceOutputIndex;
	};
	TArray<FRecordedInput> OldInputs;
	for (int32 i = 0; ; ++i)
	{
		FExpressionInput* Input = OldExpr->GetInput(i);
		if (!Input) break;
		if (Input->Expression)
		{
			FRecordedInput Rec;
			Rec.InputName = OldExpr->GetInputName(i).ToString();
			Rec.InputIndex = i;
			Rec.SourceExpr = Input->Expression;
			Rec.SourceOutputIndex = Input->OutputIndex;
			OldInputs.Add(Rec);
		}
	}

	// --- Record OUTPUT consumers (what references the old expression) ---
	struct FRecordedOutput
	{
		FString ConsumerInputName;
		int32 ConsumerInputIndex;
		int32 OldOutputIndex;
		FString OldOutputName;
		UMaterialExpression* ConsumerExpr; // nullptr for material output
		EMaterialProperty MatProperty;     // MP_MAX if not material output
	};
	TArray<FRecordedOutput> OldOutputConsumers;

	// Build output name map from old expression
	TMap<int32, FString> OldOutputNames;
	for (int32 i = 0; i < OldExpr->Outputs.Num(); ++i)
	{
		OldOutputNames.Add(i, OldExpr->Outputs[i].OutputName.ToString());
	}

	// Scan all expressions for inputs referencing OldExpr
	for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
	{
		if (!Expr || Expr == OldExpr) continue;
		for (int32 i = 0; ; ++i)
		{
			FExpressionInput* Input = Expr->GetInput(i);
			if (!Input) break;
			if (Input->Expression == OldExpr)
			{
				FRecordedOutput Rec;
				Rec.ConsumerInputName = Expr->GetInputName(i).ToString();
				Rec.ConsumerInputIndex = i;
				Rec.OldOutputIndex = Input->OutputIndex;
				Rec.OldOutputName = OldOutputNames.Contains(Input->OutputIndex)
					? OldOutputNames[Input->OutputIndex] : TEXT("");
				Rec.ConsumerExpr = Expr;
				Rec.MatProperty = MP_MAX;
				OldOutputConsumers.Add(Rec);
			}
		}
	}

	// Scan material output slots
	for (const FMaterialOutputEntry& Entry : MaterialOutputEntries)
	{
		FExpressionInput* Input = Mat->GetExpressionInputForProperty(Entry.Property);
		if (Input && Input->Expression == OldExpr)
		{
			FRecordedOutput Rec;
			Rec.ConsumerInputName = Entry.Name;
			Rec.ConsumerInputIndex = -1;
			Rec.OldOutputIndex = Input->OutputIndex;
			Rec.OldOutputName = OldOutputNames.Contains(Input->OutputIndex)
				? OldOutputNames[Input->OutputIndex] : TEXT("");
			Rec.ConsumerExpr = nullptr;
			Rec.MatProperty = Entry.Property;
			OldOutputConsumers.Add(Rec);
		}
	}

	// --- Perform the replacement in a single undo transaction ---
	GEditor->BeginTransaction(FText::FromString(TEXT("ReplaceExpression")));
	Mat->Modify();

	// Delete old expression
	UMaterialEditingLibrary::DeleteMaterialExpression(Mat, OldExpr);
	OldExpr = nullptr; // dangling after delete

	// Create new expression
	UMaterialExpression* NewExpr = UMaterialEditingLibrary::CreateMaterialExpression(
		Mat, NewClass, OldPosX, OldPosY);

	if (!NewExpr)
	{
		GEditor->EndTransaction();
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Failed to create replacement expression of class '%s'"), *NewClassName));
	}

	// Set properties via reflection if provided
	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	if (Params->TryGetObjectField(TEXT("new_properties"), PropsObj) && PropsObj)
	{
		for (const auto& Pair : (*PropsObj)->Values)
		{
			FProperty* Prop = NewExpr->GetClass()->FindPropertyByName(*Pair.Key);
			if (Prop)
			{
				FString ValueStr;
				if (Pair.Value->TryGetString(ValueStr))
				{
					void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(NewExpr);
					Prop->ImportText_Direct(*ValueStr, ValuePtr, NewExpr, PPF_None);
				}
			}
		}
	}

	// --- Reconnect ---
	TArray<TSharedPtr<FJsonValue>> ReconnectedArr;
	TArray<TSharedPtr<FJsonValue>> FailedArr;
	TArray<TSharedPtr<FJsonValue>> WarningsArr;

	if (bPreserveConnections)
	{
		// Build new expression input name map
		TMap<FString, int32> NewInputNameToIndex;
		for (int32 i = 0; ; ++i)
		{
			FExpressionInput* Input = NewExpr->GetInput(i);
			if (!Input) break;
			NewInputNameToIndex.Add(NewExpr->GetInputName(i).ToString(), i);
		}

		// Count new inputs for index-based fallback bounds
		int32 NewInputCount = NewInputNameToIndex.Num();

		// Build new expression output name map
		TMap<FString, int32> NewOutputNameToIndex;
		for (int32 i = 0; i < NewExpr->Outputs.Num(); ++i)
		{
			FString OutName = NewExpr->Outputs[i].OutputName.ToString();
			if (!OutName.IsEmpty())
			{
				NewOutputNameToIndex.Add(OutName, i);
			}
		}

		// Reconnect inputs (what fed into old expr -> feed into new expr)
		for (const FRecordedInput& Rec : OldInputs)
		{
			bool bReconnected = false;
			bool bUsedFallback = false;

			// Try name match first
			int32* MatchedIdx = NewInputNameToIndex.Find(Rec.InputName);
			if (MatchedIdx)
			{
				FString SourceOutputName;
				if (Rec.SourceExpr->Outputs.IsValidIndex(Rec.SourceOutputIndex))
				{
					SourceOutputName = Rec.SourceExpr->Outputs[Rec.SourceOutputIndex].OutputName.ToString();
				}
				bReconnected = UMaterialEditingLibrary::ConnectMaterialExpressions(
					Rec.SourceExpr, SourceOutputName, NewExpr, Rec.InputName);
			}

			// Index fallback
			if (!bReconnected && Rec.InputIndex < NewInputCount)
			{
				FString SourceOutputName;
				if (Rec.SourceExpr->Outputs.IsValidIndex(Rec.SourceOutputIndex))
				{
					SourceOutputName = Rec.SourceExpr->Outputs[Rec.SourceOutputIndex].OutputName.ToString();
				}
				// Get the new input name at the same index
				FString NewInputName;
				for (const auto& Pair : NewInputNameToIndex)
				{
					if (Pair.Value == Rec.InputIndex)
					{
						NewInputName = Pair.Key;
						break;
					}
				}
				bReconnected = UMaterialEditingLibrary::ConnectMaterialExpressions(
					Rec.SourceExpr, SourceOutputName, NewExpr, NewInputName);
				if (bReconnected) bUsedFallback = true;
			}

			if (bReconnected)
			{
				auto RecJson = MakeShared<FJsonObject>();
				RecJson->SetStringField(TEXT("type"), TEXT("input"));
				RecJson->SetStringField(TEXT("old_input"), Rec.InputName);
				RecJson->SetStringField(TEXT("source"), Rec.SourceExpr->GetName());
				RecJson->SetBoolField(TEXT("name_match"), !bUsedFallback);
				ReconnectedArr.Add(MakeShared<FJsonValueObject>(RecJson));

				if (bUsedFallback)
				{
					WarningsArr.Add(MakeShared<FJsonValueString>(FString::Printf(
						TEXT("Input '%s' reconnected via index fallback (index %d) — new expression has no pin named '%s'"),
						*Rec.InputName, Rec.InputIndex, *Rec.InputName)));
				}
			}
			else
			{
				auto FailJson = MakeShared<FJsonObject>();
				FailJson->SetStringField(TEXT("type"), TEXT("input"));
				FailJson->SetStringField(TEXT("old_input"), Rec.InputName);
				FailJson->SetStringField(TEXT("source"), Rec.SourceExpr->GetName());
				FailedArr.Add(MakeShared<FJsonValueObject>(FailJson));
			}
		}

		// Reconnect outputs (consumers of old expr -> now consume new expr)
		for (const FRecordedOutput& Rec : OldOutputConsumers)
		{
			bool bReconnected = false;
			bool bUsedFallback = false;

			// Determine which output index on the new expression to use
			int32 NewOutputIndex = -1;

			// Try name match on output
			if (!Rec.OldOutputName.IsEmpty())
			{
				int32* MatchedIdx = NewOutputNameToIndex.Find(Rec.OldOutputName);
				if (MatchedIdx)
				{
					NewOutputIndex = *MatchedIdx;
				}
			}

			// Index fallback for output
			if (NewOutputIndex < 0 && Rec.OldOutputIndex < NewExpr->Outputs.Num())
			{
				NewOutputIndex = Rec.OldOutputIndex;
				if (!Rec.OldOutputName.IsEmpty()) bUsedFallback = true;
			}

			if (NewOutputIndex < 0)
			{
				NewOutputIndex = 0; // default to first output
				bUsedFallback = true;
			}

			FString NewOutputName;
			if (NewExpr->Outputs.IsValidIndex(NewOutputIndex))
			{
				NewOutputName = NewExpr->Outputs[NewOutputIndex].OutputName.ToString();
			}

			if (Rec.ConsumerExpr)
			{
				// Expression-to-expression connection
				bReconnected = UMaterialEditingLibrary::ConnectMaterialExpressions(
					NewExpr, NewOutputName, Rec.ConsumerExpr, Rec.ConsumerInputName);
			}
			else
			{
				// Material output connection
				bReconnected = UMaterialEditingLibrary::ConnectMaterialProperty(
					NewExpr, NewOutputName, Rec.MatProperty);
			}

			if (bReconnected)
			{
				auto RecJson = MakeShared<FJsonObject>();
				RecJson->SetStringField(TEXT("type"), TEXT("output"));
				RecJson->SetStringField(TEXT("consumer"), Rec.ConsumerExpr
					? Rec.ConsumerExpr->GetName() : Rec.ConsumerInputName);
				RecJson->SetBoolField(TEXT("name_match"), !bUsedFallback);
				ReconnectedArr.Add(MakeShared<FJsonValueObject>(RecJson));

				if (bUsedFallback)
				{
					WarningsArr.Add(MakeShared<FJsonValueString>(FString::Printf(
						TEXT("Output to '%s' reconnected via index fallback (output index %d)"),
						*Rec.ConsumerInputName, NewOutputIndex)));
				}
			}
			else
			{
				auto FailJson = MakeShared<FJsonObject>();
				FailJson->SetStringField(TEXT("type"), TEXT("output"));
				FailJson->SetStringField(TEXT("consumer"), Rec.ConsumerExpr
					? Rec.ConsumerExpr->GetName() : Rec.ConsumerInputName);
				FailedArr.Add(MakeShared<FJsonValueObject>(FailJson));
			}
		}
	}

	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	GEditor->EndTransaction();

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetStringField(TEXT("old_class"), OldClassName);
	ResultJson->SetStringField(TEXT("new_class"), NewExpr->GetClass()->GetName());
	ResultJson->SetStringField(TEXT("new_name"), NewExpr->GetName());
	ResultJson->SetArrayField(TEXT("reconnected"), ReconnectedArr);
	ResultJson->SetArrayField(TEXT("failed_reconnections"), FailedArr);
	ResultJson->SetArrayField(TEXT("warnings"), WarningsArr);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: get_expression_pin_info
// Params: { "class_name": "MaterialExpressionMultiply" }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::GetExpressionPinInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString ClassName = Params->GetStringField(TEXT("class_name"));

	// Resolve class name with shorthand support
	FString FullClassName = ClassName;
	if (!FullClassName.StartsWith(TEXT("MaterialExpression")))
	{
		FullClassName = TEXT("MaterialExpression") + FullClassName;
	}
	UClass* ExprClass = FindFirstObject<UClass>(*FullClassName, EFindFirstObjectOptions::NativeFirst);
	if (!ExprClass)
	{
		ExprClass = FindFirstObject<UClass>(*(TEXT("U") + FullClassName), EFindFirstObjectOptions::NativeFirst);
	}
	if (!ExprClass || !ExprClass->IsChildOf(UMaterialExpression::StaticClass()))
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Expression class '%s' not found. Use list_expression_classes to see valid options."), *ClassName));
	}

	// Create temporary instance to read pin info
	UMaterialExpression* TempExpr = NewObject<UMaterialExpression>(GetTransientPackage(), ExprClass, NAME_None, RF_Transient);
	if (!TempExpr)
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Failed to create temp instance of '%s'"), *ClassName));
	}

	// Read inputs
	TArray<TSharedPtr<FJsonValue>> InputsArray;
	for (int32 i = 0; ; ++i)
	{
		FExpressionInput* Input = TempExpr->GetInput(i);
		if (!Input) break;

		auto InputJson = MakeShared<FJsonObject>();
		InputJson->SetNumberField(TEXT("index"), i);
		InputJson->SetStringField(TEXT("name"), TempExpr->GetInputName(i).ToString());
		InputsArray.Add(MakeShared<FJsonValueObject>(InputJson));
	}

	// Read outputs
	TArray<TSharedPtr<FJsonValue>> OutputsArray;
	for (int32 i = 0; i < TempExpr->Outputs.Num(); ++i)
	{
		auto OutputJson = MakeShared<FJsonObject>();
		OutputJson->SetNumberField(TEXT("index"), i);
		OutputJson->SetStringField(TEXT("name"), TempExpr->Outputs[i].OutputName.ToString());
		OutputsArray.Add(MakeShared<FJsonValueObject>(OutputJson));
	}

	TempExpr->MarkAsGarbage();

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("class"), ExprClass->GetName());
	ResultJson->SetStringField(TEXT("display_name"), ExprClass->GetDisplayNameText().ToString());
	ResultJson->SetArrayField(TEXT("inputs"), InputsArray);
	ResultJson->SetNumberField(TEXT("input_count"), InputsArray.Num());
	ResultJson->SetArrayField(TEXT("outputs"), OutputsArray);
	ResultJson->SetNumberField(TEXT("output_count"), OutputsArray.Num());

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: rename_expression
// Params: { "asset_path", "expression_name", "new_desc" }
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::RenameExpression(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ExprName = Params->GetStringField(TEXT("expression_name"));
	FString NewDesc = Params->GetStringField(TEXT("new_desc"));

	UMaterial* Mat = LoadBaseMaterial(AssetPath);
	if (!Mat)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to load base material at '%s'"), *AssetPath));
	}

	UMaterialExpression* TargetExpr = nullptr;
	for (const TObjectPtr<UMaterialExpression>& Expr : Mat->GetExpressions())
	{
		if (Expr && Expr->GetName() == ExprName)
		{
			TargetExpr = Expr;
			break;
		}
	}

	if (!TargetExpr)
	{
		TArray<FString> AvailableNames;
		for (const TObjectPtr<UMaterialExpression>& Expr : Mat->GetExpressions())
		{
			if (Expr) AvailableNames.Add(Expr->GetName());
		}
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Expression '%s' not found. Available: %s"),
			*ExprName, *FString::Join(AvailableNames, TEXT(", "))));
	}

	FString OldDesc = TargetExpr->Desc;

	GEditor->BeginTransaction(FText::FromString(TEXT("RenameExpression")));
	TargetExpr->Modify();
	TargetExpr->Desc = NewDesc;
	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	GEditor->EndTransaction();

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("asset_path"), AssetPath);
	ResultJson->SetStringField(TEXT("expression_name"), ExprName);
	ResultJson->SetStringField(TEXT("old_desc"), OldDesc);
	ResultJson->SetStringField(TEXT("new_desc"), NewDesc);

	return FMonolithActionResult::Success(ResultJson);
}

// ============================================================================
// Action: list_material_instances
// Params: { "parent_path", "recursive"? }
// Uses IAssetRegistry::GetReferencers to find assets that reference the parent.
// ============================================================================

FMonolithActionResult FMonolithMaterialActions::ListMaterialInstances(const TSharedPtr<FJsonObject>& Params)
{
	FString ParentPath = Params->GetStringField(TEXT("parent_path"));
	bool bRecursive = Params->HasField(TEXT("recursive")) ? Params->GetBoolField(TEXT("recursive")) : true;

	// Verify parent exists
	UObject* ParentAsset = UEditorAssetLibrary::LoadAsset(ParentPath);
	if (!ParentAsset || !Cast<UMaterialInterface>(ParentAsset))
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Failed to load material interface at '%s'"), *ParentPath));
	}

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	struct FInstanceInfo
	{
		FString Path;
		FString Name;
		int32 Depth;
	};
	TArray<FInstanceInfo> AllInstances;

	// BFS to find instances
	struct FQueueEntry
	{
		FName PackageName;
		int32 Depth;
	};

	TArray<FQueueEntry> Queue;
	TSet<FName> Visited;

	// Extract package name from asset path
	FString PackageName = ParentPath;
	Queue.Add({ FName(*PackageName), 1 });
	Visited.Add(FName(*PackageName));

	while (Queue.Num() > 0)
	{
		FQueueEntry Current = Queue[0];
		Queue.RemoveAt(0);

		TArray<FAssetIdentifier> Referencers;
		AR.GetReferencers(FAssetIdentifier(Current.PackageName), Referencers);

		for (const FAssetIdentifier& Ref : Referencers)
		{
			FName RefPackage = Ref.PackageName;
			if (RefPackage.IsNone() || Visited.Contains(RefPackage))
			{
				continue;
			}

			// Check if this referencer is a MaterialInstanceConstant
			TArray<FAssetData> AssetsInPackage;
			AR.GetAssetsByPackageName(RefPackage, AssetsInPackage, true);

			for (const FAssetData& AssetData : AssetsInPackage)
			{
				// Check if it's a MIC or subclass (MaterialInstanceConstant is the primary persistent type)
				FTopLevelAssetPath ClassPath = AssetData.AssetClassPath;
				FString ClassStr = ClassPath.GetAssetName().ToString();
				if (ClassStr == TEXT("MaterialInstanceConstant"))
				{
					FInstanceInfo Info;
					Info.Path = AssetData.GetObjectPathString();
					// Clean up the path: remove the .AssetName suffix if present
					if (Info.Path.Contains(TEXT(".")))
					{
						Info.Path = AssetData.PackageName.ToString();
					}
					Info.Name = AssetData.AssetName.ToString();
					Info.Depth = Current.Depth;
					AllInstances.Add(Info);

					// Queue for recursive traversal
					if (bRecursive && !Visited.Contains(RefPackage))
					{
						Queue.Add({ RefPackage, Current.Depth + 1 });
					}
				}
			}

			Visited.Add(RefPackage);
		}
	}

	// Sort by depth then name
	AllInstances.Sort([](const FInstanceInfo& A, const FInstanceInfo& B)
	{
		if (A.Depth != B.Depth) return A.Depth < B.Depth;
		return A.Name < B.Name;
	});

	TArray<TSharedPtr<FJsonValue>> InstancesArray;
	for (const FInstanceInfo& Info : AllInstances)
	{
		auto InstJson = MakeShared<FJsonObject>();
		InstJson->SetStringField(TEXT("path"), Info.Path);
		InstJson->SetStringField(TEXT("name"), Info.Name);
		InstJson->SetNumberField(TEXT("depth"), Info.Depth);
		InstancesArray.Add(MakeShared<FJsonValueObject>(InstJson));
	}

	auto ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("parent"), ParentPath);
	ResultJson->SetNumberField(TEXT("instance_count"), InstancesArray.Num());
	ResultJson->SetArrayField(TEXT("instances"), InstancesArray);
	ResultJson->SetBoolField(TEXT("recursive"), bRecursive);

	return FMonolithActionResult::Success(ResultJson);
}
