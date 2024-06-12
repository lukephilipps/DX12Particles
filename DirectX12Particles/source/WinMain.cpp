#include "pch.h"

/* ------------------------------------------------- */
/* Global Variables                                  */
/* ------------------------------------------------- */
#pragma region GlobalVariables
// The number of swap chain back buffers
const								uint8_t NumFrames = 3;

// Use WARP adapter
bool								UseWarp = false;

// Set to true once the DX12 objects have been initialized
bool								IsInitialized = false;

WCHAR								WindowClassName[MAX_NAME_STRING];
WCHAR								WindowTitle[MAX_NAME_STRING];
uint32_t							WindowWidth = 1280;
uint32_t							WindowHeight = 720;

HICON								hIcon;

HWND								hWnd;
RECT								WindowRect;

// DX12 objects 
ComPtr<ID3D12Device2>				Device;
ComPtr<ID3D12CommandQueue>			CommandQueue;
ComPtr<IDXGISwapChain4>				SwapChain;
ComPtr<ID3D12Resource>				BackBuffers[NumFrames];
ComPtr<ID3D12GraphicsCommandList>	CommandList;
ComPtr<ID3D12CommandAllocator>		CommandAllocators[NumFrames];
ComPtr<ID3D12DescriptorHeap>		RTVDescriptorHeap;
UINT								RTVDescriptorSize;
UINT								CurrentBackBufferIndex;

// Synchronization objects
ComPtr<ID3D12Fence> Fence;
uint64_t FenceValue = 0;
uint64_t FrameFenceValues[NumFrames] = { };
HANDLE FenceEvent;

// Swap chain present method vars
bool VSync = true;
bool TearingSupported = false;
bool Fullscreen = false;

#pragma endregion

/* ------------------------------------------------- */
/* Pre-Declarations                                  */
/* ------------------------------------------------- */
#pragma region Pre-Declarations

VOID ParseCommandLineArguments();
VOID EnableDebugLayer();
void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName);
HWND CreateWindow(const wchar_t* windowClassName, HINSTANCE hInst, const wchar_t* windowTitle, uint32_t width, uint32_t height);
ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp);
ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter);
ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
bool CheckTearingSupport();
ComPtr<IDXGISwapChain4> CreateSwapChain(HWND hWnd, ComPtr<ID3D12CommandQueue> commandQueue, uint32_t width, uint32_t height, uint32_t bufferCount);
ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDesciptors);
void UpdateRenderTargetViews(ComPtr<ID3D12Device2> device, ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap);
ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12Device2> device, ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type);
ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> device);
HANDLE CreateEventHandle();
uint64_t Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue);
void WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent, std::chrono::milliseconds duration = std::chrono::milliseconds::max());
void Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue, HANDLE fenceEvent);
void Update();
void Render();
void Resize(uint32_t width, uint32_t height);
void SetFullScreen(bool fullscreen);
LRESULT CALLBACK WindowProcess(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);

#pragma endregion

/* ------------------------------------------------- */
/* Operations                                        */
/* ------------------------------------------------- */
#pragma region Operations

INT CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, INT)
{
	LoadString(HInstance(), IDS_PERGAMENAME, WindowTitle, MAX_NAME_STRING);
	LoadString(HInstance(), IDS_WINDOWCLASS, WindowClassName, MAX_NAME_STRING);
	hIcon = LoadIcon(HInstance(), MAKEINTRESOURCE(IDI_MAINICON));

	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	const wchar_t* windowClassName = L"DX12WindowClass";
	ParseCommandLineArguments();
	EnableDebugLayer();

	TearingSupported = CheckTearingSupport();

	RegisterWindowClass(HInstance(), windowClassName);
	hWnd = CreateWindow(windowClassName, HInstance(), L"WINDOW NAMEEEEEE", WindowWidth, WindowHeight);

	::GetWindowRect(hWnd, &WindowRect);

	ComPtr<IDXGIAdapter4> dxgiAdapter4 = GetAdapter(UseWarp);

	Device = CreateDevice(dxgiAdapter4);

	CommandQueue = CreateCommandQueue(Device, D3D12_COMMAND_LIST_TYPE_DIRECT);

	SwapChain = CreateSwapChain(hWnd, CommandQueue, WindowWidth, WindowHeight, NumFrames);

	CurrentBackBufferIndex = SwapChain->GetCurrentBackBufferIndex();

	RTVDescriptorHeap = CreateDescriptorHeap(Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NumFrames);
	RTVDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	UpdateRenderTargetViews(Device, SwapChain, RTVDescriptorHeap);

	for (int i = 0; i < NumFrames; ++i)
	{
		CommandAllocators[i] = CreateCommandAllocator(Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}
	CommandList = CreateCommandList(Device, CommandAllocators[CurrentBackBufferIndex], D3D12_COMMAND_LIST_TYPE_DIRECT);

	Fence = CreateFence(Device);
	FenceEvent = CreateEventHandle();

	IsInitialized = true;

	::ShowWindow(hWnd, SW_SHOW);

	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}

	Flush(CommandQueue, Fence, FenceValue, FenceEvent);

	::CloseHandle(FenceEvent);

	return 0;
}

