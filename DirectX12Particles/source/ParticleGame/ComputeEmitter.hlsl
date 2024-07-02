#define threadGroupSize 128

cbuffer RootConstants : register(b0)
{
    float deltaTime;
    float particleLifetime;
    uint emitCount;
    uint maxParticleCount;
    float4 emitPosition;
    float4 emitVelocity;
    float4 emitAcceleration;
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
AppendStructuredBuffer<uint> AliveIndices0 : register(u1);
ConsumeStructuredBuffer<uint> DeadIndices : register(u3);
Buffer<uint> DeadIndicesCounter : register(t0);

float random(float2 p)
{
    float2 K1 = float2(
        23.14069263277926, // e^pi (Gelfond's constant)
         2.665144142690225 // 2^sqrt(2) (Gelfond–Schneider constant)
    );
    return frac(cos(dot(p, K1)) * 12345.6789);
}

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
        newParticle.acceleration = emitAcceleration;
        newParticle.lifeTimeLeft = particleLifetime;
        
        float randomValue0 = random(float2(index, particleIndex));
        float randomValue1 = random(float2(particleIndex + realEmitCount, index));
        
        newParticle.velocity.x += lerp(-3, 3, randomValue0);
        newParticle.velocity.z += lerp(-3, 3, randomValue1);
        
        Particles[particleIndex] = newParticle;
        AliveIndices0.Append(particleIndex);
    }
}
