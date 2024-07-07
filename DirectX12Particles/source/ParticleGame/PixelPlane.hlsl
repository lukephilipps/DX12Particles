Texture2D Texture : register(t0);
SamplerState PointSampler : register(s0);

struct v2f
{
    float2 UV : TEXCOORD0;
    float ColorOverride : TEXCOORD1;
    float4 Color : COLOR;
};

float4 PSMain(v2f i) : SV_Target
{
    return lerp(Texture.Sample(PointSampler, i.UV), i.Color, i.ColorOverride);
}