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
	{ XMFLOAT3(-0.15f, -0.15f, -0.15f), XMFLOAT3(0.0f, 0.0f, 0.0f) },
	{ XMFLOAT3(-0.15f,  0.15f, -0.15f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
	{ XMFLOAT3( 0.15f,  0.15f, -0.15f), XMFLOAT3(1.0f, 1.0f, 0.0f) },
	{ XMFLOAT3( 0.15f, -0.15f, -0.15f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
	{ XMFLOAT3(-0.15f, -0.15f,  0.15f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
	{ XMFLOAT3(-0.15f,  0.15f,  0.15f), XMFLOAT3(0.0f, 1.0f, 1.0f) },
	{ XMFLOAT3( 0.15f,  0.15f,  0.15f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
	{ XMFLOAT3( 0.15f, -0.15f,  0.15f), XMFLOAT3(1.0f, 0.0f, 1.0f) }
};

static WORD Indices[36] = {
	0, 1, 2, 0, 2, 3,
	4, 6, 5, 4, 7, 6,
	4, 5, 1, 4, 1, 0,
	3, 2, 6, 3, 6, 7,
	1, 5, 6, 1, 6, 2,
	4, 0, 3, 4, 3, 7
};

const UINT ParticleGame::ParticleBufferCounterOffset = (sizeof(UINT) * MaxParticleCount + (D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT - 1);

ParticleGame::ParticleGame(const std::wstring& name, int width, int height, bool vSync)
	: super(name, width, height, vSync)
	, ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	, Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)))
	, FoV(45.0)
	, ContentLoaded(false)
	, drawOffset(0)
	, UseCompute(false)
	, deltaTime(0)
	, PressingW(false)
	, PressingA(false)
	, PressingS(false)
	, PressingD(false)
	, PressingQ(false)
	, PressingE(false)
{
	CSRootConstants.particleLifetime = 5;
	CSRootConstants.emitCount = 3;
	CSRootConstants.maxParticleCount = MaxParticleCount;
	CSRootConstants.emitAABBMin = XMFLOAT4(-10, 0, 0, 0);
	CSRootConstants.emitAABBMax = XMFLOAT4(10, 10, 20, 0);
	CSRootConstants.emitVelocityMin = XMFLOAT4(-1, -1, -1, 0);
	CSRootConstants.emitVelocityMax = XMFLOAT4(1, 1, 1, 0);
	CSRootConstants.emitAccelerationMin = XMFLOAT4(0, 0, 0, 0);
	CSRootConstants.emitAccelerationMax = XMFLOAT4(0, 0, 0, 0);

	CameraPosition = XMFLOAT4(0, 0, -10, 1);
}

void ParticleGame::UpdateBufferResource(ComPtr<ID3D12GraphicsCommandList2> commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource,
	size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags)
{
	auto device = Application::Get().GetDevice();

	size_t bufferSize = numElements * elementSize;

	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);

	// Create a committed resource in GPU mem
	CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(pDestinationResource)));

	// Create a committed resource for the upload
	if (bufferData)
	{
		bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		ThrowIfFailed(device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
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

void ParticleGame::UpdateBufferResourceWithCounter(ComPtr<ID3D12GraphicsCommandList2> commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource,
	size_t bufferSize, const void* bufferData)
{
	auto device = Application::Get().GetDevice();

	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	// Create a committed resource in GPU mem
	CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(pDestinationResource)));

	// Create a committed resource for the upload
	if (bufferData)
	{
		bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		ThrowIfFailed(device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
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
		DescriptorHeap = Application::Get().CreateDescriptorHeap(5 + Window::BufferCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
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
		{
			CD3DX12_DESCRIPTOR_RANGE1 renderRanges[1];
			renderRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

			CD3DX12_ROOT_PARAMETER1 rootParameters[2];
			rootParameters[0].InitAsDescriptorTable(_countof(renderRanges), renderRanges);
			rootParameters[1].InitAsConstants(sizeof(VSRootConstants) / 4, 1, 0, D3D12_SHADER_VISIBILITY_VERTEX);

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC RSDescription;
			RSDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&RSDescription, featureData.HighestVersion, &RSBlob, &errorBlob));
			ThrowIfFailed(device->CreateRootSignature(0, RSBlob->GetBufferPointer(), RSBlob->GetBufferSize(), IID_PPV_ARGS(&RenderRS)));
		}

		// Create emitter compute signature
		{
			CD3DX12_DESCRIPTOR_RANGE1 emitRanges[2];
			emitRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
			emitRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

			CD3DX12_ROOT_PARAMETER1 emitRootParameters[2];
			emitRootParameters[0].InitAsDescriptorTable(_countof(emitRanges), emitRanges);
			emitRootParameters[1].InitAsConstants(sizeof(CSRootConstants) / 4, 0);

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC emitRSDescription;
			emitRSDescription.Init_1_1(_countof(emitRootParameters), emitRootParameters);

			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&emitRSDescription, featureData.HighestVersion, &RSBlob, &errorBlob));
			ThrowIfFailed(device->CreateRootSignature(0, RSBlob->GetBufferPointer(), RSBlob->GetBufferSize(), IID_PPV_ARGS(&EmitRS)));
		}

		// Create simulation compute signature
		{
			CD3DX12_DESCRIPTOR_RANGE1 simulateRanges[2];
			simulateRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
			simulateRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

			CD3DX12_ROOT_PARAMETER1 simulateRootParameters[2];
			simulateRootParameters[0].InitAsDescriptorTable(_countof(simulateRanges), simulateRanges);
			simulateRootParameters[1].InitAsConstants(sizeof(CSRootConstants) / 4, 0);

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC simulateRSDescription;
			simulateRSDescription.Init_1_1(_countof(simulateRootParameters), simulateRootParameters);

			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&simulateRSDescription, featureData.HighestVersion, &RSBlob, &errorBlob));
			ThrowIfFailed(device->CreateRootSignature(0, RSBlob->GetBufferPointer(), RSBlob->GetBufferSize(), IID_PPV_ARGS(&SimulateRS)));
		}
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
		ThrowIfFailed(D3DReadFileToBlob((assetPathString + L"ComputeEmitter.cso").c_str(), &computeEmitShader));
		ThrowIfFailed(D3DReadFileToBlob((assetPathString + L"ComputeSimulator.cso").c_str(), &computeSimulateShader));

		// Define rendering PSO
		{
			D3D12_INPUT_ELEMENT_DESC inputLayout[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

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
		}

		// Define emit PSO
		{
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
		}

		// Define simulate PSO
		{
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
	}

	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetCommandList();
	ComPtr<ID3D12Resource> intermediateVertexBuffer;
	ComPtr<ID3D12Resource> intermediateIndexBuffer;

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

	DescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	ComPtr<ID3D12Resource> intermediateParticleBuffer;
	ComPtr<ID3D12Resource> intermediateAlive0Buffer;
	ComPtr<ID3D12Resource> intermediateAlive1Buffer;
	ComPtr<ID3D12Resource> intermediateDeadBuffer;
	ComPtr<ID3D12Resource> intermediateDeadBufferCounter;
	ComPtr<ID3D12Resource> intermediateStagedParticleBuffer;

	// Define descriptor heap
	{
		Particle particleData[MaxParticleCount];
		Particle stagedParticleData[MaxParticleCount * Window::BufferCount];
		UINT particleAliveIndices[MaxParticleCount + 1];
		UINT particleDeadIndices[MaxParticleCount];
		UINT deadCounter[1] = { MaxParticleCount };

		for (UINT n = 0; n < MaxParticleCount; n++)
		{
			particleData[n] = {};
			particleDeadIndices[n] = n;
		}

		UINT index = 0;
		for (UINT frame = 0; frame < Window::BufferCount; frame++)
		{
			for (UINT n = 0; n < MaxParticleCount; n++)
			{
				stagedParticleData[index] = {};
				++index;
			}
		}

		particleAliveIndices[MaxParticleCount] = 0;

		CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandle(DescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 0, DescriptorSize);

		// Entry 0, Particle buffer for compute shaders
		UpdateBufferResource(commandList.Get(), &ParticleBuffer, &intermediateParticleBuffer, _countof(particleData), sizeof(Particle), particleData, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = MaxParticleCount;
		uavDesc.Buffer.StructureByteStride = sizeof(Particle);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		device->CreateUnorderedAccessView(ParticleBuffer.Get(), nullptr, &uavDesc, descriptorHandle);

		// Entry 1, Alive particle index buffer 0
		descriptorHandle.Offset(1, DescriptorSize);
		UpdateBufferResourceWithCounter(commandList.Get(), &AliveIndexList0, &intermediateAlive0Buffer, ParticleBufferCounterOffset, particleAliveIndices);
		uavDesc.Buffer.StructureByteStride = sizeof(UINT);
		uavDesc.Buffer.CounterOffsetInBytes = ParticleBufferCounterOffset;
		device->CreateUnorderedAccessView(AliveIndexList0.Get(), AliveIndexList0.Get(), &uavDesc, descriptorHandle);

		// Entry 2, Alive particle index buffer 1
		descriptorHandle.Offset(1, DescriptorSize);
		UpdateBufferResourceWithCounter(commandList.Get(), &AliveIndexList1, &intermediateAlive1Buffer, ParticleBufferCounterOffset, particleAliveIndices);
		device->CreateUnorderedAccessView(AliveIndexList1.Get(), AliveIndexList1.Get(), &uavDesc, descriptorHandle);

		// Create counter resource for DeadIndexList as we want to place the counter on the heap to read from compute shaders as an SRV
		UpdateBufferResource(commandList.Get(), &DeadIndexListCounter, &intermediateDeadBufferCounter, 1, sizeof(UINT), deadCounter, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		// Entry 3, Dead particle index buffer
		descriptorHandle.Offset(1, DescriptorSize);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		UpdateBufferResource(commandList.Get(), &DeadIndexList, &intermediateDeadBuffer, MaxParticleCount, sizeof(UINT), particleDeadIndices, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		device->CreateUnorderedAccessView(DeadIndexList.Get(), DeadIndexListCounter.Get(), &uavDesc, descriptorHandle);
		
		// Entry 4, Dead particle index buffer counter
		descriptorHandle.Offset(1, DescriptorSize);
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.NumElements = 1;
		srvDesc.Buffer.StructureByteStride = sizeof(UINT);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		device->CreateShaderResourceView(DeadIndexListCounter.Get(), &srvDesc, descriptorHandle);

		// Entries 5-7, Staged particle data for rendering
		descriptorHandle.Offset(1, DescriptorSize);
		UpdateBufferResource(commandList.Get(), &StagedParticleBuffers, &intermediateStagedParticleBuffer, _countof(stagedParticleData), sizeof(Particle), stagedParticleData);
		srvDesc.Buffer.NumElements = MaxParticleCount;
		srvDesc.Buffer.StructureByteStride = sizeof(Particle);
		for (UINT frame = 0; frame < Window::BufferCount; frame++)
		{
			srvDesc.Buffer.FirstElement = frame * MaxParticleCount;
			device->CreateShaderResourceView(StagedParticleBuffers.Get(), &srvDesc, descriptorHandle);
			descriptorHandle.Offset(1, DescriptorSize);
		}

		// UAV counter reset (For AliveBuffer1)
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT));
		CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		ThrowIfFailed(device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&UAVCounterReset)));

		UINT8* pMappedCounterReset = nullptr;
		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(UAVCounterReset->Map(0, &readRange, reinterpret_cast<void**>(&pMappedCounterReset)));
		ZeroMemory(pMappedCounterReset, sizeof(UINT));
		UAVCounterReset->Unmap(0, nullptr);
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
	
	// Update constant info
	{
		CameraPosition.x += ((float)PressingD - (float)PressingA) * deltaTime * CameraMoveSpeed;
		CameraPosition.z += ((float)PressingW - (float)PressingS) * deltaTime * CameraMoveSpeed;
		CameraPosition.y += ((float)PressingE - (float)PressingQ) * deltaTime * CameraMoveSpeed;

		const XMVECTOR eyePos = XMLoadFloat4(&CameraPosition);
		const XMVECTOR focusPoint = XMVectorSet(0, 0, 0, 1);
		const XMVECTOR upDirection = XMVectorSet(0, 1, 0, 1);
		XMMATRIX viewMatrix = DirectX::XMMatrixLookAtLH(eyePos, focusPoint, upDirection);

		float aspectRatio = GetWindowWidth() / static_cast<float>(GetWindowHeight());
		XMMATRIX projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(XMConvertToRadians(FoV), aspectRatio, 0.1f, 100.0f);

		float angle = XMConvertToRadians(static_cast<float>(e.TotalTime * 90));
		VSRootConstants.MVP = DirectX::XMMatrixMultiply(viewMatrix, projectionMatrix);
		VSRootConstants.InvView = DirectX::XMMatrixInverse(nullptr, viewMatrix);
		VSRootConstants.angle = angle;

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

	D3D12_GPU_DESCRIPTOR_HANDLE descriptorHandle = DescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	// Compute command list
	if (UseCompute)
	{
		// Emit
		computeCommandList->SetPipelineState(EmitPSO.Get());
		computeCommandList->SetComputeRootSignature(EmitRS.Get());

		ID3D12DescriptorHeap* ppHeaps[] = { DescriptorHeap.Get() };
		computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		computeCommandList->SetComputeRootDescriptorTable(0, CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHandle, 0, DescriptorSize));
		computeCommandList->SetComputeRoot32BitConstants(1, sizeof(CSRootConstants) / 4, reinterpret_cast<void*>(&CSRootConstants), 0);

		computeCommandList->Dispatch(static_cast<UINT>(ceil((float)CSRootConstants.emitCount / float(ComputeThreadGroupSize))), 1, 1);

		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
		computeCommandList->ResourceBarrier(1, &barrier);

		// Simulate
		computeCommandList->SetPipelineState(SimulatePSO.Get());
		computeCommandList->SetComputeRootSignature(SimulateRS.Get());

		computeCommandList->Dispatch(static_cast<UINT>(ceil(MaxParticleCount / float(ComputeThreadGroupSize))), 1, 1);

		computeCommandList->CopyBufferRegion(StagedParticleBuffers.Get(), currentBackBufferIndex * sizeof(Particle) * MaxParticleCount, ParticleBuffer.Get(), 0, sizeof(Particle) * MaxParticleCount);
		computeCommandList->CopyResource(AliveIndexList0.Get(), AliveIndexList1.Get());
		
		TransitionResource(computeCommandList, AliveIndexList1, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		computeCommandList->CopyBufferRegion(AliveIndexList1.Get(), ParticleBufferCounterOffset, UAVCounterReset.Get(), 0, sizeof(UINT));
		TransitionResource(computeCommandList, AliveIndexList1, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
	}

	// Rendering command list
	{
		commandList->SetPipelineState(RenderPSO.Get());
		commandList->SetGraphicsRootSignature(RenderRS.Get());

		ID3D12DescriptorHeap* ppHeaps[] = { DescriptorHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		commandList->RSSetViewports(1, &Viewport);
		commandList->RSSetScissorRects(1, &ScissorRect);

		TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

		const FLOAT clearColor[] = { 0.8f, 0.2f, 0.1f, 1.0f };
		commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &VertexBufferView);
		commandList->IASetIndexBuffer(&IndexBufferView);

		commandList->SetGraphicsRootDescriptorTable(0, CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHandle, 5 + currentBackBufferIndex, DescriptorSize));
		commandList->SetGraphicsRoot32BitConstants(1, sizeof(VSRootConstants) / 4, reinterpret_cast<void*>(&VSRootConstants), 0);

		commandList->DrawIndexedInstanced(_countof(Indices), MaxParticleCount, 0, 0, 0);

		TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
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
		char buffer[512];
		sprintf_s(buffer, "Using Compute?: %d\n", UseCompute);
		OutputDebugStringA(buffer);
		break;
	case KeyCode::W:
		PressingW = true;
		break;
	case KeyCode::A:
		PressingA = true;
		break;
	case KeyCode::S:
		PressingS = true;
		break;
	case KeyCode::D:
		PressingD = true;
		break;
	case KeyCode::Q:
		PressingQ = true;
		break;
	case KeyCode::E:
		PressingE = true;
		break;
	}
}

void ParticleGame::OnKeyReleased(KeyEventArgs& e)
{
	super::OnKeyReleased(e);

	switch (e.Key)
	{
	case KeyCode::W:
		PressingW = false;
		break;
	case KeyCode::A:
		PressingA = false;
		break;
	case KeyCode::S:
		PressingS = false;
		break;
	case KeyCode::D:
		PressingD = false;
		break;
	case KeyCode::Q:
		PressingQ = false;
		break;
	case KeyCode::E:
		PressingE = false;
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