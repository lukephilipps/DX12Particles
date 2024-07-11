cbuffer RootConstants : register(b0)
{
    int2 WindowDimensions;
    matrix InvVP;
    matrix V;
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
        
        if (d0 == 1.0f)
        {
            SceneCap[id.xy] = float4(0, 0, 0, 1.0f);
            return;
        }
        
        float3 P0 = reconstructPosition(uv0, d0, InvVP);
        float3 P1 = reconstructPosition(uv1, d1, InvVP);
        float3 P2 = reconstructPosition(uv2, d2, InvVP);

        float3 normal = mul(V, float4(-normalize(cross(P2 - P0, P1 - P0)), 0.0f));
        float3 randomVector = SSAONoise.SampleLevel(PointSampler, uv0 * float2(width / NoiseSize, height / NoiseSize), 0).xyz;
        randomVector.xy = randomVector.xy * 2.0f - 1.0f;
        float3 tangent = normalize(randomVector - normal * dot(randomVector, normal));
        float3 bitangent = cross(normal, tangent);
        float3x3 tbn = float3x3(tangent, bitangent, normal);
        
        P0 = mul(V, float4(P0, 1));
        
        float occlusion = 0.0f;
        for (int i = 0; i < KernelSize; ++i)
        {
            float3 ray = SSAOKernel[i].xyz;
            
            if (dot(ray, normal) > 0.15f)
                continue;
            
            float3 sample = mul(tbn, ray);
            sample = mul(sample, KernelRadius) + P0;

            float4 offset = float4(sample, 1.0f);
            offset = mul(P, offset);
            offset.xyz /= offset.w;
            offset.xy = offset.xy * 0.5f + 0.5f;
            offset.y = 1 - offset.y;
            
            float sampleDepth = DepthBuffer.SampleLevel(PointSampler, offset.xy, 0).r;
            
            float rangeCheck = abs(d0 - sampleDepth) < KernelRadius ? 1.0f : 0.0f;
            occlusion += (sampleDepth <= d0 ? 1.0f : 0.0f) * rangeCheck;
        }
        
        SceneCap[id.xy] -= (1.0f - (occlusion / KernelSize)) / 12;
        //SceneCap[id.xy] = 1.0f - (occlusion / KernelSize);
        
        //SceneCap[id.xy] = float4(normal * .5 + .5, 1.0f);
        //SceneCap[id.xy] = float4(normal, 1.0f);
        //SceneCap[id.xy] = float4(randomVector, 1.0f);
    }
}