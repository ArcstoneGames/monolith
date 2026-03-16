#include "MonolithBlueprintNodeActions.h"
#include "MonolithBlueprintInternal.h"
#include "MonolithBlueprintVariableActions.h"
#include "MonolithBlueprintComponentActions.h"
#include "MonolithBlueprintGraphActions.h"
#include "MonolithBlueprintCompileActions.h"
#include "MonolithJsonUtils.h"
#include "MonolithParamSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_SpawnActorFromClass.h"
#include "EdGraphSchema_K2.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Editor.h"
#include "UObject/Package.h"

// ============================================================
//  Registration
// ============================================================

void FMonolithBlueprintNodeActions::RegisterActions(FMonolithToolRegistry& Registry)
{
	Registry.RegisterAction(TEXT("blueprint"), TEXT("add_node"),
		TEXT("Add a new node to a Blueprint graph. Supports CallFunction, VariableGet, VariableSet, CustomEvent, Branch, Sequence, MacroInstance, and SpawnActorFromClass node types."),
		FMonolithActionHandler::CreateStatic(&HandleAddNode),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"),       TEXT("string"),  TEXT("Blueprint asset path"))
			.Required(TEXT("node_type"),         TEXT("string"),  TEXT("Node type: CallFunction (or 'function'/'call'), VariableGet (or 'get'), VariableSet (or 'set'), CustomEvent (or 'event'), Branch (or 'if'), Sequence, MacroInstance (or 'macro'), SpawnActorFromClass (or 'spawn')"))
			.Optional(TEXT("graph_name"),        TEXT("string"),  TEXT("Graph name (defaults to EventGraph)"))
			.Optional(TEXT("position"),          TEXT("array"),   TEXT("Node position as [x, y] (default: [0, 0])"))
			.Optional(TEXT("function_name"),     TEXT("string"),  TEXT("Function name for CallFunction nodes (e.g. PrintString)"))
			.Optional(TEXT("target_class"),      TEXT("string"),  TEXT("Class to search for the function (e.g. KismetSystemLibrary). If omitted, searches all loaded classes."))
			.Optional(TEXT("variable_name"),     TEXT("string"),  TEXT("Variable name for VariableGet/VariableSet nodes"))
			.Optional(TEXT("event_name"),        TEXT("string"),  TEXT("Custom event name for CustomEvent nodes"))
			.Optional(TEXT("macro_name"),        TEXT("string"),  TEXT("Macro graph name for MacroInstance nodes"))
			.Optional(TEXT("macro_blueprint"),   TEXT("string"),  TEXT("Blueprint asset path containing the macro (optional for MacroInstance)"))
			.Optional(TEXT("actor_class"),       TEXT("string"),  TEXT("Actor class name for SpawnActorFromClass nodes"))
			.Build());

	Registry.RegisterAction(TEXT("blueprint"), TEXT("remove_node"),
		TEXT("Remove a node from a Blueprint graph by node ID."),
		FMonolithActionHandler::CreateStatic(&HandleRemoveNode),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"),  TEXT("string"), TEXT("Blueprint asset path"))
			.Required(TEXT("node_id"),     TEXT("string"), TEXT("Node ID (from get_nodes or add_node response)"))
			.Optional(TEXT("graph_name"),  TEXT("string"), TEXT("Graph name (searches all graphs if omitted)"))
			.Build());

	Registry.RegisterAction(TEXT("blueprint"), TEXT("connect_pins"),
		TEXT("Connect two pins in a Blueprint graph. Source pin must be an output, target pin must be an input (or vice versa — the schema will sort it out)."),
		FMonolithActionHandler::CreateStatic(&HandleConnectPins),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"),   TEXT("string"), TEXT("Blueprint asset path"))
			.Required(TEXT("source_node"),  TEXT("string"), TEXT("Source node ID"))
			.Required(TEXT("source_pin"),   TEXT("string"), TEXT("Source pin name"))
			.Required(TEXT("target_node"),  TEXT("string"), TEXT("Target node ID"))
			.Required(TEXT("target_pin"),   TEXT("string"), TEXT("Target pin name"))
			.Optional(TEXT("graph_name"),   TEXT("string"), TEXT("Graph name (searches all graphs if omitted)"))
			.Build());

	Registry.RegisterAction(TEXT("blueprint"), TEXT("disconnect_pins"),
		TEXT("Disconnect a pin on a Blueprint node. If target_node and target_pin are omitted, all connections on the pin are broken."),
		FMonolithActionHandler::CreateStatic(&HandleDisconnectPins),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"),   TEXT("string"), TEXT("Blueprint asset path"))
			.Required(TEXT("node_id"),      TEXT("string"), TEXT("Node ID containing the pin to disconnect"))
			.Required(TEXT("pin_name"),     TEXT("string"), TEXT("Pin name to disconnect"))
			.Optional(TEXT("target_node"),  TEXT("string"), TEXT("Target node ID — if provided, only breaks the connection to this specific node"))
			.Optional(TEXT("target_pin"),   TEXT("string"), TEXT("Target pin name — required if target_node is specified"))
			.Optional(TEXT("graph_name"),   TEXT("string"), TEXT("Graph name (searches all graphs if omitted)"))
			.Build());

	Registry.RegisterAction(TEXT("blueprint"), TEXT("set_pin_default"),
		TEXT("Set the default value of a pin on a Blueprint node."),
		FMonolithActionHandler::CreateStatic(&HandleSetPinDefault),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"),  TEXT("string"), TEXT("Blueprint asset path"))
			.Required(TEXT("node_id"),     TEXT("string"), TEXT("Node ID"))
			.Required(TEXT("pin_name"),    TEXT("string"), TEXT("Pin name"))
			.Required(TEXT("value"),       TEXT("string"), TEXT("Default value as string"))
			.Optional(TEXT("graph_name"),  TEXT("string"), TEXT("Graph name (searches all graphs if omitted)"))
			.Build());

	Registry.RegisterAction(TEXT("blueprint"), TEXT("set_node_position"),
		TEXT("Move a Blueprint graph node to a new position."),
		FMonolithActionHandler::CreateStatic(&HandleSetNodePosition),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"),  TEXT("string"), TEXT("Blueprint asset path"))
			.Required(TEXT("node_id"),     TEXT("string"), TEXT("Node ID"))
			.Required(TEXT("position"),    TEXT("array"),  TEXT("New position as [x, y]"))
			.Optional(TEXT("graph_name"),  TEXT("string"), TEXT("Graph name (searches all graphs if omitted)"))
			.Build());

	Registry.RegisterAction(TEXT("blueprint"), TEXT("resolve_node"),
		TEXT("Dry-run node creation — returns resolved type, class, function, and all pins with types/defaults/direction without modifying any asset. Useful for discovering what pins a node will have before adding it."),
		FMonolithActionHandler::CreateStatic(&HandleResolveNode),
		FParamSchemaBuilder()
			.Required(TEXT("node_type"),     TEXT("string"), TEXT("Node type: CallFunction, VariableGet, VariableSet, Branch, CustomEvent (same aliases as add_node)"))
			.Optional(TEXT("function_name"), TEXT("string"), TEXT("Function name for CallFunction nodes"))
			.Optional(TEXT("target_class"),  TEXT("string"), TEXT("Class to search for the function (optional for CallFunction)"))
			.Optional(TEXT("variable_name"), TEXT("string"), TEXT("Variable name hint for VariableGet/VariableSet (uses wildcard if omitted)"))
			.Build());

	Registry.RegisterAction(TEXT("blueprint"), TEXT("batch_execute"),
		TEXT("Execute multiple Blueprint write operations on a single asset in one transaction. Each operation is { \"op\": \"action_name\", ...action_params_minus_asset_path }. Supported ops: add_node, remove_node, connect_pins, disconnect_pins, set_pin_default, set_node_position, add_variable, remove_variable, rename_variable, set_variable_type, set_variable_defaults, add_local_variable, remove_local_variable, add_component, remove_component, rename_component, reparent_component, set_component_property, duplicate_component, add_function, remove_function, rename_function, add_macro, add_event_dispatcher, set_function_params, implement_interface, remove_interface."),
		FMonolithActionHandler::CreateStatic(&HandleBatchExecute),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"),         TEXT("string"),  TEXT("Blueprint asset path"))
			.Required(TEXT("operations"),          TEXT("array"),   TEXT("Array of operation objects: { op, ...params }"))
			.Optional(TEXT("compile_on_complete"), TEXT("boolean"), TEXT("Compile the Blueprint after all operations complete (default: false)"))
			.Optional(TEXT("stop_on_error"),       TEXT("boolean"), TEXT("Stop processing on first failed operation (default: false)"))
			.Build());

	// ---- Wave 4 ----

	Registry.RegisterAction(TEXT("blueprint"), TEXT("add_nodes_bulk"),
		TEXT("Place multiple nodes in one transaction. Returns a temp_id -> node_id mapping so callers can immediately reference created nodes in connect_pins_bulk. Each entry: { temp_id, node_type, function_name?, target_class?, variable_name?, position? }."),
		FMonolithActionHandler::CreateStatic(&HandleAddNodesBulk),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"),  TEXT("string"),  TEXT("Blueprint asset path"))
			.Required(TEXT("nodes"),       TEXT("array"),   TEXT("Array of node descriptors: { temp_id, node_type, function_name?, target_class?, variable_name?, position? }"))
			.Optional(TEXT("graph_name"),  TEXT("string"),  TEXT("Graph name (defaults to EventGraph)"))
			.Optional(TEXT("auto_layout"), TEXT("boolean"), TEXT("Auto-position nodes in a 5-column grid (200px horizontal, 100px vertical spacing). Ignored if position is set per node. Default: false."))
			.Build());

	Registry.RegisterAction(TEXT("blueprint"), TEXT("connect_pins_bulk"),
		TEXT("Wire multiple pin connections in one transaction. Each entry: { source_node, source_pin, target_node, target_pin }. Returns per-connection success/error."),
		FMonolithActionHandler::CreateStatic(&HandleConnectPinsBulk),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"),   TEXT("string"), TEXT("Blueprint asset path"))
			.Required(TEXT("connections"),  TEXT("array"),  TEXT("Array of connection descriptors: { source_node, source_pin, target_node, target_pin }"))
			.Optional(TEXT("graph_name"),   TEXT("string"), TEXT("Graph name (searches all graphs if omitted)"))
			.Build());

	Registry.RegisterAction(TEXT("blueprint"), TEXT("set_pin_defaults_bulk"),
		TEXT("Set multiple pin default values in one transaction. Each entry: { node_id, pin_name, value }. Returns per-entry success/error."),
		FMonolithActionHandler::CreateStatic(&HandleSetPinDefaultsBulk),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Blueprint asset path"))
			.Required(TEXT("defaults"),   TEXT("array"),  TEXT("Array of pin default descriptors: { node_id, pin_name, value }"))
			.Optional(TEXT("graph_name"), TEXT("string"), TEXT("Graph name (searches all graphs if omitted)"))
			.Build());
}

