#pragma once

#include <Events.h>
#include <memory>
#include <string>

class Window;

class Game : public std::enable_shared_from_this<Game>
{
public:

	Game(const std::wstring& name, int width, int height, bool vSync);
	virtual ~Game();

	int GetWindowWidth() const { return Width; }
	int GetWindowHeight() const { return Height; }

	virtual bool Initialize();
	virtual bool LoadContent() = 0;
	virtual void UnloadContent() = 0;
	virtual void Destroy();

protected:

	friend class Window;

	// Events
	virtual void OnUpdate(UpdateEventArgs& e);
	virtual void OnRender(RenderEventArgs& e);
	virtual void OnKeyPressed(KeyEventArgs& e);
	virtual void OnKeyReleased(KeyEventArgs& e);
	virtual void OnMouseMoved(MouseMotionEventArgs& e);
	virtual void OnMouseButtonPressed(MouseButtonEventArgs& e);
	virtual void OnMouseButtonReleased(MouseButtonEventArgs& e);
	virtual void OnMouseWheel(MouseWheelEventArgs& e);
	virtual void OnResize(ResizeEventArgs& e);

	virtual void OnWindowDestroy();

	std::shared_ptr<Window> pWindow;

private:

	std::wstring Name;
	int Width;
	int Height;
	bool VSync;
};