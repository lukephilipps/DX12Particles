struct ParticleData
{
    float4 position;
    float4 velocity;
    float4 acceleration;
    float4 color;
    float lifeTimeLeft;
    float scale;
};

StructuredBuffer<ParticleData> Particles : register(t0);

cbuffer RootConstants : register(b0)
{
    matrix V;
    matrix P;
};

struct appdata
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD;
};

struct v2f
{
    float2 UV: TEXCOORD0;
    float4 color : TEXCOORD1;
    float4 Position : SV_Position;
};

v2f VSMain(appdata i, uint instanceID : SV_InstanceID)
{
    v2f o;
    ParticleData data = Particles[instanceID];
    
    o.Position = mul(P, mul(V, data.position) + float4(i.Position.x, i.Position.y, 0, 0) * float4(data.scale + abs(data.velocity.x) / 50, data.scale + min(1.3, data.velocity.y / 8), 1, 1));
    o.UV = i.UV;
    o.color = data.color;
    
    if (data.lifeTimeLeft <= 0) o.Position = float4(0, 0, 0, 0);
    
    return o;
}