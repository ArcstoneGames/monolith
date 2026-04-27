#include "MonolithLevelSequenceModule.h"
#include "MonolithLevelSequenceActions.h"
#include "MonolithLevelSequenceIndexer.h"
#include "MonolithToolRegistry.h"
#include "MonolithJsonUtils.h"
#include "MonolithSettings.h"
#include "MonolithIndexSubsystem.h"
#include "Editor.h"
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "FMonolithLevelSequenceModule"

void FMonolithLevelSequenceModule::StartupModule()
{
	const UMonolithSettings* Settings = GetDefault<UMonolithSettings>();
	if (!Settings) return;

	if (Settings->bEnableLevelSequence)
	{
		FMonolithLevelSequenceActions::RegisterActions(FMonolithToolRegistry::Get());
		UE_LOG(LogMonolith, Log, TEXT("Monolith — LevelSequence module loaded (1 action)"));
	}

	if (Settings->bIndexLevelSequences)
	{
		PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddLambda([this]()
		{
			if (GEditor)
			{
				if (UMonolithIndexSubsystem* IndexSS = GEditor->GetEditorSubsystem<UMonolithIndexSubsystem>())
				{
					IndexSS->RegisterIndexer(MakeShared<FLevelSequenceIndexer>());
					UE_LOG(LogMonolith, Log, TEXT("MonolithLevelSequence: Registered FLevelSequenceIndexer into MonolithIndex"));
				}
			}
		});
	}
}

void FMonolithLevelSequenceModule::ShutdownModule()
{
	if (PostEngineInitHandle.IsValid())
	{
		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}

	FMonolithToolRegistry::Get().UnregisterNamespace(TEXT("level_sequence"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMonolithLevelSequenceModule, MonolithLevelSequence)
