struct ParticleData
{
    float4 position;
    float4 velocity;
    float4 acceleration;
    float lifeTimeLeft;
    
    float buffer[51];
};

StructuredBuffer<ParticleData> particles : register(t0);

cbuffer Matrices : register(b1)
{
    matrix V;
    matrix P;
    float angle;
};

struct appdata
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD;
};

struct v2f
{
    float2 UV: TEXCOORD;
    float4 Position : SV_Position;
};

float3x3 AngleAxis3x3(float angle, float3 axis)
{
    float c, s;
    sincos(angle, s, c);

    float t = 1 - c;
    float x = axis.x;
    float y = axis.y;
    float z = axis.z;

    return float3x3(
        t * x * x + c, t * x * y - s * z, t * x * z + s * y,
        t * x * y + s * z, t * y * y + c, t * y * z - s * x,
        t * x * z - s * y, t * y * z + s * x, t * z * z + c
    );
}

v2f VSMain(appdata i, uint instanceID : SV_InstanceID)
{
    v2f o;
    ParticleData data = particles[instanceID];
    
    o.Position = mul(P, mul(V, data.position) + float4(i.Position.x, i.Position.y, 0, 0));
    o.UV = i.UV;
    
    if (data.lifeTimeLeft <= 0) o.Position = float4(0, 0, 0, 0);
    
    return o;
}