LRESULT CALLBACK WindowProcess(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (IsInitialized)
	{
		switch (message)
		{
		case WM_PAINT:
			Update();
			Render();
			break;
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
		{
			bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

			switch (wParam)
			{
			case 'V':
				VSync = !VSync;
				break;
			case VK_ESCAPE:
				::PostQuitMessage(0);
				break;
			case VK_RETURN:
				if (alt)
				{
			case VK_F11:
				SetFullScreen(!Fullscreen);
				}
				break;
			}
		}
			break;
		case WM_SYSCHAR:
			break;
		case WM_SIZE:
		{
			RECT clientRect = {};
			::GetClientRect(hWnd, &clientRect);

			int width = clientRect.right - clientRect.left;
			int height = clientRect.bottom - clientRect.top;

			Resize(width, height);
		}
			break;
		case WM_DESTROY:
			::PostQuitMessage(0);
			break;
		default:
			return ::DefWindowProcW(hWnd, message, wParam, lParam);
		}
	}
	else
	{
		return ::DefWindowProcW(hWnd, message, wParam, lParam);
	}

	return 0;
}

#pragma endregion

/* ------------------------------------------------- */
/* Functions                                         */
/* ------------------------------------------------- */
#pragma region Functions

VOID ParseCommandLineArguments()
{
	int argc;
	wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);

	for (size_t i = 0; i < argc; ++i)
	{
		if (::wcscmp(argv[i], L"-w") == 0 || ::wcscmp(argv[i], L"--width") == 0)
		{
			WindowWidth = ::wcstol(argv[++i], nullptr, 10);
		}
		if (::wcscmp(argv[i], L"-h") == 0 || ::wcscmp(argv[i], L"--height") == 0)
		{
			WindowHeight = ::wcstol(argv[++i], nullptr, 10);
		}
		if (::wcscmp(argv[i], L"-warp") == 0 || ::wcscmp(argv[i], L"--warp") == 0)
		{
			// Windows Advanced Rasterization Platform
			UseWarp = true;
		}
	}

	// Free memory allocated by CommandLineToArgvW
	::LocalFree(argv);
}

VOID EnableDebugLayer()
{
	//Unused atm
#if defined(_DEBUG)
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif
}

void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName)
{
	WNDCLASSEX windowClass = {};

	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = &WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = hInst;
	windowClass.hIcon = hIcon;
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	windowClass.lpszMenuName = nullptr;
	windowClass.lpszClassName = windowClassName;
	windowClass.hIconSm = hIcon;

	static ATOM atom = ::RegisterClassExW(&windowClass);
	assert(atom > 0);
}

