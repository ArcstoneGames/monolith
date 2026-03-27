#include "MonolithMeshModule.h"
#include "MonolithMeshInspectionActions.h"
#include "MonolithMeshSceneActions.h"
#include "MonolithMeshSpatialActions.h"
#include "MonolithMeshBlockoutActions.h"
#include "MonolithToolRegistry.h"
#include "MonolithJsonUtils.h"
#include "MonolithSettings.h"

#define LOCTEXT_NAMESPACE "FMonolithMeshModule"

void FMonolithMeshModule::StartupModule()
{
	if (!GetDefault<UMonolithSettings>()->bEnableMesh)
	{
		UE_LOG(LogMonolith, Log, TEXT("Monolith — Mesh module disabled via settings"));
		return;
	}

	FMonolithMeshInspectionActions::RegisterActions(FMonolithToolRegistry::Get());
	FMonolithMeshSceneActions::RegisterActions(FMonolithToolRegistry::Get());
	FMonolithMeshSpatialActions::RegisterActions(FMonolithToolRegistry::Get());
	FMonolithMeshBlockoutActions::RegisterActions(FMonolithToolRegistry::Get());

	UE_LOG(LogMonolith, Log, TEXT("Monolith — Mesh module loaded (46 actions)"));
}

void FMonolithMeshModule::ShutdownModule()
{
	FMonolithToolRegistry::Get().UnregisterNamespace(TEXT("mesh"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMonolithMeshModule, MonolithMesh)
