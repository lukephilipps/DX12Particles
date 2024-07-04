struct v2f
{
    float2 UV : TEXCOORD0;
};

float4 PSMain(v2f i) : SV_Target
{
    if (all(i.UV > .1f) && all(i.UV < .9f))
        discard;
    return float4(1, 0, 0, 1);
}