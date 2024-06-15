#include "pch.h"
#include "CubeRenderer.h"

#include "Application.h"
#include "CommandQueue.h"
#include "Window.h"

using namespace DirectX;

struct VertexPosColor
{
	XMFLOAT3 Position;
	XMFLOAT3 Color;
};

static VertexPosColor Vertices[8] = {
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
	UpdateBufferResource(commandList.Get(), &VertexBuffer, &intermediateVertexBuffer, _countof(Vertices), sizeof(VertexPosColor), Vertices);

	VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
	VertexBufferView.SizeInBytes = sizeof(Vertices);
	VertexBufferView.StrideInBytes = sizeof(VertexPosColor);

	ComPtr<ID3D12Resource> intermediateIndexBuffer;
	UpdateBufferResource(commandList.Get(), &IndexBuffer, &intermediateIndexBuffer, _countof(Indices), sizeof(WORD), Indices);

	IndexBufferView.BufferLocation = IndexBuffer->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
	IndexBufferView.SizeInBytes = sizeof(Indices);

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&DSVHeap)));

	ComPtr<ID3DBlob> vertexShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"v_Passthrough.cso", &vertexShaderBlob));

	ComPtr<ID3DBlob> fragmentShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"f_PositionColor.cso", &fragmentShaderBlob));

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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
	rootParameters[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

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

}

void CubeRenderer::ResizeDepthBuffer(int width, int height)
{

}

void CubeRenderer::OnUpdate(UpdateEventArgs& e)
{

}

void CubeRenderer::OnRender(RenderEventArgs& e)
{

}

void CubeRenderer::OnKeyPressed(KeyEventArgs& e)
{

}

void CubeRenderer::OnMouseWheel(MouseWheelEventArgs& e)
{

}

void CubeRenderer::OnResize(ResizeEventArgs& e)
{

}
