#pragma once

#include "Modules/ModuleManager.h"

class FMonolithLevelSequenceModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	FDelegateHandle PostEngineInitHandle;
};