// ============================================================
//  add_node
// ============================================================

FMonolithActionResult FMonolithBlueprintNodeActions::HandleAddNode(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	UBlueprint* BP = MonolithBlueprintInternal::LoadBlueprintFromParams(Params, AssetPath);
	if (!BP)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	FString NodeType = Params->GetStringField(TEXT("node_type"));
	if (NodeType.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("Missing required parameter: node_type"));
	}

	// Normalize common aliases to canonical node type names
	{
		static const TMap<FString, FString> Aliases = {
			{TEXT("function"),       TEXT("CallFunction")},
			{TEXT("call_function"),  TEXT("CallFunction")},
			{TEXT("call"),           TEXT("CallFunction")},
			{TEXT("func"),           TEXT("CallFunction")},
			{TEXT("get"),            TEXT("VariableGet")},
			{TEXT("variable_get"),   TEXT("VariableGet")},
			{TEXT("set"),            TEXT("VariableSet")},
			{TEXT("variable_set"),   TEXT("VariableSet")},
			{TEXT("event"),          TEXT("CustomEvent")},
			{TEXT("custom_event"),   TEXT("CustomEvent")},
			{TEXT("branch"),         TEXT("Branch")},
			{TEXT("if"),             TEXT("Branch")},
			{TEXT("sequence"),       TEXT("Sequence")},
			{TEXT("macro"),          TEXT("MacroInstance")},
			{TEXT("macro_instance"), TEXT("MacroInstance")},
			{TEXT("spawn_actor"),    TEXT("SpawnActorFromClass")},
			{TEXT("spawn"),          TEXT("SpawnActorFromClass")},
		};
		FString Lower = NodeType.ToLower();
		if (const FString* Canonical = Aliases.Find(Lower))
		{
			NodeType = *Canonical;
		}
	}

	FString GraphName = Params->GetStringField(TEXT("graph_name"));
	UEdGraph* Graph = MonolithBlueprintInternal::FindGraphByName(BP, GraphName);
	if (!Graph)
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Graph not found: %s"), GraphName.IsEmpty() ? TEXT("EventGraph") : *GraphName));
	}

	// Parse position
	int32 PosX = 0;
	int32 PosY = 0;
	const TArray<TSharedPtr<FJsonValue>>* PosArray = nullptr;
	if (Params->TryGetArrayField(TEXT("position"), PosArray) && PosArray && PosArray->Num() >= 2)
	{
		PosX = (int32)(*PosArray)[0]->AsNumber();
		PosY = (int32)(*PosArray)[1]->AsNumber();
	}

	UEdGraphNode* NewNode = nullptr;

	// ---- CallFunction ----
	if (NodeType == TEXT("CallFunction"))
	{
		FString FuncName = Params->GetStringField(TEXT("function_name"));
		if (FuncName.IsEmpty())
		{
			return FMonolithActionResult::Error(TEXT("CallFunction node requires 'function_name'"));
		}

		FString TargetClassName = Params->GetStringField(TEXT("target_class"));

		UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);

		// Build list of function name variants to try:
		// Blueprint-callable wrappers use K2_ prefix (e.g. GetActorLocation → K2_GetActorLocation)
		TArray<FName> FuncNameCandidates;
		FuncNameCandidates.Add(FName(*FuncName));
		if (!FuncName.StartsWith(TEXT("K2_")))
		{
			FuncNameCandidates.Add(FName(*FString::Printf(TEXT("K2_%s"), *FuncName)));
		}

		UFunction* Func = nullptr;
		if (!TargetClassName.IsEmpty())
		{
			// Resolve class name — try as-is, with U prefix, and without U prefix
			UClass* TargetClass = FindFirstObject<UClass>(*TargetClassName, EFindFirstObjectOptions::NativeFirst);
			if (!TargetClass && !TargetClassName.StartsWith(TEXT("U")))
				TargetClass = FindFirstObject<UClass>(*FString::Printf(TEXT("U%s"), *TargetClassName), EFindFirstObjectOptions::NativeFirst);
			if (!TargetClass && TargetClassName.StartsWith(TEXT("U")))
				TargetClass = FindFirstObject<UClass>(*TargetClassName.Mid(1), EFindFirstObjectOptions::NativeFirst);

			if (TargetClass)
			{
				// FindFunctionByName searches the full inheritance chain by default
				for (const FName& Candidate : FuncNameCandidates)
				{
					Func = TargetClass->FindFunctionByName(Candidate);
					if (Func) break;
				}
			}
			if (!Func)
			{
				return FMonolithActionResult::Error(FString::Printf(
					TEXT("Function '%s' not found on class '%s' (also tried K2_ prefix). Ensure the function is BlueprintCallable."),
					*FuncName, *TargetClassName));
			}
		}
		else
		{
			for (TObjectIterator<UClass> It; It && !Func; ++It)
			{
				for (const FName& Candidate : FuncNameCandidates)
				{
					Func = It->FindFunctionByName(Candidate);
					if (Func) break;
				}
			}
			if (!Func)
			{
				return FMonolithActionResult::Error(FString::Printf(
					TEXT("Function '%s' not found in any loaded class (also tried K2_ prefix)"), *FuncName));
			}
		}

		CallNode->SetFromFunction(Func);
		CallNode->NodePosX = PosX;
		CallNode->NodePosY = PosY;
		Graph->AddNode(CallNode, /*bUserAction=*/true, /*bSelectNewNode=*/false);
		CallNode->AllocateDefaultPins();
		NewNode = CallNode;
	}
	// ---- VariableGet ----
	else if (NodeType == TEXT("VariableGet"))
	{
		FString VarName = Params->GetStringField(TEXT("variable_name"));
		if (VarName.IsEmpty())
		{
			return FMonolithActionResult::Error(TEXT("VariableGet node requires 'variable_name'"));
		}

		UK2Node_VariableGet* VarNode = NewObject<UK2Node_VariableGet>(Graph);
		VarNode->VariableReference.SetSelfMember(FName(*VarName));
		VarNode->NodePosX = PosX;
		VarNode->NodePosY = PosY;
		Graph->AddNode(VarNode, true, false);
		VarNode->AllocateDefaultPins();
		NewNode = VarNode;
	}
	// ---- VariableSet ----
	else if (NodeType == TEXT("VariableSet"))
	{
		FString VarName = Params->GetStringField(TEXT("variable_name"));
		if (VarName.IsEmpty())
		{
			return FMonolithActionResult::Error(TEXT("VariableSet node requires 'variable_name'"));
		}

		UK2Node_VariableSet* VarNode = NewObject<UK2Node_VariableSet>(Graph);
		VarNode->VariableReference.SetSelfMember(FName(*VarName));
		VarNode->NodePosX = PosX;
		VarNode->NodePosY = PosY;
		Graph->AddNode(VarNode, true, false);
		VarNode->AllocateDefaultPins();
		NewNode = VarNode;
	}
	// ---- CustomEvent ----
	else if (NodeType == TEXT("CustomEvent"))
	{
		FString EventName = Params->GetStringField(TEXT("event_name"));
		if (EventName.IsEmpty())
		{
			return FMonolithActionResult::Error(TEXT("CustomEvent node requires 'event_name'"));
		}

		UK2Node_CustomEvent* EventNode = NewObject<UK2Node_CustomEvent>(Graph);
		EventNode->CustomFunctionName = FName(*EventName);
		EventNode->NodePosX = PosX;
		EventNode->NodePosY = PosY;
		Graph->AddNode(EventNode, true, false);
		EventNode->AllocateDefaultPins();
		NewNode = EventNode;
	}
	// ---- Branch ----
	else if (NodeType == TEXT("Branch"))
	{
		UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(Graph);
		BranchNode->NodePosX = PosX;
		BranchNode->NodePosY = PosY;
		Graph->AddNode(BranchNode, true, false);
		BranchNode->AllocateDefaultPins();
		NewNode = BranchNode;
	}
	// ---- Sequence ----
	else if (NodeType == TEXT("Sequence"))
	{
		UK2Node_ExecutionSequence* SeqNode = NewObject<UK2Node_ExecutionSequence>(Graph);
		SeqNode->NodePosX = PosX;
		SeqNode->NodePosY = PosY;
		Graph->AddNode(SeqNode, true, false);
		SeqNode->AllocateDefaultPins();
		NewNode = SeqNode;
	}
	// ---- MacroInstance ----
	else if (NodeType == TEXT("MacroInstance"))
	{
		FString MacroName = Params->GetStringField(TEXT("macro_name"));
		if (MacroName.IsEmpty())
		{
			return FMonolithActionResult::Error(TEXT("MacroInstance node requires 'macro_name'"));
		}

		// Resolve the macro graph — search current BP first, then optional macro_blueprint
		UEdGraph* MacroGraph = nullptr;
		FString MacroBPPath = Params->GetStringField(TEXT("macro_blueprint"));
		if (!MacroBPPath.IsEmpty())
		{
			UBlueprint* MacroBP = FMonolithAssetUtils::LoadAssetByPath<UBlueprint>(MacroBPPath);
			if (MacroBP)
			{
				for (const auto& MG : MacroBP->MacroGraphs)
				{
					if (MG && MG->GetName() == MacroName)
					{
						MacroGraph = MG;
						break;
					}
				}
			}
			if (!MacroGraph)
			{
				return FMonolithActionResult::Error(FString::Printf(
					TEXT("Macro '%s' not found in blueprint '%s'"), *MacroName, *MacroBPPath));
			}
		}
		else
		{
			for (const auto& MG : BP->MacroGraphs)
			{
				if (MG && MG->GetName() == MacroName)
				{
					MacroGraph = MG;
					break;
				}
			}
			if (!MacroGraph)
			{
				return FMonolithActionResult::Error(FString::Printf(
					TEXT("Macro '%s' not found in this Blueprint. Provide 'macro_blueprint' if it's in another BP."), *MacroName));
			}
		}

		UK2Node_MacroInstance* MacroNode = NewObject<UK2Node_MacroInstance>(Graph);
		MacroNode->SetMacroGraph(MacroGraph);
		MacroNode->NodePosX = PosX;
		MacroNode->NodePosY = PosY;
		Graph->AddNode(MacroNode, true, false);
		MacroNode->AllocateDefaultPins();
		NewNode = MacroNode;
	}
	// ---- SpawnActorFromClass ----
	else if (NodeType == TEXT("SpawnActorFromClass"))
	{
		FString ActorClassName = Params->GetStringField(TEXT("actor_class"));
		if (ActorClassName.IsEmpty())
		{
			return FMonolithActionResult::Error(TEXT("SpawnActorFromClass node requires 'actor_class'"));
		}

		UClass* ActorClass = FindFirstObject<UClass>(*ActorClassName, EFindFirstObjectOptions::NativeFirst);
		if (!ActorClass)
		{
			return FMonolithActionResult::Error(FString::Printf(
				TEXT("Actor class not found: %s"), *ActorClassName));
		}

		UK2Node_SpawnActorFromClass* SpawnNode = NewObject<UK2Node_SpawnActorFromClass>(Graph);
		SpawnNode->NodePosX = PosX;
		SpawnNode->NodePosY = PosY;
		Graph->AddNode(SpawnNode, true, false);
		SpawnNode->AllocateDefaultPins();

		// Set the class pin default
		UEdGraphPin* ClassPin = SpawnNode->GetClassPin();
		if (ClassPin)
		{
			ClassPin->DefaultObject = ActorClass;
		}

		NewNode = SpawnNode;
	}
	else
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Unknown node_type: '%s'. Valid types: CallFunction, VariableGet, VariableSet, CustomEvent, Branch, Sequence, MacroInstance, SpawnActorFromClass"), *NodeType));
	}

	if (!NewNode)
	{
		return FMonolithActionResult::Error(TEXT("Failed to create node — NewObject returned null"));
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	TSharedPtr<FJsonObject> Root = MonolithBlueprintInternal::SerializeNode(NewNode);
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetStringField(TEXT("graph"), Graph->GetName());
	return FMonolithActionResult::Success(Root);
}

