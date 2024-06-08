#pragma once
#include "Platform/W32/IApplication.h"

class Application : public IApplication
{
public:
	Application();
	~Application();
public:
	VOID Initialize();
	VOID Update();
};