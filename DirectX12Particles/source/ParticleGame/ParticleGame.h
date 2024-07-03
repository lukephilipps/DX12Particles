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
	virtual void OnKeyReleased(KeyEventArgs& e) override;
	virtual void OnMouseWheel(MouseWheelEventArgs& e) override;
	virtual void OnResize(ResizeEventArgs& e) override;

private:

	// Transition a resource's state
	void TransitionResource(ComPtr<ID3D12GraphicsCommandList2> commandList, ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState);

	// Create a GPU buffer
	void UpdateBufferResource(ComPtr<ID3D12GraphicsCommandList2> commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource, 
		size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	void UpdateBufferResourceWithCounter(ComPtr<ID3D12GraphicsCommandList2> commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource,
		size_t bufferSize, const void* bufferData);
	void UpdateTextureResourceFromFile(ComPtr<ID3D12GraphicsCommandList2> commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource, std::wstring fileName);

	// Resize the depth buffer to match the window area
	void ResizeDepthBuffer(int width, int height);

	struct Particle
	{
		XMFLOAT4 position;
		XMFLOAT4 velocity;
		XMFLOAT4 acceleration;
		float lifeTimeLeft;

		float padding[51];
	};

	struct VSRootConstants
	{
		XMMATRIX V;
		XMMATRIX P;
		float angle;
	};

	struct CSRootConstants
	{
		float deltaTime;
		float particleLifetime;
		UINT emitCount;
		UINT maxParticleCount;
		XMFLOAT4 emitAABBMin;
		XMFLOAT4 emitAABBMax;
		XMFLOAT4 emitVelocityMin;
		XMFLOAT4 emitVelocityMax;
		XMFLOAT4 emitAccelerationMin;
		XMFLOAT4 emitAccelerationMax;
	};

	static const UINT ComputeThreadGroupSize = 128;

	VSRootConstants VSRootConstants;
	CSRootConstants CSRootConstants;

	uint64_t FenceValues[Window::BufferCount] = {};

	ComPtr<ID3D12DescriptorHeap> RTVHeap; // Used for post-processing, RTVHeap for rendering exists in Window class
	ComPtr<ID3D12DescriptorHeap> DSVHeap;
	ComPtr<ID3D12DescriptorHeap> DescriptorHeap;
	UINT DescriptorSize;
	UINT DescriptorSizeRTV;

	ComPtr<ID3D12Resource> VertexBuffer;
	ComPtr<ID3D12Resource> IndexBuffer;
	ComPtr<ID3D12Resource> DepthBuffer;
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;

	ComPtr<ID3D12RootSignature> RenderRS;
	ComPtr<ID3D12RootSignature> EmitRS;
	ComPtr<ID3D12RootSignature> SimulateRS;
	ComPtr<ID3D12RootSignature> PostProcessRS;

	ComPtr<ID3D12PipelineState> RenderPSO;
	ComPtr<ID3D12PipelineState> EmitPSO;
	ComPtr<ID3D12PipelineState> SimulatePSO;
	ComPtr<ID3D12PipelineState> PostProcessPSO;

	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;

	float FoV;
	float drawOffset;
	float deltaTime;

	bool ContentLoaded;
	bool UseCompute;

	static const UINT MaxParticleCount = 1000;
	static const UINT ParticleResourceCount = MaxParticleCount * Window::BufferCount;
	ComPtr<ID3D12Resource> ParticleBuffer;
	ComPtr<ID3D12Resource> AliveIndexList0;
	ComPtr<ID3D12Resource> AliveIndexList1;
	ComPtr<ID3D12Resource> DeadIndexList;
	ComPtr<ID3D12Resource> DeadIndexListCounter;
	ComPtr<ID3D12Resource> StagedParticleBuffers;
	ComPtr<ID3D12Resource> UAVCounterReset;
	static const UINT ParticleBufferCounterOffset;

	// Camera vars
	bool PressingW;
	bool PressingA;
	bool PressingS;
	bool PressingD;
	bool PressingQ;
	bool PressingE;

	XMFLOAT4 CameraPosition;
	const float CameraMoveSpeed = 8;

	ComPtr<ID3D12Resource> TilesTexture;
	ComPtr<ID3D12Resource> RenderTexture;
};