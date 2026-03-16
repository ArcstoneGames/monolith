#pragma once
#include "CoreMinimal.h"
#include "MonolithToolRegistry.h"

class FMonolithBlueprintNodeActions
{
public:
	static void RegisterActions(FMonolithToolRegistry& Registry);

	static FMonolithActionResult HandleAddNode(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleRemoveNode(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleConnectPins(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleDisconnectPins(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleSetPinDefault(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleSetNodePosition(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleBatchExecute(const TSharedPtr<FJsonObject>& Params);

	// Wave 3 — Discovery & Resolution
	static FMonolithActionResult HandleResolveNode(const TSharedPtr<FJsonObject>& Params);

	// Wave 4 — Bulk Node Operations
	static FMonolithActionResult HandleAddNodesBulk(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleConnectPinsBulk(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleSetPinDefaultsBulk(const TSharedPtr<FJsonObject>& Params);
};
