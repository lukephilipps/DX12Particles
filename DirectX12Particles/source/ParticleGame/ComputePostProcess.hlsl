cbuffer RootConstants : register(b0)
{
    int2 WindowDimensions;
};

RWTexture2D<float4> SceneCap : register(u0);
Texture2D DepthBuffer : register(t0);
SamplerState PointSampler : register(s0);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    static const float nearDistance = 0.1f;
    static const float farDistance = 100.0f;
    
    if (id.x < WindowDimensions.x && id.y < WindowDimensions.y)
    {
        // Visualize Depth
        //SceneCap[id.xy] = (2.0f * nearDistance) / (farDistance + nearDistance - DepthBuffer[id.xy].r * (farDistance - nearDistance));
        
        uint width, height;
        DepthBuffer.GetDimensions(width, height);
        
        float2 texCoords = id.xy / float2(width, height);
        
        SceneCap[id.xy] += DepthBuffer.GatherRed(PointSampler, texCoords) / 4;
    }
}