Texture2D Texture : register(t0);
SamplerState PointSampler : register(s0);

struct v2f
{
    float2 UV : TEXCOORD;
};

float4 PSMain(v2f i) : SV_Target
{
    return Texture.Sample(PointSampler, i.UV);
}