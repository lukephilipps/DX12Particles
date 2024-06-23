cbuffer SceneConstantBuffer : register(b0)
{
    float4 palceHolder;
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

v2f VSMain(appdata i)
{
    v2f o;
    
    o.Position = mul(P, mul(V, mul(M, float4(i.Position, 1.0f))));
    o.Color = float4(i.Color, 1.0f);
    
    return o;
}