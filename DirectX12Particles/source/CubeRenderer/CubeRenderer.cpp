#include "pch.h"
#include "CubeRenderer.h"

#include "Application.h"
#include "CommandQueue.h"
#include "Window.h"

using namespace DirectX;

struct VertexTexCoord
{
	XMFLOAT3 Position;
	XMFLOAT3 Color;
};

static VertexTexCoord Vertices[8] = {
	{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) },
	{ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
	{ XMFLOAT3( 1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f) },
	{ XMFLOAT3( 1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
	{ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
	{ XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 1.0f) },
	{ XMFLOAT3( 1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
	{ XMFLOAT3( 1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 1.0f) }
};

static WORD Indices[36] = {
	0, 1, 2, 0, 2, 3,
	4, 6, 5, 4, 7, 6,
	4, 5, 1, 4, 1, 0,
	3, 2, 6, 3, 6, 7,
	1, 5, 6, 1, 6, 2,
	4, 0, 3, 4, 3, 7
};

CubeRenderer::CubeRenderer(const std::wstring& name, int width, int height, bool vSync)
	: super(name, width, height, vSync)
	, ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	, Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)))
	, FoV(45.0)
	, ContentLoaded(false)
{
}

void CubeRenderer::UpdateBufferResource(ComPtr<ID3D12GraphicsCommandList2> commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource,
	size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags)
{
	auto device = Application::Get().GetDevice();

	size_t bufferSize = numElements * elementSize;

	CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);

	// Create a committed resource in GPU mem
	CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&vertexBufferDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(pDestinationResource)));

	// Create a committed resource for the upload
	if (bufferData)
	{
		CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		ThrowIfFailed(device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&vertexBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(pIntermediateResource)));

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = bufferData;
		subresourceData.RowPitch = bufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		UpdateSubresources(commandList.Get(), *pDestinationResource, *pIntermediateResource, 0, 0, 1, &subresourceData);
	}
}

bool CubeRenderer::LoadContent()
{
	auto device = Application::Get().GetDevice();
	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	auto commandList = commandQueue->GetCommandList();

	ComPtr<ID3D12Resource> intermediateVertexBuffer;
	UpdateBufferResource(commandList.Get(), &VertexBuffer, &intermediateVertexBuffer, _countof(Vertices), sizeof(VertexTexCoord), Vertices);

	VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
	VertexBufferView.SizeInBytes = sizeof(Vertices);
	VertexBufferView.StrideInBytes = sizeof(VertexTexCoord);

	ComPtr<ID3D12Resource> intermediateIndexBuffer;
	UpdateBufferResource(commandList.Get(), &IndexBuffer, &intermediateIndexBuffer, _countof(Indices), sizeof(WORD), Indices);

	IndexBufferView.BufferLocation = IndexBuffer->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
	IndexBufferView.SizeInBytes = sizeof(Indices);

	DSVHeap = Application::Get().CreateDescriptorHeap(1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	ComPtr<ID3DBlob> vertexShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"v_Passthrough.cso", &vertexShaderBlob));

	ComPtr<ID3DBlob> fragmentShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"f_PositionColor.cso", &fragmentShaderBlob));

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // Omit flag if input assembler not required
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | 
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | 
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS | 
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
	
	CD3DX12_ROOT_PARAMETER1 rootParameters[1];
	rootParameters[0].InitAsConstants(3 * sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

	// Serialize the root signature into binary
	ComPtr<ID3DBlob> rootSignatureBlob;
	ComPtr<ID3DBlob> errorBlob;

	// Rewrites the root sig in version 1.0 even if passed in as 1.1 (if needed)
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription, featureData.HighestVersion, &rootSignatureBlob, &errorBlob));

	// Create the root signature YIPPEE
	ThrowIfFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&RootSignature)));

	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
	} pipelineStateStream;

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	pipelineStateStream.pRootSignature = RootSignature.Get();
	pipelineStateStream.InputLayout = { inputLayout, _countof(inputLayout)};
	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(fragmentShaderBlob.Get());
	pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pipelineStateStream.RTVFormats = rtvFormats;

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(PipelineStateStream), &pipelineStateStream
	};
	ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&PipelineState)));

	auto fenceValue = commandQueue->ExecuteCommandList(commandList);
	commandQueue->WaitForFenceValue(fenceValue);

	ContentLoaded = true;

	ResizeDepthBuffer(GetWindowWidth(), GetWindowHeight());

	return true;
}

void CubeRenderer::UnloadContent()
{
	ContentLoaded = false;
}

void CubeRenderer::ResizeDepthBuffer(int width, int height)
{
	if (ContentLoaded)
	{
		Application::Get().Flush();

		width = max(1, width);
		height = max(1, height);

		auto device = Application::Get().GetDevice();

		D3D12_CLEAR_VALUE optimizedClearValue = {};
		optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		optimizedClearValue.DepthStencil = { 1.0f, 0 };

		CD3DX12_RESOURCE_DESC dsDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		// Create a committed resource in GPU mem
		CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

		ThrowIfFailed(device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&dsDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optimizedClearValue,
			IID_PPV_ARGS(&DepthBuffer)
		));

		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = DXGI_FORMAT_D32_FLOAT;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = D3D12_DSV_FLAG_NONE;

		device->CreateDepthStencilView(DepthBuffer.Get(), &dsv, DSVHeap->GetCPUDescriptorHandleForHeapStart());
	}
}

