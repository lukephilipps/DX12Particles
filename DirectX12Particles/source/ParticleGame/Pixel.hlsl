Texture2D texture : register(t0);
SamplerState pointSampler : register(s0);

struct v2f
{
    float2 uv : TEXCOORD;
};

float4 PSMain(v2f i) : SV_Target
{
    return texture.Sample(pointSampler, i.uv);
}