#include "MonolithMaterialModule.h"
#include "MonolithMaterialActions.h"
#include "MonolithToolRegistry.h"
#include "MonolithJsonUtils.h"
#include "MonolithSettings.h"

#define LOCTEXT_NAMESPACE "FMonolithMaterialModule"

void FMonolithMaterialModule::StartupModule()
{
	if (!GetDefault<UMonolithSettings>()->bEnableMaterial) return;

	FMonolithMaterialActions::RegisterActions(FMonolithToolRegistry::Get());
	UE_LOG(LogMonolith, Log, TEXT("Monolith — Material module loaded (25 actions)"));
}

void FMonolithMaterialModule::ShutdownModule()
{
	FMonolithToolRegistry::Get().UnregisterNamespace(TEXT("material"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMonolithMaterialModule, MonolithMaterial)