HWND CreateWindow(const wchar_t* windowClassName, HINSTANCE hInst, const wchar_t* windowTitle, uint32_t width, uint32_t height)
{
	int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

	RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
	::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	int windowWidth = windowRect.right - windowRect.left;
	int windowHeight = windowRect.bottom - windowRect.top;

	int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
	int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

	HWND hWnd = ::CreateWindowExW(
		NULL,
		windowClassName,
		windowTitle,
		WS_OVERLAPPEDWINDOW,
		windowX,
		windowY,
		windowWidth,
		windowHeight,
		NULL,
		NULL,
		hInst,
		nullptr
	);

	assert(hWnd && "Failed to create window");

	return hWnd;
}

ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp)
{
	ComPtr<IDXGIFactory4> dxgiFactory;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	ComPtr<IDXGIAdapter1> dxgiAdapter1;
	ComPtr<IDXGIAdapter4> dxgiAdapter4;

	if (useWarp)
	{
		ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
		ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
	}
	else
	{
		SIZE_T maxDedicatedVideoMemory = 0;
		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			// Check if adapter is D3D12 compatible without creating it in memory.
			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
					SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
						dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
			}
		}
	}

	return dxgiAdapter4;
}

ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter)
{
	ComPtr<ID3D12Device2> d3d12Device2;
	ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));

#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> pInfoQueue;
	if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
	{
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// Suppress categories of messages
		// D3D12_MESSAGE_CATEGORY Categories[] = {};

		// Ignore messages of severity
		D3D12_MESSAGE_SEVERITY Severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Ignore messages of ID
		D3D12_MESSAGE_ID DenyIds[] =
		{
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
		};

		D3D12_INFO_QUEUE_FILTER NewFilter = {};
		//NewFilter.DenyList.NumCategories = _countof(Categories);
		//NewFilter.DenyList.pCategoryList = Categories;
		NewFilter.DenyList.NumSeverities = _countof(Severities);
		NewFilter.DenyList.pSeverityList = Severities;
		NewFilter.DenyList.NumIDs = _countof(DenyIds);
		NewFilter.DenyList.pIDList = DenyIds;

		ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
	}
#endif

	return d3d12Device2;
}

ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12CommandQueue> d3d12CommandQueue;

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12CommandQueue)));

	return d3d12CommandQueue;
}

bool CheckTearingSupport()
{
	BOOL allowTearing = FALSE;

	ComPtr<IDXGIFactory4> factory4;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
	{
		ComPtr<IDXGIFactory5> factory5;
		if (SUCCEEDED(factory4.As(&factory5)))
		{
			if (FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
			{
				allowTearing = FALSE;
			}
		}
	}

	return allowTearing == TRUE;
}

ComPtr<IDXGISwapChain4> CreateSwapChain(HWND hWnd, ComPtr<ID3D12CommandQueue> commandQueue, uint32_t width, uint32_t height, uint32_t bufferCount)
{
	ComPtr<IDXGISwapChain4> dxgiSwapChain4;
	ComPtr<IDXGIFactory4> dxgiFactory4;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc = {1, 0}; // 1, 0 for flip model swap chain
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = bufferCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ComPtr<IDXGISwapChain1> swapChain1;
	ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(commandQueue.Get(), hWnd, &swapChainDesc, nullptr, nullptr, &swapChain1));

	// Disable alt+Enter fullscreen
	ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));

	return dxgiSwapChain4;
}

ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDesciptors)
{
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDesciptors;
	desc.Type = type;

	ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

	return descriptorHeap;
}

void UpdateRenderTargetViews(ComPtr<ID3D12Device2> device, ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap)
{
	auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < NumFrames; ++i)
	{
		ComPtr<ID3D12Resource> backBuffer;
		ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

		device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

		BackBuffers[i] = backBuffer;

		rtvHandle.Offset(rtvDescriptorSize);
	}
}

ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));

	return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12Device2> device, ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12GraphicsCommandList> commandList;
	ThrowIfFailed(device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

	ThrowIfFailed(commandList->Close());

	return commandList;
}

ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> device)
{
	ComPtr<ID3D12Fence> fence;

	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

	return fence;
}

HANDLE CreateEventHandle()
{
	HANDLE fenceEvent;

	fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent && "Failed to create fence event.");

	return fenceEvent;
}

uint64_t Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue)
{
	uint64_t fenceValueForSignal = ++fenceValue;
	ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceValueForSignal));

	return fenceValueForSignal;
}

void WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent, std::chrono::milliseconds duration)
{
	if (fence->GetCompletedValue() < fenceValue)
	{
		ThrowIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
		::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
	}
}

void Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue, HANDLE fenceEvent)
{
	uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
	WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
}

