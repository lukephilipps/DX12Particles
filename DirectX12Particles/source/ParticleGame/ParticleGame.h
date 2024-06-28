#pragma once

#include "Game.h"
#include "Window.h"

using namespace DirectX;

class ParticleGame : public Game
{
public:

	using super = Game;

	ParticleGame(const std::wstring& name, int width, int height, bool vSync = false);
	virtual bool LoadContent() override;
	virtual void UnloadContent() override;

protected:

	virtual void OnUpdate(UpdateEventArgs& e) override;
	virtual void OnRender(RenderEventArgs& e) override;
	virtual void OnKeyPressed(KeyEventArgs& e) override;
	virtual void OnMouseWheel(MouseWheelEventArgs& e) override;
	virtual void OnResize(ResizeEventArgs& e) override;

private:

	// Transition a resource's state
	void TransitionResource(ComPtr<ID3D12GraphicsCommandList2> commandList, ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState);

	// Create a GPU buffer
	void UpdateBufferResource(ComPtr<ID3D12GraphicsCommandList2> commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource, 
		size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	// Resize the depth buffer to match the window area
	void ResizeDepthBuffer(int width, int height);

	struct Particle
	{
		XMFLOAT3 velocity;
		float age;
	};

	// Root constants for the compute shader.
	struct CSRootConstants
	{
		float x;
		float commandCount;
	};

	// Data structure to match the command signature used for ExecuteIndirect.
	struct IndirectCommand
	{
		D3D12_GPU_VIRTUAL_ADDRESS cbv;
		D3D12_INDEX_BUFFER_VIEW ibv;
		D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
		UINT padding[1];
	};

	// Constant buffer definition.
	struct SceneConstantBuffer
	{
		XMFLOAT4 rotation;
		XMFLOAT4X4 M;
		XMFLOAT4X4 V;
		XMFLOAT4X4 P;

		// Constant buffers are 256-byte aligned. Add padding in the struct to allow multiple buffers
		// to be array-indexed.
		float padding[12];
	};

	static const UINT BoxCount = 3;
	static const UINT BoxResourceCount = BoxCount * Window::BufferCount;
	static const UINT CommandSizePerFrame;                // The size of the indirect commands to draw all of the triangles in a single frame.
	static const UINT CommandBufferCounterOffset;         // The offset of the UAV counter in the processed command buffer.
	static const UINT ComputeThreadGroupSize = 128;       // Should match the value in compute.hlsl.

	std::vector<SceneConstantBuffer> ConstantBufferData;
	UINT8* CbvDataBegin;

	CSRootConstants CSRootConstants;    // Constants for the compute shader.

	ComPtr<ID3D12CommandSignature> CommandSignature;

	uint64_t FenceValues[Window::BufferCount] = {};

	ComPtr<ID3D12DescriptorHeap> DSVHeap;
	ComPtr<ID3D12DescriptorHeap> SRV2UAV1Heap;

	UINT RTVDescriptorSize;
	UINT SRV2UAV1DescriptorSize;

	ComPtr<ID3D12Resource> VertexBuffer;
	ComPtr<ID3D12Resource> ConstantBuffer;
	ComPtr<ID3D12Resource> IndexBuffer;
	ComPtr<ID3D12Resource> DepthBuffer;
	ComPtr<ID3D12Resource> CommandBuffer;
	ComPtr<ID3D12Resource> ProcessedCommandBuffers[Window::BufferCount];
	ComPtr<ID3D12Resource> ProcessedCommandBufferCounterReset;
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;

	ComPtr<ID3D12RootSignature> RootSignature;
	ComPtr<ID3D12RootSignature> ComputeRootSignature;
	ComPtr<ID3D12PipelineState> PipelineState;
	ComPtr<ID3D12PipelineState> ComputeState;

	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;

	float FoV;
	float drawOffset;
	float deltaTime;

	DirectX::XMMATRIX ViewMatrix;
	DirectX::XMMATRIX ProjectionMatrix;

	bool ContentLoaded;
	bool UseCompute;
};