// ============================================================
//  remove_node
// ============================================================

FMonolithActionResult FMonolithBlueprintNodeActions::HandleRemoveNode(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	UBlueprint* BP = MonolithBlueprintInternal::LoadBlueprintFromParams(Params, AssetPath);
	if (!BP)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	FString NodeId = Params->GetStringField(TEXT("node_id"));
	if (NodeId.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("Missing required parameter: node_id"));
	}

	FString GraphName = Params->GetStringField(TEXT("graph_name"));
	UEdGraphNode* Node = MonolithBlueprintInternal::FindNodeById(BP, GraphName, NodeId);
	if (!Node)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Node not found: %s"), *NodeId));
	}

	FBlueprintEditorUtils::RemoveNode(BP, Node, /*bDontRecompile=*/false);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetStringField(TEXT("removed_node"), NodeId);
	Root->SetBoolField(TEXT("success"), true);
	return FMonolithActionResult::Success(Root);
}

// ============================================================
//  connect_pins
// ============================================================

FMonolithActionResult FMonolithBlueprintNodeActions::HandleConnectPins(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	UBlueprint* BP = MonolithBlueprintInternal::LoadBlueprintFromParams(Params, AssetPath);
	if (!BP)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	FString SourceNodeId = Params->GetStringField(TEXT("source_node"));
	FString SourcePinName = Params->GetStringField(TEXT("source_pin"));
	FString TargetNodeId = Params->GetStringField(TEXT("target_node"));
	FString TargetPinName = Params->GetStringField(TEXT("target_pin"));

	if (SourceNodeId.IsEmpty()) return FMonolithActionResult::Error(TEXT("Missing required parameter: source_node"));
	if (SourcePinName.IsEmpty()) return FMonolithActionResult::Error(TEXT("Missing required parameter: source_pin"));
	if (TargetNodeId.IsEmpty()) return FMonolithActionResult::Error(TEXT("Missing required parameter: target_node"));
	if (TargetPinName.IsEmpty()) return FMonolithActionResult::Error(TEXT("Missing required parameter: target_pin"));

	FString GraphName = Params->GetStringField(TEXT("graph_name"));

	UEdGraphNode* SrcNode = MonolithBlueprintInternal::FindNodeById(BP, GraphName, SourceNodeId);
	if (!SrcNode)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Source node not found: %s"), *SourceNodeId));
	}

	UEdGraphNode* TgtNode = MonolithBlueprintInternal::FindNodeById(BP, GraphName, TargetNodeId);
	if (!TgtNode)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Target node not found: %s"), *TargetNodeId));
	}

	UEdGraphPin* SrcPin = MonolithBlueprintInternal::FindPinOnNode(SrcNode, SourcePinName);
	if (!SrcPin)
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Source pin '%s' not found on node '%s'"), *SourcePinName, *SourceNodeId));
	}

	UEdGraphPin* TgtPin = MonolithBlueprintInternal::FindPinOnNode(TgtNode, TargetPinName);
	if (!TgtPin)
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Target pin '%s' not found on node '%s'"), *TargetPinName, *TargetNodeId));
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// Check compatibility before attempting connection
	FPinConnectionResponse Response = Schema->CanCreateConnection(SrcPin, TgtPin);
	if (Response.Response == CONNECT_RESPONSE_DISALLOW)
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Cannot connect pins: %s"), *Response.Message.ToString()));
	}

	bool bConnected = Schema->TryCreateConnection(SrcPin, TgtPin);
	if (!bConnected)
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("TryCreateConnection failed for '%s.%s' -> '%s.%s'"),
			*SourceNodeId, *SourcePinName, *TargetNodeId, *TargetPinName));
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetStringField(TEXT("source_node"), SourceNodeId);
	Root->SetStringField(TEXT("source_pin"), SourcePinName);
	Root->SetStringField(TEXT("target_node"), TargetNodeId);
	Root->SetStringField(TEXT("target_pin"), TargetPinName);
	Root->SetBoolField(TEXT("success"), true);
	return FMonolithActionResult::Success(Root);
}

