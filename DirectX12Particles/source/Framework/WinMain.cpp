#include "pch.h"

#include "Application.h"
#include "../CubeRenderer/CubeRenderer.h"

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
		std::shared_ptr<CubeRenderer> demo = std::make_shared<CubeRenderer>(L"ROTATING CUBE", 1280, 720);
		retCode = Application::Get().Run(demo);
	}
	Application::Destroy();

#ifdef _DEBUG
	atexit(&ReportLiveObjects);
#endif

	return retCode;
}