void CubeRenderer::OnResize(ResizeEventArgs& e)
{
	if (e.Width != GetWindowWidth() || e.Height != GetWindowHeight())
	{
		super::OnResize(e);

		Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(e.Width), static_cast<float>(e.Height));

		ResizeDepthBuffer(e.Width, e.Height);
	}
}

void CubeRenderer::OnUpdate(UpdateEventArgs& e)
{
	static uint64_t frameCount = 0;
	static double totalTime = 0.0;

	super::OnUpdate(e);

	totalTime += e.ElapsedTime;
	++frameCount;

	if (totalTime > 1.0)
	{
		double fps = frameCount / totalTime;

		char buffer[512];
		sprintf_s(buffer, "FPS: %f\n", fps);
		OutputDebugStringA(buffer);

		frameCount = 0;
		totalTime = 0.0;
	}

	// Update model matrix
	float angle = static_cast<float>(e.TotalTime * 90.0);
	const XMVECTOR rotationAxis = XMVectorSet(0, 1, 1, 0);
	ModelMatrix = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(angle));

	// Update view matrix
	const XMVECTOR eyePosition = XMVectorSet(0, 0, -10, 1);
	const XMVECTOR focusPoint = XMVectorSet(0, 0, 0, 1);
	const XMVECTOR upDirection = XMVectorSet(0, 1, 0, 1);
	ViewMatrix = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

	// Update projection matrix
	float aspectRatio = GetWindowWidth() / static_cast<float>(GetWindowHeight());
	ProjectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(FoV), aspectRatio, 0.1f, 100.0f);
}

void CubeRenderer::TransitionResource(ComPtr<ID3D12GraphicsCommandList2> commandList, ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), beforeState, afterState);

	commandList->ResourceBarrier(1, &barrier);
}

void CubeRenderer::OnRender(RenderEventArgs& e)
{
	super::OnRender(e);

	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetCommandList(); // in a state to be written to, doesn't need to be reset

	UINT currentBackBufferIndex = pWindow->GetCurrentBackBufferIndex();
	auto backBuffer = pWindow->GetCurrentBackBuffer();
	auto rtv = pWindow->GetCurrentRenderTargetView();
	auto dsv = DSVHeap->GetCPUDescriptorHandleForHeapStart();

	// Clear render targets
	{
		TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
		commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

		FLOAT clearDepth = 1.0f;
		commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, clearDepth, 0, 0, nullptr);
	}

	// Set up PSO and root signature
	commandList->SetPipelineState(PipelineState.Get()); // Binds PSO to render pipeline
	commandList->SetGraphicsRootSignature(RootSignature.Get()); // Must do even though set in PSO

	// Set up input assembler
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &VertexBufferView);
	commandList->IASetIndexBuffer(&IndexBufferView);

	// Set up rasterizer state
	commandList->RSSetViewports(1, &Viewport);
	commandList->RSSetScissorRects(1, &ScissorRect);

	// Bind render targets
	commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

	// Update root parameters
	XMMATRIX mvpMatrix[3] = { ModelMatrix, ViewMatrix, ProjectionMatrix };
	XMMATRIX mvpMatrix2[3] = { XMMatrixTranslation(0, 3, 0), ViewMatrix, ProjectionMatrix};
	commandList->SetGraphicsRoot32BitConstants(0, 3 * sizeof(XMMATRIX) / 4, &mvpMatrix, 0);

	// Draw
	commandList->DrawIndexedInstanced(_countof(Indices), 2, 0, 0, 0);

	commandList->SetGraphicsRoot32BitConstants(0, 3 * sizeof(XMMATRIX) / 4, &mvpMatrix2, 0);
	commandList->DrawIndexedInstanced(_countof(Indices), 2, 0, 0, 0);

	// Present
	{
		TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		FenceValues[currentBackBufferIndex] = commandQueue->ExecuteCommandList(commandList);

		currentBackBufferIndex = pWindow->Present();

		commandQueue->WaitForFenceValue(FenceValues[currentBackBufferIndex]);
	}
}

void CubeRenderer::OnKeyPressed(KeyEventArgs& e)
{
	super::OnKeyPressed(e);

	switch (e.Key)
	{
	case KeyCode::Escape:
		Application::Get().Quit(0);
		break;
	case KeyCode::Enter:
		if (e.Alt)
		{
	case KeyCode::F11:
		pWindow->ToggleFullScreen();
		break;
		}
	case KeyCode::V:
		pWindow->ToggleVSync();
		break;
	}
}

void CubeRenderer::OnMouseWheel(MouseWheelEventArgs& e)
{
	FoV -= e.WheelDelta;
	FoV = std::clamp(FoV, 12.0f, 90.0f);

	char buffer[256];
	sprintf_s(buffer, "FoV: %f\n", FoV);
	OutputDebugStringA(buffer);
}