void Update()
{
	static uint64_t frameCounter = 0;
	static double elapsedSeconds = 0.0;
	static std::chrono::high_resolution_clock clock;
	static auto t0 = clock.now();

	++frameCounter;
	auto t1 = clock.now();
	auto deltaTime = t1 - t0;
	t0 = t1;

	elapsedSeconds += deltaTime.count() * 1e-9;
	if (elapsedSeconds > 1.0)
	{
		char buffer[500];
		auto fps = frameCounter / elapsedSeconds;
		sprintf_s(buffer, 500, "FPS: %f\n", fps);
		OutputDebugStringA(buffer);

		frameCounter = 0;
		elapsedSeconds = 0.0;
	}
}

void Render()
{
	auto commandAllocator = CommandAllocators[CurrentBackBufferIndex];
	auto backBuffer = BackBuffers[CurrentBackBufferIndex];

	// Prepare command list for recording next frame
	commandAllocator->Reset();
	CommandList->Reset(commandAllocator.Get(), nullptr);

	// Must transition RT to RENDER_TARGET state before clearing
	{
		// Create a transition resource barrier
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		CommandList->ResourceBarrier(1, &barrier);

		FLOAT clearColor[] = { 0.4f, 0.6f, 0.2f, 1.0f };
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), CurrentBackBufferIndex, RTVDescriptorSize);

		CommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
	}

	// Present the RTV
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		CommandList->ResourceBarrier(1, &barrier);

		ThrowIfFailed(CommandList->Close());

		ID3D12CommandList* const commandLists[] = {
			CommandList.Get()
		};
		CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

		UINT syncInterval = VSync ? 1 : 0;
		UINT presentFlags = TearingSupported && !VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		ThrowIfFailed(SwapChain->Present(syncInterval, presentFlags));

		FrameFenceValues[CurrentBackBufferIndex] = Signal(CommandQueue, Fence, FenceValue);

		// Gets the correct back buffer index as FLIP_DISCARD model can be nonsequential
		CurrentBackBufferIndex = SwapChain->GetCurrentBackBufferIndex();

		WaitForFenceValue(Fence, FrameFenceValues[CurrentBackBufferIndex], FenceEvent);
	}
}

void Resize(uint32_t width, uint32_t height)
{
	if (WindowWidth != width || WindowHeight != height)
	{
		WindowWidth = std::max(1u, width);
		WindowHeight = std::max(1u, height);

		Flush(CommandQueue, Fence, FenceValue, FenceEvent);

		for (int i = 0; i < NumFrames; ++i)
		{
			// Release all back buffers and set all fence values to currentBackBuffer
			BackBuffers[i].Reset();
			FrameFenceValues[i] = FrameFenceValues[CurrentBackBufferIndex];
		}

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		ThrowIfFailed(SwapChain->GetDesc(&swapChainDesc));
		ThrowIfFailed(SwapChain->ResizeBuffers(NumFrames, WindowWidth, WindowHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

		// Current back buffer index could have changed, get current index known by app
		CurrentBackBufferIndex = SwapChain->GetCurrentBackBufferIndex();

		UpdateRenderTargetViews(Device, SwapChain, RTVDescriptorHeap);
	}
}

void SetFullScreen(bool fullscreen)
{
	if (Fullscreen != fullscreen)
	{
		Fullscreen = fullscreen;

		if (Fullscreen)
		{
			// Store restorable window dimensions
			::GetWindowRect(hWnd, &WindowRect);

			UINT windowStyle = WS_OVERLAPPEDWINDOW & (WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

			::SetWindowLongW(hWnd, GWL_STYLE, windowStyle);

			HMONITOR hMonitor = ::MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
			MONITORINFOEX monitorInfo = {};
			monitorInfo.cbSize = sizeof(MONITORINFOEX);
			::GetMonitorInfo(hMonitor, &monitorInfo);

			::SetWindowPos(hWnd, HWND_TOP,
				monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.top,
				monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE);

			::ShowWindow(hWnd, SW_MAXIMIZE);
		}
		else
		{
			::SetWindowLong(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

			::SetWindowPos(hWnd, HWND_NOTOPMOST,
				WindowRect.left,
				WindowRect.top,
				WindowRect.right - WindowRect.left,
				WindowRect.bottom - WindowRect.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE);

			::ShowWindow(hWnd, SW_NORMAL);
		}
	}
}

#pragma endregion