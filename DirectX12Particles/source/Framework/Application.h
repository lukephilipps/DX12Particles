#pragma once

class Window;
class Game;
class CommandQueue;

class Application
{
public:

	// Application singleton functions
	static void Create(HINSTANCE hInst);
	static void Destroy();
	static Application& Get();

	// Check if !VSync is supported
	bool IsTearingSupported() const;

	// Window class functions
	std::shared_ptr<Window> CreateRenderWindow(const std::wstring& windowName, int windowWidth, int windowHeight, bool vSync = true);
	void DestroyWindow(const std::wstring& windowName);
	void DestroyWindow(const std::shared_ptr<Window> window);
	std::shared_ptr<Window> GetWindowByName(const std::wstring& windowName);

	// Run the application loop
	int Run(const std::shared_ptr<Game> game);

	// Request to quit the application and close windows
	void Quit(int exitCode = 0);

	// D3D12 Getters
	ComPtr<ID3D12Device2> GetDevice() const;
	std::shared_ptr<CommandQueue> GetCommandQueue(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) const;

	// Flush command queues
	void Flush();

	// Descriptor heap functions
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
	UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const;

protected:

	Application(HINSTANCE hInst);
	virtual ~Application();

	void Initialize();

	ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp);
	ComPtr<ID3D12Device2> CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter);
	bool CheckTearingSupport();

private:

	Application(const Application& copy) = delete;
	Application& operator=(const Application& other) = delete;

	HINSTANCE hInstance;

	Microsoft::WRL::ComPtr<ID3D12Device2> d3d12Device;

	std::shared_ptr<CommandQueue> DirectCommandQueue;
	std::shared_ptr<CommandQueue> ComputeCommandQueue;
	std::shared_ptr<CommandQueue> CopyCommandQueue;

	bool TearingSupported;
};