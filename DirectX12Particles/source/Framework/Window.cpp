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

			UINT windowStyle = WS_OVERLAPPED;

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

void Window::OnRender(RenderEventArgs&)
{
	RenderClock.Tick();

	if (auto game = pGame.lock())
	{
		++FrameCounter;

		RenderEventArgs renderEventArgs(RenderClock.GetDeltaSeconds(), RenderClock.GetTotalSeconds());
		game->OnRender(renderEventArgs);
	}
}

void Window::OnKeyPressed(KeyEventArgs& e)
{
	if (auto game = pGame.lock())
	{
		game->OnKeyPressed(e);
	}
}

void Window::OnKeyReleased(KeyEventArgs& e)
{
	if (auto game = pGame.lock())
	{
		game->OnKeyReleased(e);
	}
}

void Window::OnMouseMoved(MouseMotionEventArgs& e)
{
	if (auto game = pGame.lock())
	{
		game->OnMouseMoved(e);
	}
}

void Window::OnMouseButtonPressed(MouseButtonEventArgs& e)
{
	if (auto game = pGame.lock())
	{
		game->OnMouseButtonPressed(e);
	}
}

void Window::OnMouseButtonReleased(MouseButtonEventArgs& e)
{
	if (auto game = pGame.lock())
	{
		game->OnMouseButtonReleased(e);
	}
}

void Window::OnMouseWheel(MouseWheelEventArgs& e)
{
	if (auto game = pGame.lock())
	{
		game->OnMouseWheel(e);
	}
}

void Window::OnResize(ResizeEventArgs& e)
{
	if (WindowWidth != e.Width || WindowHeight != e.Height)
	{
		WindowWidth = max(1, e.Width);
		WindowHeight = max(1, e.Height);

		Application::Get().Flush();

		for (int i = 0; i < BufferCount; ++i)
		{
			d3d12BackBuffers[i].Reset();
		}

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		ThrowIfFailed(dxgiSwapChain->GetDesc(&swapChainDesc));
		ThrowIfFailed(dxgiSwapChain->ResizeBuffers(BufferCount, WindowWidth, WindowHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

		// Current back buffer index could have changed, get current index known by app
		CurrentBackBufferIndex = dxgiSwapChain->GetCurrentBackBufferIndex();

		UpdateRenderTargetViews();
	}

	if (auto game = pGame.lock())
	{
		game->OnResize(e);
	}
}

ComPtr<IDXGISwapChain4> Window::CreateSwapChain()
{
	Application& app = Application::Get();

	ComPtr<IDXGISwapChain4> dxgiSwapChain4;
	ComPtr<IDXGIFactory4> dxgiFactory4;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = WindowWidth;
	swapChainDesc.Height = WindowHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc = { 1, 0 }; // 1, 0 for flip model swap chain
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = BufferCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = IsTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ID3D12CommandQueue* pCommandQueue = app.GetCommandQueue()->GetD3D12CommandQueue().Get();

	ComPtr<IDXGISwapChain1> swapChain1;
	ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(pCommandQueue, WindowHandle, &swapChainDesc, nullptr, nullptr, &swapChain1));

	// Disable alt+Enter fullscreen
	ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(WindowHandle, DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));

	CurrentBackBufferIndex = dxgiSwapChain4->GetCurrentBackBufferIndex();

	return dxgiSwapChain4;
}

void Window::UpdateRenderTargetViews()
{
	auto device = Application::Get().GetDevice();

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(d3d12RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < BufferCount; ++i)
	{
		ComPtr<ID3D12Resource> backBuffer;
		ThrowIfFailed(dxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

		device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

		d3d12BackBuffers[i] = backBuffer;

		rtvHandle.Offset(RTVDescriptorSize);
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE Window::GetCurrentRenderTargetView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(d3d12RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), CurrentBackBufferIndex, RTVDescriptorSize);
}

ComPtr<ID3D12Resource> Window::GetCurrentBackBuffer() const
{
	return d3d12BackBuffers[CurrentBackBufferIndex];
}

UINT Window::GetCurrentBackBufferIndex() const
{
	return CurrentBackBufferIndex;
}

UINT Window::Present()
{
	UINT syncInterval = VSync ? 1 : 0;
	UINT presentFlags = IsTearingSupported && !VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	ThrowIfFailed(dxgiSwapChain->Present(syncInterval, presentFlags));

	CurrentBackBufferIndex = dxgiSwapChain->GetCurrentBackBufferIndex();

	return CurrentBackBufferIndex;
}