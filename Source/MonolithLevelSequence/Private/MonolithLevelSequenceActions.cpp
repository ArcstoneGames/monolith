#include "MonolithLevelSequenceActions.h"
#include "MonolithToolRegistry.h"
#include "MonolithParamSchema.h"
#include "Dom/JsonObject.h"

// ============================================================================
// Registration
// ============================================================================

void FMonolithLevelSequenceActions::RegisterActions(FMonolithToolRegistry& Registry)
{
	Registry.RegisterAction(TEXT("level_sequence"), TEXT("ping"),
		TEXT("Smoke test — returns {status:ok, module:MonolithLevelSequence} when the module is loaded."),
		FMonolithActionHandler::CreateStatic(&FMonolithLevelSequenceActions::Ping),
		FParamSchemaBuilder().Build());
}

// ============================================================================
// Handlers
// ============================================================================

FMonolithActionResult FMonolithLevelSequenceActions::Ping(const TSharedPtr<FJsonObject>& Params)
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"), TEXT("ok"));
	Result->SetStringField(TEXT("module"), TEXT("MonolithLevelSequence"));
	return FMonolithActionResult::Success(Result);
}
