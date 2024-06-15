#include "pch.h"
#include "CommandQueue.h"
#include "Application.h"

CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE type)
	: FenceValue(0)
	, CommandListType(type)
{
	auto device = Application::Get().GetDevice();

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12CommandQueue)));
	ThrowIfFailed(device->CreateFence(FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12Fence)));

	FenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(FenceEvent && "Failed to create fence event.");
}

CommandQueue::~CommandQueue()
{

}

uint64_t CommandQueue::Signal()
{
	uint64_t fenceValue = ++FenceValue;
	ThrowIfFailed(d3d12CommandQueue->Signal(d3d12Fence.Get(), fenceValue));

	return fenceValue;
}

bool CommandQueue::IsFenceCompleted(uint64_t fenceValue)
{
	return d3d12Fence->GetCompletedValue() >= fenceValue;
}

void CommandQueue::WaitForFenceValue(uint64_t fenceValue)
{
	if (!IsFenceCompleted(fenceValue))
	{
		d3d12Fence->SetEventOnCompletion(fenceValue, FenceEvent);
		::WaitForSingleObject(FenceEvent, DWORD_MAX);
	}
}

void CommandQueue::Flush()
{
	WaitForFenceValue(Signal());
}

ComPtr<ID3D12CommandAllocator> CommandQueue::CreateCommandAllocator()
{
	auto device = Application::Get().GetDevice();

	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ThrowIfFailed(device->CreateCommandAllocator(CommandListType, IID_PPV_ARGS(&commandAllocator)));

	return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList2> CommandQueue::CreateCommandList(ComPtr<ID3D12CommandAllocator> allocator)
{
	auto device = Application::Get().GetDevice();

	ComPtr<ID3D12GraphicsCommandList2> commandList;
	ThrowIfFailed(device->CreateCommandList(0, CommandListType, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

	return commandList;
}

ComPtr<ID3D12GraphicsCommandList2> CommandQueue::GetCommandList()
{
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<ID3D12GraphicsCommandList2> commandList;

	if (!CAllocatorQueue.empty() && IsFenceCompleted(CAllocatorQueue.front().fenceValue))
	{
		commandAllocator = CAllocatorQueue.front().commandAllocator;
		CAllocatorQueue.pop();

		ThrowIfFailed(commandAllocator->Reset());
	}
	else
	{
		commandAllocator = CreateCommandAllocator();
	}

	if (!CListQueue.empty())
	{
		commandList = CListQueue.front();
		CListQueue.pop();

		ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));
	}
	else
	{
		commandList = CreateCommandList(commandAllocator);
	}

	ThrowIfFailed(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get()));

	return commandList;
}

uint64_t CommandQueue::ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList2> commandList)
{
	commandList->Close();

	ID3D12CommandAllocator* commandAllocator;
	UINT dataSize = sizeof(commandAllocator);
	ThrowIfFailed(commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &commandAllocator));

	ID3D12CommandList* const commandLists[] =
	{
		commandList.Get()
	};

	d3d12CommandQueue->ExecuteCommandLists(1, commandLists);
	uint64_t fenceValue = Signal();

	CAllocatorQueue.emplace(CommandAllocatorEntry{ fenceValue, commandAllocator });
	CListQueue.push(commandList);

	commandAllocator->Release();

	return fenceValue;
}

ComPtr<ID3D12CommandQueue> CommandQueue::GetD3D12CommandQueue() const
{
	return d3d12CommandQueue;
}