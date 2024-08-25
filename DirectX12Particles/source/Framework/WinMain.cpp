#include "pch.h"

#include "Application.h"
#include "../ParticleGame/ParticleGame.h"

void ReportLiveObjects()
{
	IDXGIDebug1* dxgiDebug;
	DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));

	dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
	dxgiDebug->Release();
}

INT CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, INT)
{
	int retCode = 0;

	Application::Create(HInstance());
	{
		std::shared_ptr<ParticleGame> demo = std::make_shared<ParticleGame>(L"PARTICLE SIM", 600, 600, true);
		retCode = Application::Get().Run(demo);
	}
	Application::Destroy();

#ifdef _DEBUG
	atexit(&ReportLiveObjects);
#endif

	return retCode;
}