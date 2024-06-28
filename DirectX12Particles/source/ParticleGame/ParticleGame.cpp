#include "pch.h"
#include "ParticleGame.h"

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

// Triangles
static WORD Indices[36] = {
	0, 1, 2, 0, 2, 3,
	4, 6, 5, 4, 7, 6,
	4, 5, 1, 4, 1, 0,
	3, 2, 6, 3, 6, 7,
	1, 5, 6, 1, 6, 2,
	4, 0, 3, 4, 3, 7
};

const UINT ParticleGame::CommandSizePerFrame = BoxCount * sizeof(IndirectCommand);
const UINT ParticleGame::CommandBufferCounterOffset = (CommandSizePerFrame + (D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT - 1);

ParticleGame::ParticleGame(const std::wstring& name, int width, int height, bool vSync)
	: super(name, width, height, vSync)
	, ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	, Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)))
	, FoV(45.0)
	, ContentLoaded(false)
	, drawOffset(0)
	, UseCompute(true)
	, deltaTime(0)
{
	CSRootConstants.emitCount = 1;
	CSRootConstants.particleLifetime = 1;
	CSRootConstants.emitPosition = XMFLOAT3(0, 0, 0);
	CSRootConstants.emitVelocity = XMFLOAT3(0, 1, 0);
	ConstantBufferData.resize(BoxCount);
}

