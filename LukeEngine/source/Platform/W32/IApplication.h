#pragma once

class IApplication
{
private:
	IApplication();
	virtual ~IApplication() = 0;
public:
	virtual VOID Initialize() = 0;
	virtual VOID Update() = 0;
};