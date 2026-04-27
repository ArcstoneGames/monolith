#pragma once

#include "CoreMinimal.h"
#include "MonolithToolRegistry.h"

/**
 * Level Sequence domain action handlers for Monolith.
 * Introspection of ULevelSequence Director Blueprints, their functions/variables,
 * and event-track binding GUID -> director-function mappings.
 */
class FMonolithLevelSequenceActions
{
public:
	/** Register all level_sequence actions with the tool registry */
	static void RegisterActions(FMonolithToolRegistry& Registry);

	// --- Action handlers ---
	static FMonolithActionResult Ping(const TSharedPtr<FJsonObject>& Params);
};
