#include "MonolithAnimationModule.h"
#include "MonolithJsonUtils.h"
#include "MonolithAnimationActions.h"
#include "MonolithPoseSearchActions.h"
#include "MonolithToolRegistry.h"

#define LOCTEXT_NAMESPACE "FMonolithAnimationModule"

void FMonolithAnimationModule::StartupModule()
{
	FMonolithAnimationActions::RegisterActions(FMonolithToolRegistry::Get());
	FMonolithPoseSearchActions::RegisterActions(FMonolithToolRegistry::Get());
	UE_LOG(LogMonolith, Verbose, TEXT("Monolith — Animation module loaded (63 actions)"));
}

void FMonolithAnimationModule::ShutdownModule()
{
	FMonolithToolRegistry::Get().UnregisterNamespace(TEXT("animation"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMonolithAnimationModule, MonolithAnimation)
