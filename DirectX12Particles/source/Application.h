#pragma once
#include <memory>
#include <string>

class Window;

class Application
{
public:
	static void Create(HINSTANCE hInst);
	static void Destroy();
	static Application& Get();
	bool IsTearingSupported() const;

	std::shared_ptr<Window> CreateRenderWindow();
protected:

private:

};