// ============================================================
//  disconnect_pins
// ============================================================

FMonolithActionResult FMonolithBlueprintNodeActions::HandleDisconnectPins(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	UBlueprint* BP = MonolithBlueprintInternal::LoadBlueprintFromParams(Params, AssetPath);
	if (!BP)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	FString NodeId = Params->GetStringField(TEXT("node_id"));
	if (NodeId.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("Missing required parameter: node_id"));
	}

	FString PinName = Params->GetStringField(TEXT("pin_name"));
	if (PinName.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("Missing required parameter: pin_name"));
	}

	FString GraphName = Params->GetStringField(TEXT("graph_name"));

	UEdGraphNode* Node = MonolithBlueprintInternal::FindNodeById(BP, GraphName, NodeId);
	if (!Node)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Node not found: %s"), *NodeId));
	}

	UEdGraphPin* Pin = MonolithBlueprintInternal::FindPinOnNode(Node, PinName);
	if (!Pin)
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Pin '%s' not found on node '%s'"), *PinName, *NodeId));
	}

	FString TargetNodeId = Params->GetStringField(TEXT("target_node"));
	FString TargetPinName = Params->GetStringField(TEXT("target_pin"));

	if (TargetNodeId.IsEmpty())
	{
		// Break all connections on this pin
		Pin->BreakAllPinLinks(true);
	}
	else
	{
		if (TargetPinName.IsEmpty())
		{
			return FMonolithActionResult::Error(TEXT("'target_pin' is required when 'target_node' is specified"));
		}

		UEdGraphNode* TargetNode = MonolithBlueprintInternal::FindNodeById(BP, GraphName, TargetNodeId);
		if (!TargetNode)
		{
			return FMonolithActionResult::Error(FString::Printf(TEXT("Target node not found: %s"), *TargetNodeId));
		}

		UEdGraphPin* TargetPin = MonolithBlueprintInternal::FindPinOnNode(TargetNode, TargetPinName);
		if (!TargetPin)
		{
			return FMonolithActionResult::Error(FString::Printf(
				TEXT("Target pin '%s' not found on node '%s'"), *TargetPinName, *TargetNodeId));
		}

		Pin->BreakLinkTo(TargetPin);
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetStringField(TEXT("node_id"), NodeId);
	Root->SetStringField(TEXT("pin_name"), PinName);
	if (!TargetNodeId.IsEmpty())
	{
		Root->SetStringField(TEXT("target_node"), TargetNodeId);
		Root->SetStringField(TEXT("target_pin"), TargetPinName);
	}
	Root->SetBoolField(TEXT("success"), true);
	return FMonolithActionResult::Success(Root);
}

// ============================================================
//  set_pin_default
// ============================================================

