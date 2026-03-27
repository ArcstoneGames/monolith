#include "MonolithMeshModule.h"
#include "MonolithMeshInspectionActions.h"
#include "MonolithMeshSceneActions.h"
#include "MonolithMeshSpatialActions.h"
#include "MonolithMeshBlockoutActions.h"
#include "MonolithMeshHorrorActions.h"
#include "MonolithMeshAccessibilityActions.h"
#include "MonolithToolRegistry.h"
#include "MonolithJsonUtils.h"
#include "MonolithSettings.h"

#if WITH_GEOMETRYSCRIPT
#include "MonolithMeshOperationActions.h"
#include "MonolithMeshHandlePool.h"
#endif

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
	FMonolithMeshHorrorActions::RegisterActions(FMonolithToolRegistry::Get());
	FMonolithMeshAccessibilityActions::RegisterActions(FMonolithToolRegistry::Get());

#if WITH_GEOMETRYSCRIPT
	HandlePool = NewObject<UMonolithMeshHandlePool>();
	HandlePool->AddToRoot();
	HandlePool->Initialize();
	FMonolithMeshOperationActions::SetHandlePool(HandlePool);
	FMonolithMeshOperationActions::RegisterActions(FMonolithToolRegistry::Get());
	UE_LOG(LogMonolith, Log, TEXT("Monolith — Mesh operations enabled (GeometryScript available)"));
#endif

	UE_LOG(LogMonolith, Log, TEXT("Monolith — Mesh module loaded (%d actions)"),
		FMonolithToolRegistry::Get().GetActions(TEXT("mesh")).Num());
}

void FMonolithMeshModule::ShutdownModule()
{
#if WITH_GEOMETRYSCRIPT
	if (HandlePool)
	{
		HandlePool->Teardown();
		HandlePool->RemoveFromRoot();
		HandlePool = nullptr;
		FMonolithMeshOperationActions::SetHandlePool(nullptr);
	}
#endif

	FMonolithToolRegistry::Get().UnregisterNamespace(TEXT("mesh"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMonolithMeshModule, MonolithMesh)
