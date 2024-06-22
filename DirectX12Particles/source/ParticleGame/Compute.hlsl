#define threadBlockSize 128

struct SceneConstantBuffer
{
    float4 velocity;
    float4 offset;
    float4 color;
    float4x4 projection;
    float4 padding[9];
};

struct IndirectCommand
{
    uint2 cbvAddress;
    uint4 drawArguments;
};

cbuffer RootConstants : register(b0)
{
    float test; // For testing passing constants
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
    if (index < commandCount)
    {
        // Project the left and right bounds of the triangle into homogenous space.
        float4 left = float4(-xOffset, 0.0f, zOffset, 1.0f) + cbv[index].offset;
        left = mul(left, cbv[index].projection);
        left /= left.w;

        float4 right = float4(xOffset, 0.0f, zOffset, 1.0f) + cbv[index].offset;
        right = mul(right, cbv[index].projection);
        right /= right.w;

        // Only draw triangles that are within the culling space.
        if (-cullOffset < right.x && left.x < cullOffset)
        {
            outputCommands.Append(inputCommands[index]);
        }
    }
}