FMonolithActionResult FMonolithBlueprintNodeActions::HandleSetPinDefault(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	UBlueprint* BP = MonolithBlueprintInternal::LoadBlueprintFromParams(Params, AssetPath);
	if (!BP)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	FString NodeId = Params->GetStringField(TEXT("node_id"));
	if (NodeId.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("Missing required parameter: node_id"));
	}

	FString PinName = Params->GetStringField(TEXT("pin_name"));
	if (PinName.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("Missing required parameter: pin_name"));
	}

	FString Value = Params->GetStringField(TEXT("value"));
	if (Value.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("Missing required parameter: value"));
	}

	FString GraphName = Params->GetStringField(TEXT("graph_name"));

	UEdGraphNode* Node = MonolithBlueprintInternal::FindNodeById(BP, GraphName, NodeId);
	if (!Node)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Node not found: %s"), *NodeId));
	}

	UEdGraphPin* Pin = MonolithBlueprintInternal::FindPinOnNode(Node, PinName);
	if (!Pin)
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Pin '%s' not found on node '%s'"), *PinName, *NodeId));
	}

	if (Pin->Direction != EGPD_Input)
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Pin '%s' is an output pin — only input pins can have default values"), *PinName));
	}

	if (Pin->LinkedTo.Num() > 0)
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Pin '%s' has active connections — disconnect it first before setting a default value"), *PinName));
	}

	Pin->DefaultValue = Value;
	Node->PinDefaultValueChanged(Pin);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetStringField(TEXT("node_id"), NodeId);
	Root->SetStringField(TEXT("pin_name"), PinName);
	Root->SetStringField(TEXT("value"), Value);
	Root->SetBoolField(TEXT("success"), true);
	return FMonolithActionResult::Success(Root);
}

// ============================================================
//  set_node_position
// ============================================================

FMonolithActionResult FMonolithBlueprintNodeActions::HandleSetNodePosition(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	UBlueprint* BP = MonolithBlueprintInternal::LoadBlueprintFromParams(Params, AssetPath);
	if (!BP)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	FString NodeId = Params->GetStringField(TEXT("node_id"));
	if (NodeId.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("Missing required parameter: node_id"));
	}

	const TArray<TSharedPtr<FJsonValue>>* PosArray = nullptr;
	if (!Params->TryGetArrayField(TEXT("position"), PosArray) || !PosArray || PosArray->Num() < 2)
	{
		return FMonolithActionResult::Error(TEXT("'position' must be an array of [x, y]"));
	}

	int32 PosX = (int32)(*PosArray)[0]->AsNumber();
	int32 PosY = (int32)(*PosArray)[1]->AsNumber();

	FString GraphName = Params->GetStringField(TEXT("graph_name"));

	UEdGraphNode* Node = MonolithBlueprintInternal::FindNodeById(BP, GraphName, NodeId);
	if (!Node)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Node not found: %s"), *NodeId));
	}

	Node->NodePosX = PosX;
	Node->NodePosY = PosY;
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetStringField(TEXT("node_id"), NodeId);

	TArray<TSharedPtr<FJsonValue>> OutPosArr;
	OutPosArr.Add(MakeShared<FJsonValueNumber>(PosX));
	OutPosArr.Add(MakeShared<FJsonValueNumber>(PosY));
	Root->SetArrayField(TEXT("position"), OutPosArr);

	Root->SetBoolField(TEXT("success"), true);
	return FMonolithActionResult::Success(Root);
}

// ============================================================
//  batch_execute
// ============================================================

FMonolithActionResult FMonolithBlueprintNodeActions::HandleBatchExecute(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	UBlueprint* BP = MonolithBlueprintInternal::LoadBlueprintFromParams(Params, AssetPath);
	if (!BP)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	// Parse operations — handle both EJson::Array (normal) and EJson::String (Claude Code quirk)
	TArray<TSharedPtr<FJsonValue>> Ops;
	TSharedPtr<FJsonValue> OpsField = Params->TryGetField(TEXT("operations"));
	if (!OpsField.IsValid())
	{
		return FMonolithActionResult::Error(TEXT("Missing required field: operations"));
	}
	if (OpsField->Type == EJson::Array)
	{
		Ops = OpsField->AsArray();
	}
	else if (OpsField->Type == EJson::String)
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(OpsField->AsString());
		if (!FJsonSerializer::Deserialize(Reader, Ops))
		{
			return FMonolithActionResult::Error(TEXT("Failed to parse operations string as JSON array"));
		}
	}
	else
	{
		return FMonolithActionResult::Error(TEXT("'operations' must be an array"));
	}

	bool bStopOnError = false;
	Params->TryGetBoolField(TEXT("stop_on_error"), bStopOnError);

	bool bCompileOnComplete = false;
	Params->TryGetBoolField(TEXT("compile_on_complete"), bCompileOnComplete);

	GEditor->BeginTransaction(NSLOCTEXT("Monolith", "BPBatchExec", "BP Batch Execute"));

	TArray<TSharedPtr<FJsonValue>> Results;
	int32 Ok = 0, Fail = 0;

	for (int32 i = 0; i < Ops.Num(); ++i)
	{
		TSharedPtr<FJsonObject> Op = Ops[i]->AsObject();
		TSharedRef<FJsonObject> RO = MakeShared<FJsonObject>();
		RO->SetNumberField(TEXT("index"), i);

		if (!Op.IsValid())
		{
			RO->SetStringField(TEXT("op"), TEXT("(invalid)"));
			RO->SetBoolField(TEXT("success"), false);
			RO->SetStringField(TEXT("error"), TEXT("Operation entry is not a valid JSON object"));
			Results.Add(MakeShared<FJsonValueObject>(RO));
			Fail++;
			if (bStopOnError) break;
			continue;
		}

		FString OpName = Op->GetStringField(TEXT("op"));
		RO->SetStringField(TEXT("op"), OpName);

		// Build sub-params: inject asset_path then copy all op fields
		TSharedRef<FJsonObject> SubParams = MakeShared<FJsonObject>();
		SubParams->SetStringField(TEXT("asset_path"), AssetPath);
		for (auto& Pair : Op->Values)
		{
			SubParams->SetField(Pair.Key, Pair.Value);
		}

		FMonolithActionResult SubResult = FMonolithActionResult::Error(FString::Printf(TEXT("Unknown op: %s"), *OpName));

		// Node ops
		if      (OpName == TEXT("add_node"))               SubResult = HandleAddNode(SubParams);
		else if (OpName == TEXT("remove_node"))            SubResult = HandleRemoveNode(SubParams);
		else if (OpName == TEXT("connect_pins"))           SubResult = HandleConnectPins(SubParams);
		else if (OpName == TEXT("disconnect_pins"))        SubResult = HandleDisconnectPins(SubParams);
		else if (OpName == TEXT("set_pin_default"))        SubResult = HandleSetPinDefault(SubParams);
		else if (OpName == TEXT("set_node_position"))      SubResult = HandleSetNodePosition(SubParams);
		// Variable ops
		else if (OpName == TEXT("add_variable"))           SubResult = FMonolithBlueprintVariableActions::HandleAddVariable(SubParams);
		else if (OpName == TEXT("remove_variable"))        SubResult = FMonolithBlueprintVariableActions::HandleRemoveVariable(SubParams);
		else if (OpName == TEXT("rename_variable"))        SubResult = FMonolithBlueprintVariableActions::HandleRenameVariable(SubParams);
		else if (OpName == TEXT("set_variable_type"))      SubResult = FMonolithBlueprintVariableActions::HandleSetVariableType(SubParams);
		else if (OpName == TEXT("set_variable_defaults"))  SubResult = FMonolithBlueprintVariableActions::HandleSetVariableDefaults(SubParams);
		else if (OpName == TEXT("add_local_variable"))     SubResult = FMonolithBlueprintVariableActions::HandleAddLocalVariable(SubParams);
		else if (OpName == TEXT("remove_local_variable"))  SubResult = FMonolithBlueprintVariableActions::HandleRemoveLocalVariable(SubParams);
		// Component ops
		else if (OpName == TEXT("add_component"))          SubResult = FMonolithBlueprintComponentActions::HandleAddComponent(SubParams);
		else if (OpName == TEXT("remove_component"))       SubResult = FMonolithBlueprintComponentActions::HandleRemoveComponent(SubParams);
		else if (OpName == TEXT("rename_component"))       SubResult = FMonolithBlueprintComponentActions::HandleRenameComponent(SubParams);
		else if (OpName == TEXT("reparent_component"))     SubResult = FMonolithBlueprintComponentActions::HandleReparentComponent(SubParams);
		else if (OpName == TEXT("set_component_property")) SubResult = FMonolithBlueprintComponentActions::HandleSetComponentProperty(SubParams);
		else if (OpName == TEXT("duplicate_component"))    SubResult = FMonolithBlueprintComponentActions::HandleDuplicateComponent(SubParams);
		// Graph/interface ops
		else if (OpName == TEXT("add_function"))           SubResult = FMonolithBlueprintGraphActions::HandleAddFunction(SubParams);
		else if (OpName == TEXT("remove_function"))        SubResult = FMonolithBlueprintGraphActions::HandleRemoveFunction(SubParams);
		else if (OpName == TEXT("rename_function"))        SubResult = FMonolithBlueprintGraphActions::HandleRenameFunction(SubParams);
		else if (OpName == TEXT("add_macro"))              SubResult = FMonolithBlueprintGraphActions::HandleAddMacro(SubParams);
		else if (OpName == TEXT("add_event_dispatcher"))   SubResult = FMonolithBlueprintGraphActions::HandleAddEventDispatcher(SubParams);
		else if (OpName == TEXT("set_function_params"))    SubResult = FMonolithBlueprintGraphActions::HandleSetFunctionParams(SubParams);
		else if (OpName == TEXT("implement_interface"))    SubResult = FMonolithBlueprintGraphActions::HandleImplementInterface(SubParams);
		else if (OpName == TEXT("remove_interface"))       SubResult = FMonolithBlueprintGraphActions::HandleRemoveInterface(SubParams);

		RO->SetBoolField(TEXT("success"), SubResult.bSuccess);
		if (!SubResult.bSuccess)
		{
			RO->SetStringField(TEXT("error"), SubResult.ErrorMessage);
		}
		if (SubResult.bSuccess && SubResult.Result.IsValid())
		{
			RO->SetObjectField(TEXT("data"), SubResult.Result);
		}

		Results.Add(MakeShared<FJsonValueObject>(RO));
		if (SubResult.bSuccess) Ok++; else Fail++;

		if (!SubResult.bSuccess && bStopOnError) break;
	}

	GEditor->EndTransaction();

	TSharedRef<FJsonObject> Final = MakeShared<FJsonObject>();
	Final->SetBoolField(TEXT("success"), Fail == 0);
	Final->SetNumberField(TEXT("total"), Ops.Num());
	Final->SetNumberField(TEXT("succeeded"), Ok);
	Final->SetNumberField(TEXT("failed"), Fail);
	Final->SetArrayField(TEXT("results"), Results);

	if (bCompileOnComplete)
	{
		TSharedRef<FJsonObject> CompileParams = MakeShared<FJsonObject>();
		CompileParams->SetStringField(TEXT("asset_path"), AssetPath);
		FMonolithActionResult CompileResult = FMonolithBlueprintCompileActions::HandleCompileBlueprint(CompileParams);
		Final->SetBoolField(TEXT("compile_success"), CompileResult.bSuccess);
		if (CompileResult.bSuccess && CompileResult.Result.IsValid())
		{
			Final->SetObjectField(TEXT("compile_result"), CompileResult.Result);
		}
		else if (!CompileResult.bSuccess)
		{
			Final->SetStringField(TEXT("compile_error"), CompileResult.ErrorMessage);
		}
	}

	return FMonolithActionResult::Success(Final);
}

