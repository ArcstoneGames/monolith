#include "Modules/ModuleManager.h"
#include "IMonolithGraphFormatter.h"
#include "MonolithSettings.h"

#if WITH_BLUEPRINT_ASSIST
#include "MonolithBAFormatterImpl.h"
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogMonolithBABridge, Log, All);
DEFINE_LOG_CATEGORY(LogMonolithBABridge);

class FMonolithBABridgeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		const UMonolithSettings* Settings = GetDefault<UMonolithSettings>();
		if (!Settings || !Settings->bEnableBlueprintAssist)
		{
			UE_LOG(LogMonolithBABridge, Log,
				TEXT("MonolithBABridge: Blueprint Assist integration disabled in settings"));
			return;
		}

#if WITH_BLUEPRINT_ASSIST
		Formatter = MakeUnique<FMonolithBAFormatterImpl>();
		IModularFeatures::Get().RegisterModularFeature(
			IMonolithGraphFormatter::GetModularFeatureName(),
			Formatter.Get());
		UE_LOG(LogMonolithBABridge, Log,
			TEXT("MonolithBABridge: Registered BA graph formatter"));
#else
		UE_LOG(LogMonolithBABridge, Log,
			TEXT("MonolithBABridge: Blueprint Assist not found at compile time, bridge inactive"));
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_BLUEPRINT_ASSIST
		if (Formatter.IsValid())
		{
			IModularFeatures::Get().UnregisterModularFeature(
				IMonolithGraphFormatter::GetModularFeatureName(),
				Formatter.Get());
			Formatter.Reset();
		}
#endif
	}

private:
#if WITH_BLUEPRINT_ASSIST
	TUniquePtr<FMonolithBAFormatterImpl> Formatter;
#endif
};

IMPLEMENT_MODULE(FMonolithBABridgeModule, MonolithBABridge)
