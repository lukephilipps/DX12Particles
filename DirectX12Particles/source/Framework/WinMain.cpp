#include "pch.h"

#include "Application.h"
#include "../CubeRenderer/CubeRenderer.h"

//#include "dxdidebug.h"

INT CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, INT)
{
	int retCode = 0;

	Application::Create(HInstance());
	{
		std::shared_ptr<CubeRenderer> demo = std::make_shared<CubeRenderer>(L"ROTATING CUBE", 1280, 720);
		retCode = Application::Get().Run(demo);
	}
	Application::Destroy();

	//atexit(&ReportLiveObjects);

	return retCode;
}