struct v2f
{
    float4 Color : COLOR;
};

float4 PSMain(v2f i) : SV_Target
{
    return i.Color;

}