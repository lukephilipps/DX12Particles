#define threadGroupSize 128

cbuffer RootConstants : register(b0)
{
    float deltaTime;
    float particleLifetime;
    uint emitCount;
    uint maxParticleCount;
    float4 emitPosition;
    float4 emitVelocity;
};

struct Particle
{
    float4 position;
    float4 velocity;
    float lifeTimeLeft;

    float padding[55];
};

RWStructuredBuffer<Particle> Particles : register(u0);
AppendStructuredBuffer<uint> AliveIndices0 : register(u1);
ConsumeStructuredBuffer<uint> DeadIndices : register(u3);
Buffer<uint> DeadIndicesCounter : register(t0);

[numthreads(threadGroupSize, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    uint index = (groupId.x * threadGroupSize) + groupIndex;
    uint realEmitCount = min(DeadIndicesCounter[0], emitCount);
    
    GroupMemoryBarrierWithGroupSync();
    
    if (index < realEmitCount)
    {
        uint particleIndex = DeadIndices.Consume();
        
        Particle newParticle;
        newParticle.position = emitPosition;
        newParticle.velocity = emitVelocity;
        newParticle.lifeTimeLeft = particleLifetime;
        
        Particles[particleIndex] = newParticle;
        AliveIndices0.Append(particleIndex);
    }
}
