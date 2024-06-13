#pragma once

#include <Windows.h>
#include <wrl.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include "Events.h"
#include "HighResolutionClock.h"

#include <string>
#include <memory>

class Game;

class Window
{
public:
	static const UINT BufferCount = 3;

	HWND GetWindowHandle() const;

	void Destroy();

	const std::wstring& GetWindowName() const;

	int GetWindowWidth() const;
	int GetWindowHeight() const;

	bool IsVSync() const;
	void SetVSync(bool vSync);
	void ToggleVSync();

	bool IsFullScreen() const;

	void SetFullScreen(bool fullScreen);
	void ToggleFullScreen();

	void Show();
	void Hide();

	UINT GetCurrentBackBufferIndex() const;

    // Present the swapchains back buffer to the screen and return current back buffer index
	UINT Present();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView() const;

	ComPtr<ID3D12Resource> GetCurrentBackBuffer() const;

protected:

    friend LRESULT CALLBACK WindowProcess(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    friend class Application;
    friend class Game;

    Window() = delete;
    Window(HWND hWnd, const std::wstring& windowName, int windowWidth, int windowHeight, bool vSync);
    virtual ~Window();

    // Register a Game with this window. This allows
    // the window to callback functions in the Game class.
    void RegisterCallbacks(std::shared_ptr<Game> game);

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

    ComPtr<IDXGISwapChain4> CreateSwapChain();

    void UpdateRenderTargetViews();

private:

    Window(const Window& copy) = delete;
    Window& operator=(const Window& other) = delete;

    HWND WindowHandle;

    std::wstring WindowName;

    int WindowWidth;
    int WindowHeight;
    bool VSync;
    bool FullScreen;

    HighResolutionClock UpdateClock;
    HighResolutionClock RenderClock;
    uint64_t FrameCounter;

    std::weak_ptr<Game> pGame;

    ComPtr<IDXGISwapChain4>         dxgiSwapChain;
    ComPtr<ID3D12DescriptorHeap>    d3d12RTVDescriptorHeap;
    ComPtr<ID3D12Resource>          d3d12BackBuffers[BufferCount];

    UINT RTVDescriptorSize;
    UINT CurrentBackBufferIndex;

    RECT WindowRect;
    bool IsTearingSupported;
};