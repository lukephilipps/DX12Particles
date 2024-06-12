#include "pch.h"
#include "Window.h"
#include "Application.h"
#include "CommandQueue.h"
#include "Game.h"

Window::Window(HWND hWnd, const std::wstring& windowName, int windowWidth, int windowHeight, bool vSync)
	: WindowHandle(hWnd)
	, WindowName(windowName)
	, WindowWidth(windowWidth)
	, WindowHeight(windowHeight)
	, VSync(vSync)
	, FullScreen(false)
	, FrameCounter(0)
{
	Application& app = Application::Get();

	IsTearingSupported = app.IsTearingSupported();

	dxgiSwapChain = CreateSwapChain();
	d3d12RTVDescriptorHeap = app.CreateDescriptorHeap(BufferCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	RTVDescriptorSize = app.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	UpdateRenderTargetViews();
}

Window::~Window()
{
	assert(!WindowHandle && "Use Application::DestroyWindow before destruction.");
}

HWND Window::GetWindowHandle() const
{
	return WindowHandle;
}

const std::wstring& Window::GetWindowName() const
{
	return WindowName;
}

void Window::Show()
{
	::ShowWindow(WindowHandle, SW_SHOW);
}

void Window::Hide()
{
	::ShowWindow(WindowHandle, SW_HIDE);
}

void Window::Destroy()
{
	if (auto game = pGame.lock())
	{
		game->OnWindowDestroy();
	}

	for (int i = 0; i < BufferCount; ++i)
	{
		auto resource = d3d12BackBuffers[i].Get();
		d3d12BackBuffers[i].Reset();
	}

	if (WindowHandle)
	{
		DestroyWindow(WindowHandle);
		WindowHandle = nullptr;
	}
}

int Window::GetWindowWidth() const
{
	return WindowWidth;
}

int Window::GetWindowHeight() const
{
	return WindowHeight;
}

bool Window::IsVSync() const
{
	return VSync;
}

void Window::SetVSync(bool vSync)
{
	VSync = vSync;
}

void Window::ToggleVSync()
{
	SetVSync(!VSync);
}

bool Window::IsFullScreen() const
{
	return FullScreen;
}

void Window::SetFullScreen(bool fullScreen)
{
	if (FullScreen != fullScreen)
	{
		FullScreen = fullScreen;

		if (FullScreen)
		{
			// Store restorable window dimensions
			::GetWindowRect(WindowHandle, &WindowRect);

			UINT windowStyle = WS_OVERLAPPEDWINDOW & (WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

			::SetWindowLongW(WindowHandle, GWL_STYLE, windowStyle);

			HMONITOR hMonitor = ::MonitorFromWindow(WindowHandle, MONITOR_DEFAULTTONEAREST);
			MONITORINFOEX monitorInfo = {};
			monitorInfo.cbSize = sizeof(MONITORINFOEX);
			::GetMonitorInfo(hMonitor, &monitorInfo);

			::SetWindowPos(WindowHandle, HWND_TOP,
				monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.top,
				monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE);

			::ShowWindow(WindowHandle, SW_MAXIMIZE);
		}
		else
		{
			::SetWindowLong(WindowHandle, GWL_STYLE, WS_OVERLAPPEDWINDOW);

			::SetWindowPos(WindowHandle, HWND_NOTOPMOST,
				WindowRect.left,
				WindowRect.top,
				WindowRect.right - WindowRect.left,
				WindowRect.bottom - WindowRect.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE);

			::ShowWindow(WindowHandle, SW_NORMAL);
		}
	}
}

void Window::ToggleFullScreen()
{
	SetFullScreen(!FullScreen);
}

void Window::RegisterCallbacks(std::shared_ptr<Game> game)
{
	pGame = game;
}

void Window::OnUpdate(UpdateEventArgs&)
{
	UpdateClock.Tick();

	if (auto game = pGame.lock())
	{
		++FrameCounter;

		UpdateEventArgs updateEventArgs(UpdateClock.GetDeltaSeconds(), UpdateClock.GetTotalSeconds());
		game->OnUpdate(updateEventArgs);
	}
}