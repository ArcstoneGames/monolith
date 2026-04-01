#include "MonolithBlueprintModule.h"
#include "MonolithBlueprintActions.h"
#include "MonolithBlueprintVariableActions.h"
#include "MonolithBlueprintComponentActions.h"
#include "MonolithBlueprintGraphActions.h"
#include "MonolithBlueprintNodeActions.h"
#include "MonolithBlueprintCompileActions.h"
#include "MonolithBlueprintCDOActions.h"
#include "MonolithBlueprintStructActions.h"
#include "MonolithBlueprintBuildActions.h"
#include "MonolithBlueprintDiffActions.h"
#include "MonolithBlueprintTemplateActions.h"
#include "MonolithBlueprintGraphExportActions.h"
#include "MonolithBlueprintLayoutActions.h"
#include "MonolithBlueprintSpawnActions.h"
#include "MonolithToolRegistry.h"
#include "MonolithJsonUtils.h"
#include "MonolithSettings.h"

#define LOCTEXT_NAMESPACE "FMonolithBlueprintModule"

void FMonolithBlueprintModule::StartupModule()
{
	if (!GetDefault<UMonolithSettings>()->bEnableBlueprint) return;

	FMonolithToolRegistry& Registry = FMonolithToolRegistry::Get();
	FMonolithBlueprintActions::RegisterActions();
	FMonolithBlueprintVariableActions::RegisterActions(Registry);
	FMonolithBlueprintComponentActions::RegisterActions(Registry);
	FMonolithBlueprintGraphActions::RegisterActions(Registry);
	FMonolithBlueprintNodeActions::RegisterActions(Registry);
	FMonolithBlueprintCompileActions::RegisterActions(Registry);
	FMonolithBlueprintCDOActions::RegisterActions(Registry);
	FMonolithBlueprintStructActions::RegisterActions(Registry);
	FMonolithBlueprintBuildActions::RegisterActions(Registry);
	FMonolithBlueprintDiffActions::RegisterActions(Registry);
	FMonolithBlueprintTemplateActions::RegisterActions(Registry);
	FMonolithBlueprintGraphExportActions::RegisterActions(Registry);
	FMonolithBlueprintLayoutActions::RegisterActions(Registry);
	FMonolithBlueprintSpawnActions::RegisterActions(Registry);
	UE_LOG(LogMonolith, Log, TEXT("Monolith — Blueprint module loaded (88 actions)"));
}

void FMonolithBlueprintModule::ShutdownModule()
{
	FMonolithToolRegistry::Get().UnregisterNamespace(TEXT("blueprint"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMonolithBlueprintModule, MonolithBlueprint)
