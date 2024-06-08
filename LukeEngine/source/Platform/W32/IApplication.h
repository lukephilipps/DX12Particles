#pragma once

class LUKE_API IApplication
{
public:
	IApplication();
	virtual ~IApplication() = 0;
public:
	virtual VOID Initialize() = 0;
	virtual VOID Update() = 0;
};