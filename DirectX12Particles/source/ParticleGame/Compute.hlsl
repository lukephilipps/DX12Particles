#define threadBlockSize 128

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
    uint4 drawArguments;
};

cbuffer RootConstants : register(b0)
{
    float x;
    float commandCount;
};

StructuredBuffer<SceneConstantBuffer> cbv : register(t0); // SRV: Wrapped constant buffers
StructuredBuffer<IndirectCommand> inputCommands : register(t1); // SRV: Indirect commands
AppendStructuredBuffer<IndirectCommand> outputCommands : register(u0); // UAV: Processed indirect commands

[numthreads(threadBlockSize, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    // Each thread of the CS operates on one of the indirect commands.
    uint index = (groupId.x * threadBlockSize) + groupIndex;

    // Don't attempt to access commands that don't exist if more threads are allocated
    // than commands.
    if (index < commandCount && cbv[index].rotation.x < 100000000.0f)
    {
        outputCommands.Append(inputCommands[index]);
    }
}