void ParticleGame::UpdateBufferResource(ComPtr<ID3D12GraphicsCommandList2> commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource,
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
		D3D12_RESOURCE_STATE_COPY_DEST,
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

bool ParticleGame::LoadContent()
{
	auto device = Application::Get().GetDevice();

	// Create descriptor heaps
	{
		DSVHeap = Application::Get().CreateDescriptorHeap(1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		// 3 comes from the heap containing 3 subheaps (and * for each frame)
		SRV2UAV1Heap = Application::Get().CreateDescriptorHeap(3 * Window::BufferCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
	}

	// Create root signatures
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		ComPtr<ID3DBlob> RSBlob;
		ComPtr<ID3DBlob> errorBlob;

		// Create render root signature
		CD3DX12_ROOT_PARAMETER1 rootParameters[1];
		rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_VERTEX);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC RSDescription;
		RSDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&RSDescription, featureData.HighestVersion, &RSBlob, &errorBlob));
		ThrowIfFailed(device->CreateRootSignature(0, RSBlob->GetBufferPointer(), RSBlob->GetBufferSize(), IID_PPV_ARGS(&RenderRS)));

		// Create emitter compute signature
		CD3DX12_DESCRIPTOR_RANGE1 emitRanges[2];
		emitRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		emitRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

		CD3DX12_ROOT_PARAMETER1 emitRootParameters[2];
		emitRootParameters[0].InitAsDescriptorTable(2, emitRanges);
		emitRootParameters[1].InitAsConstants(9, 0);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC emitRSDescription;
		emitRSDescription.Init_1_1(_countof(emitRootParameters), emitRootParameters);

		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&emitRSDescription, featureData.HighestVersion, &RSBlob, &errorBlob));
		ThrowIfFailed(device->CreateRootSignature(0, RSBlob->GetBufferPointer(), RSBlob->GetBufferSize(), IID_PPV_ARGS(&EmitRS)));

		// Create simulation compute signature
		CD3DX12_DESCRIPTOR_RANGE1 simulateRanges[2];
		simulateRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		simulateRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

		CD3DX12_ROOT_PARAMETER1 simulateRootParameters[2];
		simulateRootParameters[0].InitAsDescriptorTable(2, simulateRanges);
		simulateRootParameters[1].InitAsConstants(9, 0);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC simulateRSDescription;
		simulateRSDescription.Init_1_1(_countof(simulateRootParameters), simulateRootParameters);

		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&simulateRSDescription, featureData.HighestVersion, &RSBlob, &errorBlob));
		ThrowIfFailed(device->CreateRootSignature(0, RSBlob->GetBufferPointer(), RSBlob->GetBufferSize(), IID_PPV_ARGS(&SimulateRS)));
	}

	// Create PSO's
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;
		ComPtr<ID3DBlob> computeEmitShader;
		ComPtr<ID3DBlob> computeSimulateShader;
		ComPtr<ID3DBlob> error;

		// Get path to .exe directory (where .cso files are)
		WCHAR assetsPath[512];
		GetModuleFileNameW(nullptr, assetsPath, _countof(assetsPath));
		std::wstring assetPathString = assetsPath;
		assetPathString = assetPathString.substr(0, assetPathString.find_last_of(L"\\") + 1);

		ThrowIfFailed(D3DReadFileToBlob((assetPathString + L"Vertex.cso").c_str(), &vertexShader));
		ThrowIfFailed(D3DReadFileToBlob((assetPathString + L"Pixel.cso").c_str(), &pixelShader));
		ThrowIfFailed(D3DReadFileToBlob((assetPathString + L"Compute.cso").c_str(), &computeEmitShader));
		ThrowIfFailed(D3DReadFileToBlob((assetPathString + L"ComputeSimulator.cso").c_str(), &computeSimulateShader));

		// Define vertex input layout
		D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Define rendering PSO
		struct RenderPipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		} renderPSS;

		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 1;
		rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

		renderPSS.pRootSignature = RenderRS.Get();
		renderPSS.InputLayout = { inputLayout, _countof(inputLayout) };
		renderPSS.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		renderPSS.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		renderPSS.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
		renderPSS.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		renderPSS.RTVFormats = rtvFormats;

		D3D12_PIPELINE_STATE_STREAM_DESC psoDesc =
		{
			sizeof(RenderPipelineStateStream), &renderPSS
		};
		ThrowIfFailed(device->CreatePipelineState(&psoDesc, IID_PPV_ARGS(&RenderPSO)));

		// Define emit PSO
		struct EmitPipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_CS CS;
		} emitPSS;

		emitPSS.pRootSignature = EmitRS.Get();
		emitPSS.CS = CD3DX12_SHADER_BYTECODE(computeEmitShader.Get());;

		D3D12_PIPELINE_STATE_STREAM_DESC emitPSODesc =
		{
			sizeof(EmitPipelineStateStream), &emitPSS
		};
		ThrowIfFailed(device->CreatePipelineState(&emitPSODesc, IID_PPV_ARGS(&EmitPSO)));

		// Define simulate PSO
		struct SimulatePipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_CS CS;
		} simulatePSS;

		simulatePSS.pRootSignature = SimulateRS.Get();
		simulatePSS.CS = CD3DX12_SHADER_BYTECODE(computeSimulateShader.Get());;

		D3D12_PIPELINE_STATE_STREAM_DESC simulatePSODesc =
		{
			sizeof(SimulatePipelineStateStream), &simulatePSS
		};
		ThrowIfFailed(device->CreatePipelineState(&simulatePSODesc, IID_PPV_ARGS(&SimulatePSO)));
	}

	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetCommandList();
	ComPtr<ID3D12Resource> intermediateVertexBuffer;
	ComPtr<ID3D12Resource> intermediateIndexBuffer;
	ComPtr<ID3D12Resource> intermediateCommandBuffer;

	// Create Vertex/Index Buffer
	{
		UpdateBufferResource(commandList.Get(), &VertexBuffer, &intermediateVertexBuffer, _countof(Vertices), sizeof(VertexPosColor), Vertices);
		TransitionResource(commandList, VertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

		VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
		VertexBufferView.SizeInBytes = sizeof(Vertices);
		VertexBufferView.StrideInBytes = sizeof(VertexPosColor);

		UpdateBufferResource(commandList.Get(), &IndexBuffer, &intermediateIndexBuffer, _countof(Indices), sizeof(WORD), Indices);
		TransitionResource(commandList, IndexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

		IndexBufferView.BufferLocation = IndexBuffer->GetGPUVirtualAddress();
		IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
		IndexBufferView.SizeInBytes = sizeof(Indices);
	}

	RTVDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	SRV2UAV1DescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Create constant buffers
	{
		const UINT constantBufferDataSize = BoxResourceCount * sizeof(SceneConstantBuffer);

		CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferDataSize);
		ThrowIfFailed(device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&constantBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&ConstantBuffer)));

		const XMVECTOR eyePosition = XMVectorSet(0, 0, -10, 1);
		const XMVECTOR focusPoint = XMVectorSet(0, 0, 0, 1);
		const XMVECTOR upDirection = XMVectorSet(0, 1, 0, 1);
		const float aspectRatio = GetWindowWidth() / static_cast<float>(GetWindowHeight());

		// Init constant buffer for each box
		for (UINT n = 0; n < BoxCount; ++n)
		{
			ConstantBufferData[n].rotation = XMFLOAT4(0, 0, 0, 0);
			XMStoreFloat4x4(&ConstantBufferData[n].M, XMMatrixTranslation(0, 0, 0));
			XMStoreFloat4x4(&ConstantBufferData[n].V, XMMatrixLookAtLH(eyePosition, focusPoint, upDirection));
			XMStoreFloat4x4(&ConstantBufferData[n].P, XMMatrixPerspectiveFovLH(XMConvertToRadians(FoV), aspectRatio, 0.1f, 100.0f));
		}

		// Map and init the constant buffer
		CD3DX12_RANGE readRange(0, 0); // No CPU reading, read from no range
		ThrowIfFailed(ConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&CbvDataBegin)));
		memcpy(CbvDataBegin, &ConstantBufferData[0], BoxCount * sizeof(SceneConstantBuffer));

		// Create SRV for constant buffer of compute shader to read from (SRV2UAV1Buffer index 0)
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.NumElements = BoxCount;
		srvDesc.Buffer.StructureByteStride = sizeof(SceneConstantBuffer);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvHandle(SRV2UAV1Heap->GetCPUDescriptorHandleForHeapStart(), 0, SRV2UAV1DescriptorSize);
		for (UINT frame = 0; frame < Window::BufferCount; frame++)
		{
			srvDesc.Buffer.FirstElement = frame * BoxCount;
			device->CreateShaderResourceView(ConstantBuffer.Get(), &srvDesc, cbvSrvHandle);
			cbvSrvHandle.Offset(3, SRV2UAV1DescriptorSize);
		}
	}

	// Create command signature for indirect drawing
	{
		// Each call contains a CBV update and a DrawInstanced call
		D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[3] = {};
		argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
		argumentDescs[0].ConstantBufferView.RootParameterIndex = 0;
		argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
		argumentDescs[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
		commandSignatureDesc.pArgumentDescs = argumentDescs;
		commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
		commandSignatureDesc.ByteStride = sizeof(IndirectCommand);

		ThrowIfFailed(device->CreateCommandSignature(&commandSignatureDesc, RenderRS.Get(), IID_PPV_ARGS(&CommandSignature)));
	}

	// Create command buffers/UAVs to store results of compute work
	{
		std::vector<IndirectCommand> commands;
		commands.resize(BoxResourceCount);
		const UINT commandBufferSize = CommandSizePerFrame * Window::BufferCount;

		CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC commandBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(commandBufferSize);
		ThrowIfFailed(device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&commandBufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&CommandBuffer)));

		CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC uploadCommandBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(commandBufferSize);
		ThrowIfFailed(device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&uploadCommandBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&intermediateCommandBuffer)));

		D3D12_GPU_VIRTUAL_ADDRESS gpuAdress = ConstantBuffer->GetGPUVirtualAddress();
		UINT commandIndex = 0;

		for (UINT frame = 0; frame < Window::BufferCount; frame++)
		{
			for (UINT n = 0; n < BoxCount; ++n)
			{
				commands[commandIndex].cbv = gpuAdress;
				commands[commandIndex].ibv = IndexBufferView;
				commands[commandIndex].drawArguments.IndexCountPerInstance = _countof(Indices);
				commands[commandIndex].drawArguments.InstanceCount = 1;
				commands[commandIndex].drawArguments.StartIndexLocation = 0;
				commands[commandIndex].drawArguments.BaseVertexLocation = 0;
				commands[commandIndex].drawArguments.StartInstanceLocation = 0;

				commandIndex++;
				gpuAdress += sizeof(SceneConstantBuffer);
			}
		}

		// Copy data to intermediate (upload) heap then to command buffer
		D3D12_SUBRESOURCE_DATA commandData = {};
		commandData.pData = reinterpret_cast<UINT8*>(&commands[0]);
		commandData.RowPitch = commandBufferSize;
		commandData.SlicePitch = commandData.RowPitch;

		UpdateSubresources<1>(commandList.Get(), CommandBuffer.Get(), intermediateCommandBuffer.Get(), 0, 0, 1, &commandData);
		TransitionResource(commandList, CommandBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		// Create SRV 2, input command buffer (SRV2UAV1Buffer index 1)
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.NumElements = BoxCount;
		srvDesc.Buffer.StructureByteStride = sizeof(IndirectCommand);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		CD3DX12_CPU_DESCRIPTOR_HANDLE commandsHandle(SRV2UAV1Heap->GetCPUDescriptorHandleForHeapStart(), 1, SRV2UAV1DescriptorSize);
		for (UINT frame = 0; frame < Window::BufferCount; frame++)
		{
			srvDesc.Buffer.FirstElement = frame * BoxCount;
			device->CreateShaderResourceView(CommandBuffer.Get(), &srvDesc, commandsHandle);
			commandsHandle.Offset(3, SRV2UAV1DescriptorSize);
		}

		// Create UAV's to store results of compute work (SRV2UAV1Buffer index 2)
		CD3DX12_CPU_DESCRIPTOR_HANDLE processedCommandsHandle(SRV2UAV1Heap->GetCPUDescriptorHandleForHeapStart(), 2, SRV2UAV1DescriptorSize);
		for (UINT frame = 0; frame < Window::BufferCount; frame++)
		{
			// Allocate a buffer large enough to hold all indirect commands for a frame plus 1 UAV counter
			commandBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(CommandBufferCounterOffset + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			ThrowIfFailed(device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&commandBufferDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&ProcessedCommandBuffers[frame])));

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = DXGI_FORMAT_UNKNOWN;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.FirstElement = 0;
			uavDesc.Buffer.NumElements = BoxCount;
			uavDesc.Buffer.StructureByteStride = sizeof(IndirectCommand);
			uavDesc.Buffer.CounterOffsetInBytes = CommandBufferCounterOffset;
			uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

			device->CreateUnorderedAccessView(ProcessedCommandBuffers[frame].Get(), ProcessedCommandBuffers[frame].Get(), &uavDesc, processedCommandsHandle);

			processedCommandsHandle.Offset(3, SRV2UAV1DescriptorSize);
		}

		// Allocate buffer to reset UAV counters and initialize it to 0
		D3D12_RESOURCE_DESC resetCommandBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT));
		ThrowIfFailed(device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resetCommandBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&ProcessedCommandBufferCounterReset)));

		UINT8* pMappedCounterReset = nullptr;
		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(ProcessedCommandBufferCounterReset->Map(0, &readRange, reinterpret_cast<void**>(&pMappedCounterReset)));
		ZeroMemory(pMappedCounterReset, sizeof(UINT));
		ProcessedCommandBufferCounterReset->Unmap(0, nullptr);
	}

	auto fenceValue = commandQueue->ExecuteCommandList(commandList);
	commandQueue->WaitForFenceValue(fenceValue);

	ContentLoaded = true;

	ResizeDepthBuffer(GetWindowWidth(), GetWindowHeight());

	return true;
}

