#include "MonolithNiagaraModule.h"
#include "MonolithJsonUtils.h"
#include "MonolithNiagaraActions.h"
#include "MonolithNiagaraLayoutActions.h"
#include "MonolithToolRegistry.h"

#define LOCTEXT_NAMESPACE "FMonolithNiagaraModule"

void FMonolithNiagaraModule::StartupModule()
{
	FMonolithToolRegistry& Registry = FMonolithToolRegistry::Get();
	FMonolithNiagaraActions::RegisterActions(Registry);
	FMonolithNiagaraLayoutActions::RegisterActions(Registry);
	UE_LOG(LogMonolith, Verbose, TEXT("Monolith — Niagara module loaded (42 actions)"));
}

void FMonolithNiagaraModule::ShutdownModule()
{
	FMonolithToolRegistry::Get().UnregisterNamespace(TEXT("niagara"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMonolithNiagaraModule, MonolithNiagara)
