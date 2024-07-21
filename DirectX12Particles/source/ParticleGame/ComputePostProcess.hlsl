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
    float nearPlane = 2.0f;
    float farPlane = 30.0f;
    return (2.0f * nearPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

float3 ReconstructWorldPosition(in float2 uv, in float z)
{
    float x = uv.x * 2.0f - 1.0f;
    float y = (1 - uv.y) * 2.0f - 1.0f;
    //float y = (uv.y) * 2.0f - 1.0f;
    float4 positionP = float4(x, y, z, 1.0f);
    float4 positionV = mul(InvP, positionP);
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
        
        //SceneCap[id.xy] = 1 - DepthBuffer.SampleLevel(PointSampler, uv0, 0).r;
        //return;
        
        float d0 = DepthBuffer.Load(id, 0).r;
        float d1 = DepthBuffer.Load(id + uint3(1, 0, 0), 0).r;
        float d2 = DepthBuffer.Load(id + uint3(0, 1, 0), 0).r;
        
        if (d0 == 1.0f)
        {
            SceneCap[id.xy] = float4(0, 0, 0, 1.0f);
            return;
        }
        
        float3 WP0 = ReconstructWorldPosition(uv0, d0);
        float3 WP1 = ReconstructWorldPosition(uv1, d1);
        float3 WP2 = ReconstructWorldPosition(uv2, d2);
        
        //float3 VP0 = ReconstructViewPosition(uv0, d0);
        //float3 VP1 = ReconstructViewPosition(uv1, d1);
        //float3 VP2 = ReconstructViewPosition(uv2, d2);
        
        float3 normal = -normalize(cross(WP2 - WP0, WP1 - WP0)); //[-1, 1], world space
        //normal = mul((float3x3) transpose(InvP), normal);
        
        //float3 normal = normalize(cross(VP2 - VP0, VP1 - VP0));
        
        //normal.x = -normal.x;
        //normal.y = -normal.y;
        //normal.z = -normal.z;
        //normal.xy = -normal.xy;
        //normal.yz = -normal.yz;
        //normal.xz = -normal.xz;
        //normal.xyz = -normal.xyz;
        
        float3 noiseVector = SSAONoise.SampleLevel(PointSampler, uv0 * float2(width / NoiseSize, height / NoiseSize), 0).xyz;
        float3 tangent = normalize(noiseVector - normal * dot(noiseVector, normal));
        float3 bitangent = cross(normal, tangent);
        float3x3 tbn = float3x3(tangent, bitangent, normal);
        //tbn = transpose(tbn);
        
        //float4 offset = mul(InvV, float4(WP0, 1.0f));
        //offset.xy /= offset.w;
        //offset.xy = offset.xy * float2(0.5f, -0.5f) + 0.5f;
        //SceneCap[id.xy] = SceneCap.Load(uint3(width * offset.x, height * offset.y, 0));
        //return;
        
        float occlusion = 0.0f;
        for (int i = 0; i < KernelSize; ++i)
        {
            //float3 ray = SSAOKernel[i].xyz;
            //if (dot(ray, normal) < 0.01f)
            //    continue;
            
            float3 kernelSample = mul(SSAOKernel[i].xyz, tbn);
            
            //SceneCap[id.xy] = float4(SSAOKernel[i].xyz * 0.5f + 0.5f, 1);
            //SceneCap[id.xy] = float4(kernelSample * 0.5f + 0.5f, 1);
            //return;
            
            kernelSample = KernelRadius * kernelSample + WP0;
            
            //kernelSample = mul((float3x3)InvV, kernelSample);

            float4 offset = float4(kernelSample, 1.0f);
            
            //offset.z = -offset.z;
            offset = mul(InvV, offset);
            offset.xy /= offset.w;
            offset.xy = offset.xy * float2(0.5f, -0.5f) + 0.5f;
            
            //SceneCap[id.xy] = float4(offset.xy, 0, 0);
            //return;
            
            
            //if (any(offset.xy > 1.0f) || any(offset.xy < 0.0f))
            //    return;
            //SceneCap[id.xy] = float4(offset.xy, 0, 1);
            //return;
            
            //if (kernelSample.x > 1.0f)
            //    return;
            //SceneCap[id.xy] = float4(kernelSample, 1);
            //SceneCap[id.xy] = float4(mul(SSAOKernel[i].xyz * .5 + .5, tbn), 1);
            //if (any(SceneCap[id.xy] < 0.0f))
                //return;
            
            //float depth = ReconstructDepth(DepthBuffer.SampleLevel(PointSampler, offset.xy, 0).r);
            float depth = DepthBuffer.SampleLevel(PointSampler, offset.xy, 0).r;
            //float depth = DepthBuffer.Load(uint3(width * offset.x, height * offset.y, 0), 0).r;
            
            //SceneCap[id.xy] = 1 - d0;
            //return;
            
            //float rangeCheck = abs(d0 - depth) < KernelRadius ? 1.0f : 0.0f;
            //occlusion += (depth <= offset.z ? 1.0f : 0.0f);
            occlusion += depth <= d0 ? 1.0f : 0.0f;
        }
        
        //SceneCap[id.xy] -= (1.0f - (occlusion / KernelSize)) / 12;
        //SceneCap[id.xy] = 1.0f - (occlusion / KernelSize);
        SceneCap[id.xy] = lerp(float4(0.1f, 0.1f, 0.1f, 0.1f), SceneCap[id.xy], 1.0f - (occlusion / KernelSize));
        
        //SceneCap[id.xy] = float4(normal, 1.0f);
        //SceneCap[id.xy] = float4(normal * .5 + .5, 1.0f);
        //SceneCap[id.xy] = float4(tangent, 1.0f);
        //SceneCap[id.xy] = float4(tangent * .5 + .5, 1.0f);
        //SceneCap[id.xy] = float4(bitangent, 1.0f);
        //SceneCap[id.xy] = float4(bitangent * .5 + .5, 1.0f);
        //SceneCap[id.xy] = float4(noiseVector, 1.0f);
        //SceneCap[id.xy] = float4(noiseVector * .5 + .5, 1.0f);
        
        //SceneCap[id.xy] = float4(frac(P0), 1.0f);
        //SceneCap[id.xy] = (2.0f * nearPlane) / (farPlane + nearPlane - d0 * (farPlane - nearPlane));
        //SceneCap[id.xy] = float4(VP0, 1.0f);
        //SceneCap[id.xy] = float4(WP0, 1.0f);
        //SceneCap[id.xy] = float4(WP0 / 5, 1.0f);
        //SceneCap[id.xy] = float4(frac(WP0), 1.0f);
        //SceneCap[id.xy] = float4(VP0.xy, 0.0f, 1.0f);
        //SceneCap[id.xy] = float4(uv0, 0.0f, 1.0f);
        //SceneCap[id.xy] = ReconstructDepth(d0);
    }
}