// ============================================================
//  resolve_node
// ============================================================

FMonolithActionResult FMonolithBlueprintNodeActions::HandleResolveNode(const TSharedPtr<FJsonObject>& Params)
{
	FString NodeType = Params->GetStringField(TEXT("node_type"));
	if (NodeType.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("Missing required parameter: node_type"));
	}

	// Apply same alias normalization as add_node
	{
		static const TMap<FString, FString> Aliases = {
			{TEXT("function"),       TEXT("CallFunction")},
			{TEXT("call_function"),  TEXT("CallFunction")},
			{TEXT("call"),           TEXT("CallFunction")},
			{TEXT("func"),           TEXT("CallFunction")},
			{TEXT("get"),            TEXT("VariableGet")},
			{TEXT("variable_get"),   TEXT("VariableGet")},
			{TEXT("set"),            TEXT("VariableSet")},
			{TEXT("variable_set"),   TEXT("VariableSet")},
			{TEXT("event"),          TEXT("CustomEvent")},
			{TEXT("custom_event"),   TEXT("CustomEvent")},
			{TEXT("branch"),         TEXT("Branch")},
			{TEXT("if"),             TEXT("Branch")},
			{TEXT("sequence"),       TEXT("Sequence")},
		};
		FString Lower = NodeType.ToLower();
		if (const FString* Canonical = Aliases.Find(Lower))
		{
			NodeType = *Canonical;
		}
	}

	TArray<FString> Warnings;

	// Create a transient graph to host the node — some node types behave differently
	// without a schema on the outer graph, so we set it explicitly.
	UEdGraph* TempGraph = NewObject<UEdGraph>(GetTransientPackage());
	TempGraph->Schema = UEdGraphSchema_K2::StaticClass();

	UEdGraphNode* Node = nullptr;

	if (NodeType == TEXT("CallFunction"))
	{
		FString FuncName = Params->GetStringField(TEXT("function_name"));
		if (FuncName.IsEmpty())
		{
			return FMonolithActionResult::Error(TEXT("CallFunction requires 'function_name'"));
		}

		FString TargetClassName = Params->GetStringField(TEXT("target_class"));

		TArray<FName> Candidates;
		Candidates.Add(FName(*FuncName));
		if (!FuncName.StartsWith(TEXT("K2_")))
		{
			Candidates.Add(FName(*FString::Printf(TEXT("K2_%s"), *FuncName)));
		}

		UFunction* Func = nullptr;
		if (!TargetClassName.IsEmpty())
		{
			UClass* TargetClass = FindFirstObject<UClass>(*TargetClassName, EFindFirstObjectOptions::NativeFirst);
			if (!TargetClass && !TargetClassName.StartsWith(TEXT("U")))
				TargetClass = FindFirstObject<UClass>(*FString::Printf(TEXT("U%s"), *TargetClassName), EFindFirstObjectOptions::NativeFirst);
			if (!TargetClass && TargetClassName.StartsWith(TEXT("U")))
				TargetClass = FindFirstObject<UClass>(*TargetClassName.Mid(1), EFindFirstObjectOptions::NativeFirst);

			if (TargetClass)
			{
				for (const FName& C : Candidates)
				{
					Func = TargetClass->FindFunctionByName(C);
					if (Func) break;
				}
			}
			if (!Func)
			{
				return FMonolithActionResult::Error(FString::Printf(
					TEXT("Function '%s' not found on class '%s'"), *FuncName, *TargetClassName));
			}
		}
		else
		{
			for (TObjectIterator<UClass> It; It && !Func; ++It)
			{
				for (const FName& C : Candidates)
				{
					Func = It->FindFunctionByName(C);
					if (Func) break;
				}
			}
			if (!Func)
			{
				return FMonolithActionResult::Error(FString::Printf(
					TEXT("Function '%s' not found in any loaded class"), *FuncName));
			}
		}

		UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(TempGraph);
		CallNode->SetFromFunction(Func);
		CallNode->AllocateDefaultPins();
		Node = CallNode;
	}
	else if (NodeType == TEXT("VariableGet"))
	{
		// For a dry-run VariableGet, we use a wildcard self-member reference.
		// If variable_name is provided it's recorded in the response but the pin
		// layout is identical regardless — VariableGet always has one output data pin.
		UK2Node_VariableGet* VarNode = NewObject<UK2Node_VariableGet>(TempGraph);
		FString VarName = Params->GetStringField(TEXT("variable_name"));
		if (VarName.IsEmpty()) VarName = TEXT("Variable");
		VarNode->VariableReference.SetSelfMember(FName(*VarName));
		VarNode->AllocateDefaultPins();
		Node = VarNode;
		Warnings.Add(TEXT("VariableGet pin types reflect a wildcard — actual type depends on the specific variable in the target Blueprint"));
	}
	else if (NodeType == TEXT("VariableSet"))
	{
		UK2Node_VariableSet* VarNode = NewObject<UK2Node_VariableSet>(TempGraph);
		FString VarName = Params->GetStringField(TEXT("variable_name"));
		if (VarName.IsEmpty()) VarName = TEXT("Variable");
		VarNode->VariableReference.SetSelfMember(FName(*VarName));
		VarNode->AllocateDefaultPins();
		Node = VarNode;
		Warnings.Add(TEXT("VariableSet pin types reflect a wildcard — actual type depends on the specific variable in the target Blueprint"));
	}
	else if (NodeType == TEXT("Branch"))
	{
		UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(TempGraph);
		BranchNode->AllocateDefaultPins();
		Node = BranchNode;
	}
	else if (NodeType == TEXT("CustomEvent"))
	{
		UK2Node_CustomEvent* EventNode = NewObject<UK2Node_CustomEvent>(TempGraph);
		FString EventName = Params->GetStringField(TEXT("event_name"));
		if (EventName.IsEmpty()) EventName = TEXT("MyEvent");
		EventNode->CustomFunctionName = FName(*EventName);
		EventNode->AllocateDefaultPins();
		Node = EventNode;
	}
	else if (NodeType == TEXT("Sequence"))
	{
		UK2Node_ExecutionSequence* SeqNode = NewObject<UK2Node_ExecutionSequence>(TempGraph);
		SeqNode->AllocateDefaultPins();
		Node = SeqNode;
	}
	else
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Unsupported node_type for resolve_node: '%s'. Supported: CallFunction, VariableGet, VariableSet, Branch, CustomEvent, Sequence"), *NodeType));
	}

	if (!Node)
	{
		return FMonolithActionResult::Error(TEXT("Failed to create transient node"));
	}

	// Serialize pins
	TArray<TSharedPtr<FJsonValue>> PinsArr;
	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin || Pin->bHidden) continue;

		TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
		PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
		PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
		PinObj->SetStringField(TEXT("type"),
			MonolithBlueprintInternal::ContainerPrefix(Pin->PinType) +
			MonolithBlueprintInternal::PinTypeToString(Pin->PinType));
		PinObj->SetBoolField(TEXT("is_exec"), Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);

		if (!Pin->DefaultValue.IsEmpty())
		{
			PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
		}
		if (Pin->DefaultObject)
		{
			PinObj->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetPathName());
		}

		PinsArr.Add(MakeShared<FJsonValueObject>(PinObj));
	}

	// Determine resolved_function for CallFunction nodes
	FString ResolvedFunction;
	FString ResolvedClass;
	if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
	{
		ResolvedFunction = CallNode->FunctionReference.GetMemberName().ToString();
		if (UClass* OwnerClass = CallNode->FunctionReference.GetMemberParentClass())
		{
			ResolvedClass = OwnerClass->GetName();
		}
	}

	// Build response
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("resolved_type"), NodeType);
	Root->SetStringField(TEXT("resolved_class"), Node->GetClass()->GetName());
	if (!ResolvedFunction.IsEmpty())
	{
		Root->SetStringField(TEXT("resolved_function"), ResolvedFunction);
	}
	if (!ResolvedClass.IsEmpty())
	{
		Root->SetStringField(TEXT("resolved_function_class"), ResolvedClass);
	}
	Root->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Root->SetArrayField(TEXT("pins"), PinsArr);
	Root->SetNumberField(TEXT("pin_count"), PinsArr.Num());

	// Warnings
	TArray<TSharedPtr<FJsonValue>> WarnArr;
	for (const FString& W : Warnings)
	{
		WarnArr.Add(MakeShared<FJsonValueString>(W));
	}
	Root->SetArrayField(TEXT("warnings"), WarnArr);

	// Mark transient objects for GC
	TempGraph->MarkAsGarbage();

	return FMonolithActionResult::Success(Root);
}

