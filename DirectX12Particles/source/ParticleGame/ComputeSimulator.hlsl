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

        //particle.velocity.xyz += 15 * -normalize(particle.position).xyz * deltaTime;
        //particle.velocity += particle.acceleration * deltaTime;
        particle.velocity.y = lerp(particle.velocity.y, 0.25f, 0.08f);
        particle.velocity.xz = lerp(particle.velocity.xz, 0.0f, 0.06f);
        particle.position += particle.velocity * deltaTime;
        particle.lifeTimeLeft -= deltaTime;
        particle.scale = lerp(particleEndScale, particleStartScale, particle.lifeTimeLeft / particleLifetime);
        
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
