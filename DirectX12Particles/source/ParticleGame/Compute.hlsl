#define threadGroupSize 128

struct SceneConstantBuffer
{
    float4 rotation;
    matrix M;
    matrix V;
    matrix P;
    
    float4 padding[3];
};

struct IndirectCommand
{
    uint2 cbvAddress;
    uint4 ibv;
    uint indexCountPerInstance;
    uint instanceCount;
    uint startIndexLocation;
    int baseVertexLocation;
    uint startInstanceLocation;
};

cbuffer RootConstants : register(b0)
{
    float x;
    float commandCount;
};

StructuredBuffer<SceneConstantBuffer> cbv : register(t0); // SRV: Wrapped constant buffers
StructuredBuffer<IndirectCommand> inputCommands : register(t1); // SRV: Indirect commands
AppendStructuredBuffer<IndirectCommand> outputCommands : register(u0); // UAV: Processed indirect commands

[numthreads(threadGroupSize, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    // Each thread of the CS operates on one of the indirect commands.
    uint index = (groupId.x * threadGroupSize) + groupIndex;

    // Don't attempt to access commands that don't exist if more threads are allocated
    // than commands.
    if (index < 2 && cbv[index].rotation.x < 100000000.0f)
    {
        IndirectCommand command = inputCommands[index];
        command.instanceCount = 10;
        outputCommands.Append(command);
    }
}
