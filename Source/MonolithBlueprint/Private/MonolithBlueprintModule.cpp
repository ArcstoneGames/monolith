#include "MonolithBlueprintModule.h"
#include "MonolithBlueprintActions.h"
#include "MonolithToolRegistry.h"
#include "MonolithJsonUtils.h"
#include "MonolithSettings.h"

#define LOCTEXT_NAMESPACE "FMonolithBlueprintModule"

void FMonolithBlueprintModule::StartupModule()
{
	if (!GetDefault<UMonolithSettings>()->bEnableBlueprint) return;

	FMonolithBlueprintActions::RegisterActions();
	UE_LOG(LogMonolith, Log, TEXT("Monolith — Blueprint module loaded (6 actions)"));
}

void FMonolithBlueprintModule::ShutdownModule()
{
	FMonolithToolRegistry::Get().UnregisterNamespace(TEXT("blueprint"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMonolithBlueprintModule, MonolithBlueprint)
