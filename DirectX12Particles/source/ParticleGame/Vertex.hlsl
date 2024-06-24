cbuffer SceneConstantBuffer : register(b0)
{
    float4 rotation;
    matrix M;
    matrix V;
    matrix P;
};

struct appdata
{
    float3 Position : POSITION;
    float3 Color : COLOR;
};

struct v2f
{
    float4 Color : COLOR;
    float4 Position : SV_Position;
};

static const int x = 3;

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
    
    // Get model pos, rotate by SceneConstantBuffer.rotation and offset by insanceID
    float4 modelPos = float4(mul(AngleAxis3x3(rotation.x, normalize(float3(0, 1, 1))), i.Position), 1);
    modelPos.x += instanceID * 5;
    
    o.Position = mul(P, mul(V, mul(M, modelPos + float4(0, rotation.y * x, 0, 0))));
    o.Color = float4(i.Color, 1.0f);
    
    return o;
}