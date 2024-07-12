cbuffer RootConstants : register(b0)
{
    int2 WindowDimensions;
    matrix InvP;
    matrix InvV;
    matrix P;
    uint KernelSize;
    uint NoiseSize;
    float KernelRadius;
};

// Scene tex and depth buffer
RWTexture2D<float4> SceneCap : register(u0);
Texture2D DepthBuffer : register(t0);

// SSAO Vars
StructuredBuffer<float4> SSAOKernel : register(t1);
Texture2D SSAONoise : register(t2);

SamplerState PointSampler : register(s0);

float ReconstructDepth(in float z)
{
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    return (2.0f * nearPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

float3 ReconstructWorldPosition(in float2 uv, in float z)
{
    float x = uv.x * 2.0f - 1.0f;
    float y = (1 - uv.y) * 2.0f - 1.0f;
    //float y = (uv.y) * 2.0f - 1.0f;
    float4 positionP = float4(x, y, z, 1.0f);
    float4 positionV = mul(InvV, mul(InvP, positionP));
    return positionV.xyz / positionV.w;
}

float3 ReconstructViewPosition(in float2 uv, in float z)
{
    float linearDepth = ReconstructDepth(z);
    float4 positionP = float4(uv * 2.0f - 1.0f, linearDepth, 1.0f);
    float4 positionV = mul(InvP, positionP);
    positionV.xyz /= positionV.w;
    return positionV.xyz;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x < WindowDimensions.x && id.y < WindowDimensions.y)
    {
        uint width, height;
        DepthBuffer.GetDimensions(width, height);
        
        float2 uv0 = (id.xy + float2(0.0f, 0.0f)) / float2(width, height);
        float2 uv1 = (id.xy + float2(1.0f, 0.0f)) / float2(width, height);
        float2 uv2 = (id.xy + float2(0.0f, 1.0f)) / float2(width, height);
        
        float d0 = DepthBuffer.Load(id, 0).r;
        float d1 = DepthBuffer.Load(id + uint3(1, 0, 0), 0).r;
        float d2 = DepthBuffer.Load(id + uint3(0, 1, 0), 0).r;
        
        if (d0 == 1.0f)
        {
            SceneCap[id.xy] = float4(0, 0, 0, 1.0f);
            return;
        }
        
        float3 WP0 = ReconstructWorldPosition(uv0, d0);
        
        float3 P0 = ReconstructViewPosition(uv0, d0);
        float3 P1 = ReconstructViewPosition(uv1, d1);
        float3 P2 = ReconstructViewPosition(uv2, d2);
        
        float3 normal = normalize(cross(P2 - P0, P1 - P0));
        float3 randomVector = SSAONoise.SampleLevel(PointSampler, uv0 * float2(width / NoiseSize, height / NoiseSize), 0).xyz;
        float3 tangent = normalize(randomVector - normal * dot(randomVector, normal));
        float3 bitangent = cross(normal, tangent);
        float3x3 tbn = float3x3(tangent, bitangent, normal);
        
        float occlusion = 0.0f;
        for (int i = 0; i < KernelSize; ++i)
        {
            float3 ray = SSAOKernel[i].xyz;
            
            //if (dot(ray, normal) > 0.15f)
            //    continue;
            
            float3 sample = mul(tbn, ray);
            sample = mul(sample, KernelRadius) + P0;

            float4 offset = float4(sample, 1.0f);
            offset = mul(P, offset);
            offset.xy /= offset.w;
            offset.xy = offset.xy * 0.5f + 0.5f;
            //offset.y = 1 - offset.y;
            
            float linearDepth = ReconstructDepth(DepthBuffer.SampleLevel(PointSampler, offset.xy, 0).r);
            //float sampleDepth = DepthBuffer.SampleLevel(PointSampler, offset.xy, 0).r;
            
            //float rangeCheck = abs(d0 - linearDepth) < KernelRadius ? 1.0f : 0.0f;
            occlusion += (linearDepth <= sample.z ? 1.0f : 0.0f);
        }
        
        //SceneCap[id.xy] -= (1.0f - (occlusion / KernelSize)) / 12;
        //SceneCap[id.xy] = 1.0f - (occlusion / KernelSize);
        
        //SceneCap[id.xy] = float4(normal * .5 + .5, 1.0f);
        //SceneCap[id.xy] = float4(normal, 1.0f);
        //SceneCap[id.xy] = float4(tangent, 1.0f);
        //SceneCap[id.xy] = float4(bitangent, 1.0f);
        //SceneCap[id.xy] = float4(randomVector, 1.0f);
        
        //SceneCap[id.xy] = float4(frac(P0), 1.0f);
        //SceneCap[id.xy] = (2.0f * nearPlane) / (farPlane + nearPlane - d0 * (farPlane - nearPlane));
        //SceneCap[id.xy] = float4(P0, 1.0f);
        //SceneCap[id.xy] = float4(WP0, 1.0f);
        SceneCap[id.xy] = float4(frac(WP0), 1.0f);
        //SceneCap[id.xy] = float4(P0.xy, 0.0f, 1.0f);
        //SceneCap[id.xy] = float4(uv0, 0.0f, 1.0f);
    }
}