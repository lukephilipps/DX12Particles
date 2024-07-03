cbuffer RootConstants : register(b0)
{
    int2 WindowDimensions;
};

RWTexture2D<float4> SceneCap : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x < WindowDimensions.x && id.y < WindowDimensions.y)
    {
        SceneCap[id.xy].r = 1;
    }
}