// ============================================================
//  add_nodes_bulk
// ============================================================

FMonolithActionResult FMonolithBlueprintNodeActions::HandleAddNodesBulk(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	UBlueprint* BP = MonolithBlueprintInternal::LoadBlueprintFromParams(Params, AssetPath);
	if (!BP)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	// Parse nodes array — handle both EJson::Array (normal) and EJson::String (Claude Code quirk)
	TArray<TSharedPtr<FJsonValue>> NodesArr;
	TSharedPtr<FJsonValue> NodesField = Params->TryGetField(TEXT("nodes"));
	if (!NodesField.IsValid())
	{
		return FMonolithActionResult::Error(TEXT("Missing required field: nodes"));
	}
	if (NodesField->Type == EJson::Array)
	{
		NodesArr = NodesField->AsArray();
	}
	else if (NodesField->Type == EJson::String)
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(NodesField->AsString());
		if (!FJsonSerializer::Deserialize(Reader, NodesArr))
		{
			return FMonolithActionResult::Error(TEXT("Failed to parse nodes string as JSON array"));
		}
	}
	else
	{
		return FMonolithActionResult::Error(TEXT("'nodes' must be an array"));
	}

	bool bAutoLayout = false;
	Params->TryGetBoolField(TEXT("auto_layout"), bAutoLayout);

	FString SharedGraphName = Params->GetStringField(TEXT("graph_name"));

	GEditor->BeginTransaction(NSLOCTEXT("Monolith", "BPAddNodesBulk", "BP Add Nodes Bulk"));

	TArray<TSharedPtr<FJsonValue>> CreatedArr;
	int32 Count = 0;

	for (int32 i = 0; i < NodesArr.Num(); ++i)
	{
		TSharedPtr<FJsonObject> Entry = NodesArr[i]->AsObject();
		if (!Entry.IsValid())
		{
			// Skip invalid entries silently — can't report without a temp_id
			continue;
		}

		FString TempId = Entry->GetStringField(TEXT("temp_id"));

		// Build sub-params: inject asset_path and graph_name, then copy all entry fields
		TSharedRef<FJsonObject> SubParams = MakeShared<FJsonObject>();
		SubParams->SetStringField(TEXT("asset_path"), AssetPath);
		if (!SharedGraphName.IsEmpty())
		{
			SubParams->SetStringField(TEXT("graph_name"), SharedGraphName);
		}
		for (const auto& Pair : Entry->Values)
		{
			SubParams->SetField(Pair.Key, Pair.Value);
		}

		// Apply auto_layout position if the entry doesn't already specify one and auto_layout is on
		if (bAutoLayout && !Entry->HasField(TEXT("position")))
		{
			int32 Col = i % 5;
			int32 Row = i / 5;
			TArray<TSharedPtr<FJsonValue>> PosArr;
			PosArr.Add(MakeShared<FJsonValueNumber>(Col * 200));
			PosArr.Add(MakeShared<FJsonValueNumber>(Row * 100));
			SubParams->SetArrayField(TEXT("position"), PosArr);
		}

		FMonolithActionResult Result = HandleAddNode(SubParams);

		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetStringField(TEXT("temp_id"), TempId);
		Out->SetBoolField(TEXT("success"), Result.bSuccess);

		if (Result.bSuccess && Result.Result.IsValid())
		{
			// SerializeNode uses "id" (not "node_id") — matches get_graph_data format
			FString NodeId = Result.Result->GetStringField(TEXT("id"));
			FString NodeClass = Result.Result->GetStringField(TEXT("class"));
			FString NodeTitle = Result.Result->GetStringField(TEXT("title"));

			Out->SetStringField(TEXT("node_id"), NodeId);
			Out->SetStringField(TEXT("class"),   NodeClass);
			Out->SetStringField(TEXT("title"),   NodeTitle);
			Count++;
		}
		else if (!Result.bSuccess)
		{
			Out->SetStringField(TEXT("error"), Result.ErrorMessage);
		}

		CreatedArr.Add(MakeShared<FJsonValueObject>(Out));
	}

	GEditor->EndTransaction();

	TSharedRef<FJsonObject> Final = MakeShared<FJsonObject>();
	Final->SetArrayField(TEXT("nodes_created"), CreatedArr);
	Final->SetNumberField(TEXT("count"), Count);

	return FMonolithActionResult::Success(Final);
}