void ParticleGame::UnloadContent()
{
	ContentLoaded = false;
}

void ParticleGame::ResizeDepthBuffer(int width, int height)
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

void ParticleGame::OnResize(ResizeEventArgs& e)
{
	if (e.Width != GetWindowWidth() || e.Height != GetWindowHeight())
	{
		super::OnResize(e);

		Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(e.Width), static_cast<float>(e.Height));

		ResizeDepthBuffer(e.Width, e.Height);
	}
}

void ParticleGame::OnUpdate(UpdateEventArgs& e)
{
	static uint64_t frameCount = 0;
	static double totalTime = 0.0;

	super::OnUpdate(e);

	totalTime += e.ElapsedTime;
	++frameCount;

	deltaTime = (float)e.ElapsedTime;

	if (totalTime > 1.0)
	{
		double fps = frameCount / totalTime;

		char buffer[512];
		sprintf_s(buffer, "FPS: %f\n", fps);
		OutputDebugStringA(buffer);

		frameCount = 0;
		totalTime = 0.0;
	}
	
	// Update constant info for next frame's buffer
	{
		// Get model matrix
		XMMATRIX identityMatrix = XMMatrixIdentity();

		// Update view matrix
		const XMVECTOR eyePosition = XMVectorSet(0, 0, -10, 1);
		const XMVECTOR focusPoint = XMVectorSet(0, 0, 0, 1);
		const XMVECTOR upDirection = XMVectorSet(0, 1, 0, 1);
		ViewMatrix = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

		// Update projection matrix
		float aspectRatio = GetWindowWidth() / static_cast<float>(GetWindowHeight());
		ProjectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(FoV), aspectRatio, 0.1f, 100.0f);

		float angle = static_cast<float>(e.TotalTime * 90);

		for (UINT n = 0; n < BoxCount; n++)
		{
			ConstantBufferData[n].rotation = XMFLOAT4(XMConvertToRadians(angle), (float)n, 0, 0);
			XMStoreFloat4x4(&ConstantBufferData[n].M, identityMatrix);
			XMStoreFloat4x4(&ConstantBufferData[n].V, ViewMatrix);
			XMStoreFloat4x4(&ConstantBufferData[n].P, ProjectionMatrix);
		}

		UINT8* destination = CbvDataBegin + (BoxCount * pWindow->GetCurrentBackBufferIndex() * sizeof(SceneConstantBuffer));
		memcpy(destination, &ConstantBufferData[0], BoxCount * sizeof(SceneConstantBuffer));

		CSRootConstants.deltaTime = deltaTime;
	}
}

