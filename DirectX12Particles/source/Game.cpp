#include "pch.h"
#include "Game.h"
#include "Application.h"
#include "Window.h"

Game::Game(const std::wstring& name, int width, int height, bool vSync)
	: Name(name)
	, Width(width)
	, Height(height)
	, VSync(vSync)
{

}

Game::~Game()
{
	assert(!pWindow && "Use Game::Destroy() before destruction.");
}

bool Game::Initialize()
{
	if (!DirectX::XMVerifyCPUSupport())
	{
		MessageBoxA(NULL, "Failed to verify DirectX Math library support.", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	pWindow = Application::Get().CreateRenderWindow(Name, Width, Height, VSync);
	pWindow->RegisterCallbacks(shared_from_this());
	pWindow->Show();

	return true;
}

void Game::Destroy()
{
	Application::Get().DestroyWindow(pWindow);
	pWindow.reset();
}

// By default, don't respond to most events
void Game::OnUpdate(UpdateEventArgs& e) {}
void Game::OnRender(RenderEventArgs& e) {}
void Game::OnKeyPressed(KeyEventArgs& e) {}
void Game::OnKeyReleased(KeyEventArgs& e) {}
void Game::OnMouseMoved(MouseMotionEventArgs& e) {}
void Game::OnMouseButtonPressed(MouseButtonEventArgs& e) {}
void Game::OnMouseButtonReleased(MouseButtonEventArgs& e) {}
void Game::OnMouseWheel(MouseWheelEventArgs& e) {}

void Game::OnResize(ResizeEventArgs& e)
{
	Width = e.Width;
	Height = e.Height;
}

void Game::OnWindowDestroy()
{
	UnloadContent();
}