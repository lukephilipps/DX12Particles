struct ModelViewProjection
{
    matrix MVP;
};

ConstantBuffer<ModelViewProjection> MVPConstantBuffer : register(b0);

struct app_data
{
    float3 Position : POSITION;
    float3 Color : COLOR;
};

struct v2f
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
};

v2f main(app_data i)
{
    v2f o;
    
    o.Position = mul(MVPConstantBuffer.MVP, float4(i.Position, 1.0f));
    o.Color = float4(i.Color, 1.0f);

    return o;
}