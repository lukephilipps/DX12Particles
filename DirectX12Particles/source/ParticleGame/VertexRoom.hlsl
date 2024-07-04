struct WallData
{
    float4 position;
    float4 axisOfRotation;
    float4 scale;
    float rotation;
    
    float buffer[51];
};

StructuredBuffer<WallData> Walls : register(t0);

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
    float2 UV : TEXCOORD;
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
    
    WallData data = Walls[instanceID];
    
    o.Position = mul(P, mul(V, float4(mul(AngleAxis3x3(data.rotation, data.axisOfRotation.xyz), i.Position * data.scale.xyz), 0) + data.position));
    o.UV = i.UV;
    
    return o;
}