Texture2D Texture : register(t0);
SamplerState PointSampler : register(s0);

struct v2f
{
    float2 UV : TEXCOORD0;
    float4 color : TEXCOORD1;
};

float4 PSMain(v2f i) : SV_Target
{
    return Texture.Sample(PointSampler, i.UV).a * i.color;;
}