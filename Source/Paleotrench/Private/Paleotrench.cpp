#include "Paleotrench.h"

#include "Modules/ModuleManager.h"
#include "PaleotrenchCore/Version.h"

DEFINE_LOG_CATEGORY(LogPaleotrench);

class FPaleotrenchModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		const bool bCoreOk = pt::RunSelfCheck();
		if (bCoreOk)
		{
			UE_LOG(LogPaleotrench, Display, TEXT("Paleotrench core %s linked - self-check PASSED"),
				ANSI_TO_TCHAR(pt::CoreVersion()));
		}
		else
		{
			UE_LOG(LogPaleotrench, Error,
				TEXT("Paleotrench core %s self-check FAILED - generated caves would differ on this machine"),
				ANSI_TO_TCHAR(pt::CoreVersion()));
		}
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FPaleotrenchModule, Paleotrench, "Paleotrench");