void ParticleGame::TransitionResource(ComPtr<ID3D12GraphicsCommandList2> commandList, ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), beforeState, afterState);

	commandList->ResourceBarrier(1, &barrier);
}

void ParticleGame::OnRender(RenderEventArgs& e)
{
	super::OnRender(e);

	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetCommandList();
	auto computeCommandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
	auto computeCommandList = computeCommandQueue->GetCommandList();

	UINT currentBackBufferIndex = pWindow->GetCurrentBackBufferIndex();
	auto backBuffer = pWindow->GetCurrentBackBuffer();
	auto rtv = pWindow->GetCurrentRenderTargetView();
	auto dsv = DSVHeap->GetCPUDescriptorHandleForHeapStart();

	// Compute command list
	if (UseCompute)
	{
		UINT frameDescriptorOffset = currentBackBufferIndex * 3; // 2SRV1UAV descriptor for this frame
		D3D12_GPU_DESCRIPTOR_HANDLE SRV2UAV1Handle = SRV2UAV1Heap->GetGPUDescriptorHandleForHeapStart();

		computeCommandList->SetPipelineState(EmitPSO.Get());
		computeCommandList->SetComputeRootSignature(EmitRS.Get());

		ID3D12DescriptorHeap* ppHeaps[] = { SRV2UAV1Heap.Get() };
		computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		computeCommandList->SetComputeRootDescriptorTable(0, CD3DX12_GPU_DESCRIPTOR_HANDLE(SRV2UAV1Handle, frameDescriptorOffset, SRV2UAV1DescriptorSize));
		computeCommandList->SetComputeRoot32BitConstants(1, 9, reinterpret_cast<void*>(&CSRootConstants), 0);

		// Reset UAV counter for this frame
		computeCommandList->CopyBufferRegion(ProcessedCommandBuffers[currentBackBufferIndex].Get(), CommandBufferCounterOffset, ProcessedCommandBufferCounterReset.Get(), 0, sizeof(UINT));

		TransitionResource(computeCommandList, ProcessedCommandBuffers[currentBackBufferIndex], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		computeCommandList->Dispatch(static_cast<UINT>(ceil(BoxCount / float(ComputeThreadGroupSize))), 1, 1);
	}

	// Rendering command list
	{
		// Set up PSO and root signature
		commandList->SetPipelineState(RenderPSO.Get());
		commandList->SetGraphicsRootSignature(RenderRS.Get());

		// Bind descriptor heaps
		ID3D12DescriptorHeap* ppHeaps[] = { SRV2UAV1Heap.Get() };
		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		// Set up rasterizer state
		commandList->RSSetViewports(1, &Viewport);
		commandList->RSSetScissorRects(1, &ScissorRect);

		// Indicate that the command buffer will be used for indirect drawing
		D3D12_RESOURCE_BARRIER barriers[2] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				UseCompute ? ProcessedCommandBuffers[currentBackBufferIndex].Get() : CommandBuffer.Get(),
				UseCompute ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT),
			CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffer.Get(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET)
		};
		commandList->ResourceBarrier(_countof(barriers), barriers);

		// Bind render targets
		commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

		// Clear targets
		const FLOAT clearColor[] = { 0.3f, 0.2f, 0.1f, 1.0f };
		commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// Set up input assembler
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &VertexBufferView);

		// Draw
		if (UseCompute)
		{
			commandList->ExecuteIndirect(
				CommandSignature.Get(),
				BoxCount,
				ProcessedCommandBuffers[currentBackBufferIndex].Get(),
				0,
				ProcessedCommandBuffers[currentBackBufferIndex].Get(),
				CommandBufferCounterOffset);
		}
		else
		{
			commandList->ExecuteIndirect(
				CommandSignature.Get(),
				BoxCount,
				CommandBuffer.Get(),
				CommandSizePerFrame * currentBackBufferIndex,
				nullptr,
				0);
		}

		// Change resources for presentation and compute
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
		barriers[0].Transition.StateAfter = UseCompute ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

		commandList->ResourceBarrier(_countof(barriers), barriers);
	}

	// Execute work
	{
		if (UseCompute)
		{
			uint64_t computeFence = computeCommandQueue->ExecuteCommandList(computeCommandList);
			commandQueue->Wait(computeCommandQueue->GetD3D12Fence(), computeFence);
		}
		else
		{
			// Manually close list if we don't call CommandQueue->ExecuteCommandList()
			ThrowIfFailed(computeCommandList->Close());
		}

		FenceValues[currentBackBufferIndex] = commandQueue->ExecuteCommandList(commandList);
		currentBackBufferIndex = pWindow->Present();
		commandQueue->WaitForFenceValue(FenceValues[currentBackBufferIndex]);
	}
}

void ParticleGame::OnKeyPressed(KeyEventArgs& e)
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
	case KeyCode::Right:
		drawOffset += deltaTime * 20.0f;
		break;
	case KeyCode::Left:
		drawOffset += -deltaTime * 20.0f;
		break;
	case KeyCode::Space:
		Application::Get().Flush();
		UseCompute = !UseCompute;
		break;
	}
}

void ParticleGame::OnMouseWheel(MouseWheelEventArgs& e)
{
	FoV -= e.WheelDelta;
	FoV = std::clamp(FoV, 12.0f, 90.0f);

	char buffer[256];
	sprintf_s(buffer, "FoV: %f\n", FoV);
	OutputDebugStringA(buffer);
}