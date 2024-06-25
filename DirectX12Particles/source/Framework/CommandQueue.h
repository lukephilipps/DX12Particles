#pragma once

#include <queue>

class CommandQueue
{
public:

	CommandQueue(D3D12_COMMAND_LIST_TYPE type);
	virtual ~CommandQueue();

	ComPtr<ID3D12GraphicsCommandList2> GetCommandList();

	uint64_t ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList2> commandList);

	uint64_t Signal();
	bool IsFenceCompleted(uint64_t fenceValue);
	void WaitForFenceValue(uint64_t fenceValue);
	void Wait(ComPtr<ID3D12Fence> fence, uint64_t fenceValue);
	void Flush();

	ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const;
	ComPtr<ID3D12Fence> GetD3D12Fence() const;

protected:

	ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();
	ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(ComPtr<ID3D12CommandAllocator> commandAllocator);

private:

	// Keeps track of in-flight command allocators
	struct CommandAllocatorEntry
	{
		uint64_t fenceValue;
		ComPtr<ID3D12CommandAllocator> commandAllocator;
	};

	using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
	using CommandListQueue = std::queue<ComPtr<ID3D12GraphicsCommandList2>>;

	D3D12_COMMAND_LIST_TYPE		CommandListType;
	ComPtr<ID3D12CommandQueue>	d3d12CommandQueue;
	ComPtr<ID3D12Fence>			d3d12Fence;
	HANDLE						FenceEvent;
	uint64_t					FenceValue;

	CommandAllocatorQueue		CAllocatorQueue;
	CommandListQueue			CListQueue;
};