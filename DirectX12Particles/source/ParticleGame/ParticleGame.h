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
		float size;

		float padding[50];
	};

	struct PlaneData
	{
		XMFLOAT4 position;
		XMFLOAT4 axisOfRotation;
		XMFLOAT4 scale;
		XMFLOAT4 color;
		XMFLOAT4 UVMinMax; // uvStartx, uvEndx, uvStarty, uvEndy
		float colorOverride; // lerps between texture and color supplied above
		float rotation;

		float padding[42];
	};

	struct VSRootConstants
	{
		XMMATRIX V;
		XMMATRIX P;
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
		float particleStartScale;
		float particleEndScale;
	};

	struct PPRootConstants
	{
		int windowWidth;
		int windowHeight;
		XMMATRIX invVP;
	};

	static const UINT ComputeThreadGroupSize = 128;

	VSRootConstants VSRootConstants;
	CSRootConstants CSRootConstants;
	PPRootConstants PPRootConstants;

	uint64_t FenceValues[Window::BufferCount] = {};

	ComPtr<ID3D12DescriptorHeap> RTVHeap; // Used for post-processing, RTVHeap for rendering exists in Window class
	ComPtr<ID3D12DescriptorHeap> DSVHeap;
	ComPtr<ID3D12DescriptorHeap> DescriptorHeap;
	UINT DescriptorSize;
	UINT DescriptorSizeRTV;
	UINT DescriptorSizeDSV;

	ComPtr<ID3D12Resource> VertexBuffer;
	ComPtr<ID3D12Resource> IndexBuffer;
	ComPtr<ID3D12Resource> DepthBuffer;
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;

	ComPtr<ID3D12RootSignature> RenderRS;
	ComPtr<ID3D12RootSignature> AABBRS;
	ComPtr<ID3D12RootSignature> EmitRS;
	ComPtr<ID3D12RootSignature> SimulateRS;
	ComPtr<ID3D12RootSignature> PostProcessRS;

	ComPtr<ID3D12PipelineState> ParticleRenderPSO;
	ComPtr<ID3D12PipelineState> AABBPSO;
	ComPtr<ID3D12PipelineState> PlaneRenderPSO;
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
	bool UsePostProcess;

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
	ComPtr<ID3D12Resource> WallTexture;
	ComPtr<ID3D12Resource> RenderTexture;
	ComPtr<ID3D12Resource> PlaneBuffer;

	PlaneData Planes[14] = {
		// Bordering walls
		{ XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f), XMFLOAT4(1, 0, 0, 0), XMFLOAT4(5.1f, 10.1f, 1.0f, 1.0f), XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f),
			XMFLOAT4(0.0f, 1.0f, 0.0f, 2.0f), 1.8f, XMConvertToRadians(90) }, // Bottom
		{ XMFLOAT4(-5.0f, 5.0f, 0.0f, 1.0f), XMFLOAT4(0, 1, 0, 0), XMFLOAT4(10.1f, 5.1f, 1.0f, 1.0f), XMFLOAT4(0.6f, 0.7f, 0.6f, 1.0f),
			XMFLOAT4(0.0f, 2.0f, 0.0f, 1.0f), 1.0f, XMConvertToRadians(270) }, // Left
		{ XMFLOAT4(0.0f, 10.0f, 0.0f, 1.0f), XMFLOAT4(1, 0, 0, 0), XMFLOAT4(5.1f, 10.1f, 1.0f, 1.0f), XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f),
			XMFLOAT4(0.0f, 1.0f, 0.0f, 2.0f), 1.0f, XMConvertToRadians(270) }, // Top
		{ XMFLOAT4(0.0f, 5.0f, 10.0f, 1.0f), XMFLOAT4(0, 0, 0, 0), XMFLOAT4(5.1f, 5.1f, 1.0f, 1.0f), XMFLOAT4(0.6f, 0.7f, 0.6f, 1.0f),
			XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), 1.0f, XMConvertToRadians(0) }, // Back
		{ XMFLOAT4(5.0f, 2.0f, 0.0f, 1.0f), XMFLOAT4(0, 1, 0, 0), XMFLOAT4(10.0f, 2.0f, 1.0f, 1.0f), XMFLOAT4(0.6f, 0.7f, 0.6f, 1.0f),
			XMFLOAT4(0.0f, 2.0f, 0.0f, 0.4f), 1.0f, XMConvertToRadians(90) }, // Right 0
		{ XMFLOAT4(5.0f, 8.5f, 0.0f, 1.0f), XMFLOAT4(0, 1, 0, 0), XMFLOAT4(10.0f, 1.5f, 1.0f, 1.0f), XMFLOAT4(0.6f, 0.7f, 0.6f, 1.0f),
			XMFLOAT4(0.0f, 2.0f, 0.7f, 1.0f), 1.0f, XMConvertToRadians(90) }, // Right 1
		{ XMFLOAT4(5.0f, 5.5f, 8.0f, 1.0f), XMFLOAT4(0, 1, 0, 0), XMFLOAT4(2.0f, 1.5f, 1.0f, 1.0f), XMFLOAT4(0.6f, 0.7f, 0.6f, 1.0f),
			XMFLOAT4(0.0f, 0.4f, 0.4f, 0.7f), 1.0f, XMConvertToRadians(90) }, // Right 2
		{ XMFLOAT4(5.0f, 5.5f, -4.0f, 1.0f), XMFLOAT4(0, 1, 0, 0), XMFLOAT4(6.0f, 1.5f, 1.0f, 1.0f), XMFLOAT4(0.6f, 0.7f, 0.6f, 1.0f),
			XMFLOAT4(0.8f, 2.0f, 0.4f, 0.7f), 1.0f, XMConvertToRadians(90) }, // Right 3

		// Baseboards
		{ XMFLOAT4(-4.9f, 0.5f, 0.0f, 1.0f), XMFLOAT4(1, 0, 0, 0), XMFLOAT4(0.1f, 10.1f, 1.0f, 1.0f), XMFLOAT4(0.82f, 0.71f, 0.62f, 1.0f),
			XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), 1.0f, XMConvertToRadians(90) }, // Left-top
		{ XMFLOAT4(-4.8f, 0.25f, 0.0f, 1.0f), XMFLOAT4(0, 1, 0, 0), XMFLOAT4(10.1f, 0.25f, 1.0f, 1.0f), XMFLOAT4(0.82f, 0.71f, 0.62f, 1.0f),
			XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), 1.0f, XMConvertToRadians(270) }, // Left-side
		{ XMFLOAT4(4.9f, 0.5f, 0.0f, 1.0f), XMFLOAT4(1, 0, 0, 0), XMFLOAT4(0.1f, 10.1f, 1.0f, 1.0f), XMFLOAT4(0.82f, 0.71f, 0.62f, 1.0f),
			XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), 1.0f, XMConvertToRadians(90) }, // Right-top
		{ XMFLOAT4(4.8f, 0.25f, 0.0f, 1.0f), XMFLOAT4(0, 1, 0, 0), XMFLOAT4(10.1f, 0.25f, 1.0f, 1.0f), XMFLOAT4(0.82f, 0.71f, 0.62f, 1.0f),
			XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), 1.0f, XMConvertToRadians(90) }, // Right-side
		{ XMFLOAT4(0.0f, 0.5f, 9.9f, 1.0f), XMFLOAT4(1, 0, 0, 0), XMFLOAT4(4.8f, 0.1f, 1.0f, 1.0f), XMFLOAT4(0.82f, 0.71f, 0.62f, 1.0f),
			XMFLOAT4(0.0f, 1.0f, 0.0f, 2.0f), 1.0f, XMConvertToRadians(90) }, // Back-top
		{ XMFLOAT4(0.0f, 0.25f, 9.8f, 1.0f), XMFLOAT4(0, 0, 0, 0), XMFLOAT4(4.8f, 0.25f, 1.0f, 1.0f), XMFLOAT4(0.82f, 0.71f, 0.62f, 1.0f),
			XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), 1.0f, XMConvertToRadians(0) }, // Back-side
	};
};