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
    static const float nearDistance = 0.1f;
    static const float farDistance = 100.0f;
    
    if (id.x < WindowDimensions.x && id.y < WindowDimensions.y)
    {
        // Visualize Depth
        //SceneCap[id.xy] = (2.0f * nearDistance) / (farDistance + nearDistance - DepthBuffer[id.xy].r * (farDistance - nearDistance));
        
        //SceneCap[id.xy] -= abs(dot(DepthBuffer.GatherRed(PointSampler, texCoords), 0.25f) - DepthBuffer[id.xy].r) * 10000;
        //SceneCap[id.xy] = DepthBuffer[id.xy].r;
        
        //float4 neighbors = DepthBuffer.GatherRed(PointSampler, texCoords) * 50000;
        
        //float dx = (neighbors.x - neighbors.z) / 2;
        //float dy = (neighbors.y - neighbors.w) / 2;
        
        //SceneCap[id.xy] = float4(normalize(float3(dx, dy, 1.0f)), 1.0f);
        
        uint width, height;
        DepthBuffer.GetDimensions(width, height);
        
        float2 uv0 = id.xy / float2(width, height);
        float2 uv1 = (id.xy + int2(1, 0)) / float2(width, height);
        float2 uv2 = (id.xy + int2(0, 1)) / float2(width, height);
        
        float d0 = DepthBuffer.SampleLevel(PointSampler, uv0, 0).r;
        float d1 = DepthBuffer.SampleLevel(PointSampler, uv1, 0).r;
        float d2 = DepthBuffer.SampleLevel(PointSampler, uv2, 0).r;
        
        float3 P0 = reconstructPosition(uv0, d0, InvVP);
        float3 P1 = reconstructPosition(uv1, d1, InvVP);
        float3 P2 = reconstructPosition(uv2, d2, InvVP);

        float3 normal = normalize(cross(P2 - P0, P1 - P0));

        SceneCap[id.xy] = float4(normal, 1.0f);
    }
}