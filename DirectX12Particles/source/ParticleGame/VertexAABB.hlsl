cbuffer RootConstants : register(b0)
{
    matrix V;
    matrix P;
};

cbuffer RootConstants : register(b1)
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

struct appdata
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD;
};

struct v2f
{
    float2 UV : TEXCOORD0;
    float4 Position : SV_Position;
};

v2f VSMain(appdata i, uint instanceID : SV_InstanceID)
{
    v2f o;
    
    float4 center = (emitAABBMax + emitAABBMin) * 0.5f;
    float4 scale = emitAABBMax - emitAABBMin;
    scale.w = 1;
    
    o.Position = mul(P, mul(V, float4(i.Position, 1) * scale + center));
    o.UV = i.UV;
    
    return o;
}