struct ModelViewProjection
{
    matrix M;
    matrix V;
    matrix P;
};

ConstantBuffer<ModelViewProjection> MVPConstantBuffer : register(b0);

struct app_data
{
    float3 Position : POSITION;
    float3 Color : COLOR;
};

struct v2f
{
    float4 Color : COLOR;
    float4 Position : SV_Position;
};

v2f VSMain(app_data i, uint instanceID : SV_InstanceID)
{
    v2f o;
    
    o.Position = mul(MVPConstantBuffer.P, mul(MVPConstantBuffer.V, mul(MVPConstantBuffer.M, float4(i.Position, 1.0f)) + float4(instanceID * 5, 0, 0, 0)));
    o.Color = float4(i.Color, 1.0f);

    return o;
}