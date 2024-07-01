#define threadGroupSize 128

cbuffer RootConstants : register(b0)
{
    float deltaTime;
    float particleLifetime;
    uint emitCount;
    uint maxParticleCount;
    float3 emitPosition;
    float3 emitVelocity;
};

struct Particle
{
    float3 position;
    float3 velocity;
    float age;

    float padding[57];
};

RWStructuredBuffer<Particle> Particles : register(u0);
RWBuffer<uint> AliveIndices0 : register(u1);
RWBuffer<uint> AliveIndices1 : register(u2);
RWBuffer<uint> DeadIndices : register(u3);

[numthreads(threadGroupSize, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    uint index = (groupId.x * threadGroupSize) + groupIndex;
    
    if (index < maxParticleCount)
    {
        Particles[index].position.y += deltaTime * 1;
    }
}
