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
    float4 acceleration;
    float lifeTimeLeft;

    float padding[51];
};

RWStructuredBuffer<Particle> Particles : register(u0);
ConsumeStructuredBuffer<uint> AliveIndices0 : register(u1);
AppendStructuredBuffer<uint> AliveIndices1 : register(u2);
AppendStructuredBuffer<uint> DeadIndices : register(u3);
Buffer<uint> DeadIndicesCounter : register(t0);

[numthreads(threadGroupSize, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    uint index = (groupId.x * threadGroupSize) + groupIndex;
    uint aliveParticleCount = maxParticleCount - DeadIndicesCounter[0];
    
    GroupMemoryBarrierWithGroupSync();
    
    if (index < aliveParticleCount)
    {
        uint particleIndex = AliveIndices0.Consume();
        Particle particle = Particles[particleIndex];

        particle.velocity += particle.acceleration * deltaTime;
        particle.position += particle.velocity * deltaTime;
        particle.lifeTimeLeft -= deltaTime;
        
        if (particle.lifeTimeLeft <= 0)
        {
            DeadIndices.Append(particleIndex);
        }
        else
        {
            AliveIndices1.Append(particleIndex);
        }
        
        Particles[particleIndex] = particle;
    }
}
