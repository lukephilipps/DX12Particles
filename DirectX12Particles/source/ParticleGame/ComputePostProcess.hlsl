cbuffer RootConstants : register(b0)
{
    int2 WindowDimensions;
    matrix InvVP;
};

RWTexture2D<float4> SceneCap : register(u0);
Texture2D DepthBuffer : register(t0);
SamplerState PointSampler : register(s0);

float3 reconstructPosition(in float2 uv, in float z, in matrix InvVP)
{
    float x = uv.x * 2.0f - 1.0f;
    float y = (1.0 - uv.y) * 2.0f - 1.0f;
    float4 position_s = float4(x, y, z, 1.0f);
    float4 position_v = mul(InvVP, position_s);
    return position_v.xyz / position_v.w;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x < WindowDimensions.x && id.y < WindowDimensions.y)
    {
        uint width, height;
        DepthBuffer.GetDimensions(width, height);
        
        float2 uv0 = (id.xy + float2(0.5f, 0.5f)) / float2(width, height);
        float2 uv1 = (id.xy + float2(1.5f, 0.5f)) / float2(width, height);
        float2 uv2 = (id.xy + float2(0.5f, 1.5f)) / float2(width, height);
        
        float d0 = DepthBuffer.SampleLevel(PointSampler, uv0, 0).r;
        float d1 = DepthBuffer.SampleLevel(PointSampler, uv1, 0).r;
        float d2 = DepthBuffer.SampleLevel(PointSampler, uv2, 0).r;
        
        float3 P0 = reconstructPosition(uv0, d0, InvVP);
        float3 P1 = reconstructPosition(uv1, d1, InvVP);
        float3 P2 = reconstructPosition(uv2, d2, InvVP);

        float3 normal = normalize(cross(P2 - P0, P1 - P0));

        //SceneCap[id.xy] = float4(-normal, 1.0f);
        SceneCap[id.xy] = float4(-normal * .5 + .5, 1.0f);
    }
}