// ============================================================
//  connect_pins_bulk
// ============================================================

FMonolithActionResult FMonolithBlueprintNodeActions::HandleConnectPinsBulk(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	UBlueprint* BP = MonolithBlueprintInternal::LoadBlueprintFromParams(Params, AssetPath);
	if (!BP)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	// Parse connections array — handle both EJson::Array and EJson::String (Claude Code quirk)
	TArray<TSharedPtr<FJsonValue>> ConnArr;
	TSharedPtr<FJsonValue> ConnField = Params->TryGetField(TEXT("connections"));
	if (!ConnField.IsValid())
	{
		return FMonolithActionResult::Error(TEXT("Missing required field: connections"));
	}
	if (ConnField->Type == EJson::Array)
	{
		ConnArr = ConnField->AsArray();
	}
	else if (ConnField->Type == EJson::String)
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ConnField->AsString());
		if (!FJsonSerializer::Deserialize(Reader, ConnArr))
		{
			return FMonolithActionResult::Error(TEXT("Failed to parse connections string as JSON array"));
		}
	}
	else
	{
		return FMonolithActionResult::Error(TEXT("'connections' must be an array"));
	}

	FString SharedGraphName = Params->GetStringField(TEXT("graph_name"));

	GEditor->BeginTransaction(NSLOCTEXT("Monolith", "BPConnectPinsBulk", "BP Connect Pins Bulk"));

	TArray<TSharedPtr<FJsonValue>> Results;
	int32 Connected = 0, Failed = 0;

	for (int32 i = 0; i < ConnArr.Num(); ++i)
	{
		TSharedPtr<FJsonObject> Entry = ConnArr[i]->AsObject();

		TSharedRef<FJsonObject> RO = MakeShared<FJsonObject>();
		RO->SetNumberField(TEXT("index"), i);

		if (!Entry.IsValid())
		{
			RO->SetBoolField(TEXT("success"), false);
			RO->SetStringField(TEXT("error"), TEXT("Connection entry is not a valid JSON object"));
			Results.Add(MakeShared<FJsonValueObject>(RO));
			Failed++;
			continue;
		}

		// Build sub-params
		TSharedRef<FJsonObject> SubParams = MakeShared<FJsonObject>();
		SubParams->SetStringField(TEXT("asset_path"), AssetPath);
		if (!SharedGraphName.IsEmpty())
		{
			SubParams->SetStringField(TEXT("graph_name"), SharedGraphName);
		}
		for (const auto& Pair : Entry->Values)
		{
			SubParams->SetField(Pair.Key, Pair.Value);
		}

		FMonolithActionResult Result = HandleConnectPins(SubParams);

		RO->SetBoolField(TEXT("success"), Result.bSuccess);
		if (!Result.bSuccess)
		{
			RO->SetStringField(TEXT("error"), Result.ErrorMessage);
			Failed++;
		}
		else
		{
			Connected++;
		}

		Results.Add(MakeShared<FJsonValueObject>(RO));
	}

	GEditor->EndTransaction();

	TSharedRef<FJsonObject> Final = MakeShared<FJsonObject>();
	Final->SetNumberField(TEXT("connected"), Connected);
	Final->SetNumberField(TEXT("failed"),    Failed);
	Final->SetArrayField(TEXT("results"),    Results);

	return FMonolithActionResult::Success(Final);
}

// ============================================================
//  set_pin_defaults_bulk
// ============================================================

FMonolithActionResult FMonolithBlueprintNodeActions::HandleSetPinDefaultsBulk(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	UBlueprint* BP = MonolithBlueprintInternal::LoadBlueprintFromParams(Params, AssetPath);
	if (!BP)
	{
		return FMonolithActionResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	// Parse defaults array — handle both EJson::Array and EJson::String (Claude Code quirk)
	TArray<TSharedPtr<FJsonValue>> DefaultsArr;
	TSharedPtr<FJsonValue> DefaultsField = Params->TryGetField(TEXT("defaults"));
	if (!DefaultsField.IsValid())
	{
		return FMonolithActionResult::Error(TEXT("Missing required field: defaults"));
	}
	if (DefaultsField->Type == EJson::Array)
	{
		DefaultsArr = DefaultsField->AsArray();
	}
	else if (DefaultsField->Type == EJson::String)
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DefaultsField->AsString());
		if (!FJsonSerializer::Deserialize(Reader, DefaultsArr))
		{
			return FMonolithActionResult::Error(TEXT("Failed to parse defaults string as JSON array"));
		}
	}
	else
	{
		return FMonolithActionResult::Error(TEXT("'defaults' must be an array"));
	}

	FString SharedGraphName = Params->GetStringField(TEXT("graph_name"));

	GEditor->BeginTransaction(NSLOCTEXT("Monolith", "BPSetPinDefaultsBulk", "BP Set Pin Defaults Bulk"));

	TArray<TSharedPtr<FJsonValue>> Results;
	int32 Set = 0, Failed = 0;

	for (int32 i = 0; i < DefaultsArr.Num(); ++i)
	{
		TSharedPtr<FJsonObject> Entry = DefaultsArr[i]->AsObject();

		TSharedRef<FJsonObject> RO = MakeShared<FJsonObject>();
		RO->SetNumberField(TEXT("index"), i);

		if (!Entry.IsValid())
		{
			RO->SetBoolField(TEXT("success"), false);
			RO->SetStringField(TEXT("error"), TEXT("Default entry is not a valid JSON object"));
			Results.Add(MakeShared<FJsonValueObject>(RO));
			Failed++;
			continue;
		}

		// Build sub-params
		TSharedRef<FJsonObject> SubParams = MakeShared<FJsonObject>();
		SubParams->SetStringField(TEXT("asset_path"), AssetPath);
		if (!SharedGraphName.IsEmpty())
		{
			SubParams->SetStringField(TEXT("graph_name"), SharedGraphName);
		}
		for (const auto& Pair : Entry->Values)
		{
			SubParams->SetField(Pair.Key, Pair.Value);
		}

		FMonolithActionResult Result = HandleSetPinDefault(SubParams);

		RO->SetBoolField(TEXT("success"), Result.bSuccess);
		if (!Result.bSuccess)
		{
			RO->SetStringField(TEXT("error"), Result.ErrorMessage);
			Failed++;
		}
		else
		{
			Set++;
		}

		Results.Add(MakeShared<FJsonValueObject>(RO));
	}

	GEditor->EndTransaction();

	TSharedRef<FJsonObject> Final = MakeShared<FJsonObject>();
	Final->SetNumberField(TEXT("set"),     Set);
	Final->SetNumberField(TEXT("failed"),  Failed);
	Final->SetArrayField(TEXT("results"),  Results);

	return FMonolithActionResult::Success(Final);
}
