#pragma once

#include "CoreMinimal.h"
#include "MonolithToolRegistry.h"

/**
 * PoseSearch domain action handlers for Monolith.
 * 5 actions: schema inspection, database CRUD, stats.
 */
class MONOLITHANIMATION_API FMonolithPoseSearchActions
{
public:
	/** Register all PoseSearch actions with the tool registry */
	static void RegisterActions(FMonolithToolRegistry& Registry);

private:
	static FMonolithActionResult HandleGetPoseSearchSchema(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleGetPoseSearchDatabase(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleAddDatabaseSequence(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleRemoveDatabaseSequence(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleGetDatabaseStats(const TSharedPtr<FJsonObject>& Params);
};
