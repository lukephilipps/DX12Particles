#define threadGroupSize 128

cbuffer RootConstants : register(b0)
{
    float deltaTime;
    float particleLifetime;
    uint emitCount;
    uint maxParticleCount;
    float4 emitAABBMin;
    float4 emitAABBMax;
    float4 emitVelocityMin;
    float4 emitVelocityMax;
    float4 emitAccelerationMin;
    float4 emitAccelerationMax;
    float particleStartScale;
    float particleEndScale;
};

struct Particle
{
    float4 position;
    float4 velocity;
    float4 acceleration;
    float4 color;
    float lifeTimeLeft;
    float scale;
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
        
        float randomValue0 = random(float2(index, particleIndex));
        float randomValue1 = random(float2(particleIndex * deltaTime, index));
        float randomValue2 = random(float2(particleIndex + realEmitCount, deltaTime));
        
        Particle newParticle;
        
        newParticle.position = lerp(emitAABBMin, emitAABBMax, float4(randomValue0, randomValue1, randomValue2, 0));
        newParticle.position.w = 1;
        
        randomValue0 = random(float2(deltaTime, randomValue2));
        randomValue1 = random(float2(randomValue0, randomValue1));
        randomValue2 = random(float2(index * 5, particleIndex));
        
        newParticle.velocity = lerp(emitVelocityMin, emitVelocityMax, float4(randomValue1, randomValue2, randomValue0, 0));
        newParticle.acceleration = lerp(emitAccelerationMin, emitAccelerationMax, float4(randomValue2, randomValue0, randomValue1, 0));
        newParticle.lifeTimeLeft = particleLifetime;
        newParticle.scale = particleStartScale;
        newParticle.color = float4(randomValue0, randomValue1, randomValue2, 1);
        
        Particles[particleIndex] = newParticle;
        AliveIndices0.Append(particleIndex);
    }
}
